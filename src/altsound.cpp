#define NOMINMAX

#include "altsound.h"

#include "altsound_data.hpp"
#include "altsound_ini_processor.hpp"
#include "altsound_processor_base.hpp"
#include "altsound_processor.hpp"
#include "gsound_processor.hpp"
#include "miniaudio_bass_compat.hpp"
#include "miniaudio_private.h"

#include <thread>
#include <chrono>
#include <atomic>
#include <condition_variable>
#include <unordered_map>

StreamArray channel_stream;
std::mutex io_mutex;
BehaviorInfo music_behavior;
BehaviorInfo callout_behavior;
BehaviorInfo sfx_behavior;
BehaviorInfo solo_behavior;
BehaviorInfo overlay_behavior;
AltsoundLogger alog;

AltsoundProcessorBase* g_pProcessor = NULL;
ALTSOUND_HARDWARE_GEN g_hardwareGen = ALTSOUND_HARDWARE_GEN_NONE;
CmdData g_cmdData;
static bool g_newSoundCommands = false; // "new_sound_commands" of the pack's altsound.ini, see altsound_preprocess_commands

std::unordered_map<unsigned int, _internal_stream_data> g_streamMap;
std::mutex g_streamMapMutex;

static AltSoundAudioCallback g_audioCallback = nullptr;
static void* g_audioUserData = nullptr;
static std::mutex g_audioMutex;
uint32_t g_sampleRate = 44100;
uint32_t g_channels = 2;
uint32_t g_nextStreamId = 1;
int g_last_ma_err = 0;
ma_engine* g_engine = nullptr;
ma_context* g_context = nullptr;

static uint32_t g_bufferSizeFrames = 256;

/******************************************************
 * Audio mixing
 *
 * miniAudio owns the audio thread (a realtime-paced null device). Its engine
 * mixes every playing ma_sound (volume, channel conversion and resampling
 * included) and hands us the finished buffer through onProcess, which we just
 * forward to the host. miniAudio handles all timing, throttling and buffering.
 ******************************************************/

static void AltsoundEngineProcess(void* pUserData, float* pFramesOut, ma_uint64 frameCount)
{
    // Streams that just reached their end were queued by the miniAudio end
    // callback. Fire their SYNCPROCs here (safe point, after the read), which
    // frees the sounds and adjusts ducking.
    std::vector<EndedStream> ended;
    {
        std::lock_guard<std::mutex> lock(g_endedMutex);
        ended.swap(g_endedStreams);
    }

    for (const auto& e : ended)
        e.callback(e.hsync, e.hstream, 0, e.userdata);

    std::lock_guard<std::mutex> lock(g_audioMutex);
    if (g_audioCallback)
        g_audioCallback(pFramesOut, static_cast<size_t>(frameCount), g_sampleRate, g_channels, g_audioUserData);
}

/******************************************************
 * altsound_preprocess_commands
 ******************************************************/

void altsound_preprocess_commands(int cmd)
{
	ALT_DEBUG(0, "BEGIN altsound_preprocess_commands()");
	ALT_INDENT;

	ALT_DEBUG(0, "MAME_GEN: 0x%013llx", (unsigned long long)g_hardwareGen);

	switch (g_hardwareGen) {
		case ALTSOUND_HARDWARE_GEN_WPCDCS:
		case ALTSOUND_HARDWARE_GEN_WPCSECURITY:
		case ALTSOUND_HARDWARE_GEN_WPC95DCS:
		case ALTSOUND_HARDWARE_GEN_WPC95: {
			ALT_DEBUG(0, "Hardware Generation: WPCDCS, WPCSECURITY, WPC95DCS, WPC95");

			// For future improvements, also check https://github.com/mjrgh/DCSExplorer/ for a lot of new info on the DCS inner workings

			// E.g.: One more note on command processing: each byte of a command sequence must be received on the DCS side within 100ms of the previous byte.
			//   The DCS software clears any buffered bytes if more than 100ms elapses between consecutive bytes.
			//   This implies that a sender can wait a little longer than 100ms before sending the first byte of a new command if it wants to essentially reset the network connection,
			//   ensuring that the DCS receiver doesn't think it's in the middle of some earlier partially-sent command sequence.

			ALT_DEBUG(0, "Hardware Generation: GEN_WPCDCS, GEN_WPCSECURITY, GEN_WPC95DCS, GEN_WPC95");

			if ((g_cmdData.cmd_buffer[3] == 0x55) && (g_cmdData.cmd_buffer[2] >= 0xAB) && (g_cmdData.cmd_buffer[2] <= 0xB0) && (g_cmdData.cmd_buffer[1] == (g_cmdData.cmd_buffer[0] ^ 0xFF))) // per-DCS-channel mixing level, but on our interpretation level we do not have any knowledge about the internal channel structures of DCS
			{
				ALT_DEBUG(0, "Change volume pc %u %u", g_cmdData.cmd_buffer[2], g_cmdData.cmd_buffer[1]);

				for (int i = 0; i < ALT_MAX_CMDS; ++i)
					g_cmdData.cmd_buffer[i] = ~0;

				g_cmdData.cmd_counter = 0;
				g_cmdData.cmd_filter = 1;
			}
			else
			if ((g_cmdData.cmd_buffer[3] == 0x55) && (g_cmdData.cmd_buffer[2] == 0xC2)) // DCS software major version number
			{
				for (int i = 0; i < ALT_MAX_CMDS; ++i)
					g_cmdData.cmd_buffer[i] = ~0;

				g_cmdData.cmd_counter = 0;
				g_cmdData.cmd_filter = 1;
			}
			else
			if ((g_cmdData.cmd_buffer[3] == 0x55) && (g_cmdData.cmd_buffer[2] == 0xC3)) // DCS software minor version number
			{
				for (int i = 0; i < ALT_MAX_CMDS; ++i)
					g_cmdData.cmd_buffer[i] = ~0;

				g_cmdData.cmd_counter = 0;
				g_cmdData.cmd_filter = 1;
			}
			else
			if ((g_cmdData.cmd_buffer[3] == 0x55) && (g_cmdData.cmd_buffer[2] >= 0xBA) && (g_cmdData.cmd_buffer[2] <= 0xC1) && (g_cmdData.cmd_buffer[1] == (g_cmdData.cmd_buffer[0] ^ 0xFF))) // mystery command, see http://mjrnet.org/pinscape/dcsref/DCS_format_reference.html#SpecialCommands
			{
				for (int i = 0; i < ALT_MAX_CMDS; ++i)
					g_cmdData.cmd_buffer[i] = ~0;

				g_cmdData.cmd_counter = 0;
				g_cmdData.cmd_filter = 1;
			}
			else
			if (((g_cmdData.cmd_buffer[3] == 0x55) && (g_cmdData.cmd_buffer[2] == 0xAA))) { // change master volume?
				// DAR@20240208 The check below is dangerous.  If this is still a
				//              problem, it would be better to revisit it when it
				//              reappears to implement a more robust solution that
				//              works for all systems
				//              See https://github.com/vpinball/pinmame/issues/220
				// Maybe implementing the 'nothing happened in >100ms' reset queue (see above) would also resolve this??
				//|| ((g_cmdData.cmd_buffer[2] == 0x00) && (g_cmdData.cmd_buffer[1] == 0x00) && (g_cmdData.cmd_buffer[0] == 0x00))) { // glitch in command buffer?
				if ((g_cmdData.cmd_buffer[3] == 0x55) && (g_cmdData.cmd_buffer[2] == 0xAA) && (g_cmdData.cmd_buffer[1] == (g_cmdData.cmd_buffer[0] ^ 0xFF))) { // change volume op (following first byte = volume, second = ~volume, if these don't match: ignore)
					if (g_pProcessor->romControlsVol()) {
						//g_pProcessor->setGlobalVol(std::min((float)g_cmdData.cmd_buffer[1] / 127.f, 1.0f)); //!! input is 0..255 (or ..248 in practice? BUT at least MM triggers 255 at max volume in the menu) though, not just 0..127!
						g_pProcessor->setGlobalVol((g_cmdData.cmd_buffer[1] == 0) ? 0.f : std::min(powf(0.981201f, (float)(255u - g_cmdData.cmd_buffer[1])) * 4.0f,1.0f)); //!! *4 is magic, similar to the *2 above
						ALT_INFO(0, "Change volume %.02f (%u)", g_pProcessor->getGlobalVol(), g_cmdData.cmd_buffer[1]);
					}
				}
				else
					ALT_DEBUG(0, "Command filtered %02X %02X %02X %02X", g_cmdData.cmd_buffer[3], g_cmdData.cmd_buffer[2], g_cmdData.cmd_buffer[1], g_cmdData.cmd_buffer[0]);

				for (int i = 0; i < ALT_MAX_CMDS; ++i)
					g_cmdData.cmd_buffer[i] = ~0;

				g_cmdData.cmd_counter = 0;
				g_cmdData.cmd_filter = 1;
			}
			else
				g_cmdData.cmd_filter = 0;

			break;
		}

		case ALTSOUND_HARDWARE_GEN_WPCALPHA_2: //!! ?? test this gen actually
		case ALTSOUND_HARDWARE_GEN_WPCDMD: // remaps everything to 16bit, a bit stupid maybe
		case ALTSOUND_HARDWARE_GEN_WPCFLIPTRON: {
			ALT_DEBUG(0, "Hardware Generation: WPCALPHA_2, WPCDMD, WPCFLIPTRON");

			g_cmdData.cmd_filter = 0;
			if ((g_cmdData.cmd_buffer[2] == 0x79) && (g_cmdData.cmd_buffer[1] == (g_cmdData.cmd_buffer[0] ^ 0xFF))) { // change volume op (following first byte = volume, second = ~volume, if these don't match: ignore)
				if (g_pProcessor->romControlsVol()) {
					g_pProcessor->setGlobalVol(std::min((float)g_cmdData.cmd_buffer[1] / 127.f, 1.0f));
					ALT_INFO(0, "Change volume %.02f", g_pProcessor->getGlobalVol());
				}

				for (int i = 0; i < ALT_MAX_CMDS; ++i)
					g_cmdData.cmd_buffer[i] = ~0;

				g_cmdData.cmd_counter = 0;
				g_cmdData.cmd_filter = 1;
			}
			else if (g_cmdData.cmd_buffer[1] == 0x7A) { // 16bit command second part //!! TZ triggers a 0xFF in the beginning -> check sequence and filter?
				g_cmdData.stored_command = g_cmdData.cmd_buffer[1];
				g_cmdData.cmd_counter = 0;
			}
			else if (cmd != 0x7A) { // 8 bit command
				g_cmdData.stored_command = 0;
				g_cmdData.cmd_counter = 0;
			}
			else // 16bit command first part
				g_cmdData.cmd_counter = 1;

			break;
		}

		case ALTSOUND_HARDWARE_GEN_WPCALPHA_1: // remaps everything to 16bit, a bit stupid maybe //!! test all these generations!
		case ALTSOUND_HARDWARE_GEN_S11:
		case ALTSOUND_HARDWARE_GEN_S11X:
		case ALTSOUND_HARDWARE_GEN_S11B2:
		case ALTSOUND_HARDWARE_GEN_S11C: {
			ALT_DEBUG(0, "Hardware Generation: WPCALPHA_1, S11, S11X, S11B2, S11C");

			if (cmd != g_cmdData.cmd_buffer[1]) { //!! some stuff is doubled or tripled -> filter out?
				g_cmdData.stored_command = 0; // 8 bit command //!! 7F & 7E opcodes?
				g_cmdData.cmd_counter = 0;
			}
			else
				g_cmdData.cmd_counter = 1;

			break;
		}

		case ALTSOUND_HARDWARE_GEN_DEDMD16: // remaps everything to 16bit, a bit stupid maybe
		case ALTSOUND_HARDWARE_GEN_DEDMD32:
		case ALTSOUND_HARDWARE_GEN_DEDMD64:
		case ALTSOUND_HARDWARE_GEN_DE: { // this one just tested with BTTF so far
			ALT_DEBUG(0, "Hardware Generation: DEDMD16, DEDMD32, DEDMD64, DE");

			if (cmd != 0xFF && cmd != 0x00) { // 8 bit command
				g_cmdData.stored_command = 0;
				g_cmdData.cmd_counter = 0;
			}
			else // ignore
				g_cmdData.cmd_counter = 1;

			if (g_cmdData.cmd_buffer[1] == 0x00 && cmd == 0x00) { // handle 0x0000 special //!! meh?
				g_cmdData.stored_command = 0;
 				g_cmdData.cmd_counter = 0;
			}

			break;
		}

		case ALTSOUND_HARDWARE_GEN_WS:
		case ALTSOUND_HARDWARE_GEN_WS_1:
		case ALTSOUND_HARDWARE_GEN_WS_2: {
			ALT_DEBUG(0, "Hardware Generation: WS, WS_1, WS_2");

			g_cmdData.cmd_filter = 0;
			if (g_cmdData.cmd_buffer[1] == 0xFE) {
				if (cmd >= 0x10 && cmd <= 0x2F) {
					if (g_pProcessor->romControlsVol()) {
						g_pProcessor->setGlobalVol((float)(0x2F - cmd) / 31.f);
						ALT_INFO(0, "Change volume %.02f", g_pProcessor->getGlobalVol());
					}

					for (int i = 0; i < ALT_MAX_CMDS; ++i)
						g_cmdData.cmd_buffer[i] = ~0;

					g_cmdData.cmd_counter = 0;
					g_cmdData.cmd_filter = 1;
				}
				else if (cmd >= 0x01 && cmd <= 0x0F) { // ignore FE 01 ... FE 0F
					g_cmdData.stored_command = 0;
					g_cmdData.cmd_counter = 0;
					g_cmdData.cmd_filter = 1;
				}
			}

			if ((cmd & 0xFC) == 0xFC) // start byte of a command will ALWAYS be FF, FE, FD, FC, and never the second byte!
				g_cmdData.cmd_counter = 1;

			break;
		}

		// DAR@20240207 Seems to be 8-bit commands
		case ALTSOUND_HARDWARE_GEN_GTS80/*A*/: {  // Gottlieb System 80A
			ALT_DEBUG(0, "Hardware Generation: GTS80A");

			// DAR@29249297 It appears that this system sends 0x00 commands as a clock
			//              signal, since we recieve a ridiculous number of them.
			//              Filter them out
			if (cmd == 0x00) { // handle 0x00
				g_cmdData.stored_command = 0;
				g_cmdData.cmd_counter = 0;
				g_cmdData.cmd_filter = 1;
			}
			else {
				g_cmdData.stored_command = 0;
				g_cmdData.cmd_counter = 0;
				g_cmdData.cmd_filter = 0;
			}

			break;
		}

		// Bally MPU-17/MPU-35 and every generation without a case above: one byte per sound command for the packs
		// made for the real sound commands as PinMAME 3.7 and later logs them (new_sound_commands = 1), the bytes
		// paired two by two for the older packs, as before. Until the TODO below is done, packs for the machines
		// whose commands only the driver logs (e.g. Capcom, Pinball 2000, Mr. Game, Hankin, Atari, Zira,
		// Spinball/Inder, Zaccaria 1311) need new_sound_commands = 1.
		// TODO: Take the commands as PinMAME's own AltSound does (alt_sound_handle, preprocess_commands in its
		// snd_alt.cpp), once libpinmame passes each command's source on (TODO in its libpinmame.cpp) and the API
		// takes it (TODO at AltSoundProcessBoardCommand in altsound.h):
		// - a driver's own commands (SND_CMD_DRIVER): one by one for any pack (Capcom, Pinball 2000: 16-bit values)
		// - data writes logged as in PinMAME 3.6 (SND_CMD_DATA): as now
		// - boards whose logging changed in 3.7 (SNDBRD_DATALOG_LEGACY): their old data writes (SND_CMD_LEGACY),
		//   paired, for the old packs, their driver's commands for the others, skipping the other kind
		case ALTSOUND_HARDWARE_GEN_BY17:
		case ALTSOUND_HARDWARE_GEN_BY35:
		default: {
			if (g_newSoundCommands) {
				ALT_DEBUG(0, "Hardware Generation: BY17, BY35 or other (one byte per command)");

				g_cmdData.stored_command = 0;
				g_cmdData.cmd_counter = 0;
				g_cmdData.cmd_filter = 0;
			}
			else {
				ALT_DEBUG(0, "Hardware Generation: BY17, BY35 or other (bytes paired)");
			}

			break;
		}
	}

	ALT_OUTDENT;
	ALT_DEBUG(0, "END altsound_preprocess_commands()");
}

/******************************************************
 * altsound_postprocess_commands
 ******************************************************/

void altsound_postprocess_commands(const unsigned int combined_cmd)
{
	ALT_DEBUG(0, "BEGIN altsound_postprocess_commands()");
	ALT_INDENT;

	ALT_DEBUG(0, "MAME_GEN: 0x%013llx", (unsigned long long)g_hardwareGen);

	switch (g_hardwareGen) {
		case ALTSOUND_HARDWARE_GEN_WPCDCS:
		case ALTSOUND_HARDWARE_GEN_WPCSECURITY:
		case ALTSOUND_HARDWARE_GEN_WPC95DCS:
		case ALTSOUND_HARDWARE_GEN_WPC95:
			if (combined_cmd == 0x03E3) { // stop music
				ALT_INFO(0, "Stopping MUSIC(2)");
				g_pProcessor->stopMusic();
			}
			break;

		//!! old WPC machines music stop? -> 0x00 for SYS11?

		case ALTSOUND_HARDWARE_GEN_DEDMD32:
			if ((combined_cmd == 0x0018 || combined_cmd == 0x0023)) { // stop music //!! ???? 0x0019??
				ALT_INFO(0, "Stopping MUSIC(3)");
				g_pProcessor->stopMusic();
			}
			break;

		case ALTSOUND_HARDWARE_GEN_WS:
		case ALTSOUND_HARDWARE_GEN_WS_1:
		case ALTSOUND_HARDWARE_GEN_WS_2:
			if (((combined_cmd == 0x0000 || (combined_cmd & 0xf0ff) == 0xf000))) { // stop music
				ALT_INFO(0, "Stopping MUSIC(4)");
				g_pProcessor->stopMusic();
			}
			break;

		default: break;
	}

	ALT_OUTDENT;
	ALT_DEBUG(0, "END postprocess_commands()");
}

/******************************************************
 * AltSoundSetLogger
 ******************************************************/

ALTSOUNDAPI void AltSoundSetLogger(const string& logPath, ALTSOUND_LOG_LEVEL logLevel, bool console)
{
	alog.setLogPath(logPath);
	alog.setLogLevel((AltsoundLogger::Level)logLevel);
	alog.enableConsole(console);
}

/******************************************************
 * AltSoundInit
 ******************************************************/

ALTSOUNDAPI bool AltSoundInit(const string& pinmamePath, const string& gameName,
                              uint32_t sampleRate, uint32_t channels, uint32_t bufferSizeFrames)
{
	ALT_DEBUG(0, "BEGIN AltSoundInit()");
	ALT_INDENT;

	if (g_pProcessor) {
		ALT_ERROR(0, "Processor already defined");
		ALT_OUTDENT;
		ALT_DEBUG(0, "END AltSoundInit()");
		return false;
	}

	g_sampleRate = sampleRate;
	g_channels = channels;
	g_bufferSizeFrames = bufferSizeFrames;

	g_engine = new ma_engine();
	g_context = new ma_context();
	if (altsound_ma_engine_init_null_device(g_channels, g_sampleRate, g_bufferSizeFrames,
			AltsoundEngineProcess, nullptr, g_context, g_engine) != MA_SUCCESS) {
		ALT_ERROR(0, "FAILED to initialize miniAudio engine");
		delete g_engine;
		g_engine = nullptr;
		delete g_context;
		g_context = nullptr;
		ALT_OUTDENT;
		ALT_DEBUG(0, "END AltSoundInit()");
		return false;
	}

	// initialize channel_stream storage
	std::fill(channel_stream.begin(), channel_stream.end(), nullptr);

	const std::filesystem::path pinmame_path(pinmamePath);
	const std::filesystem::path altsound_path = pinmame_path / "altsound" / gameName;

	// parse .ini file
	AltsoundIniProcessor ini_proc;
	if (!ini_proc.parse_altsound_ini(altsound_path)) {
		// Error message and return
		ALT_ERROR(0, "Failed to parse_altsound_ini(%s)", altsound_path.string().c_str());
		ALT_OUTDENT;
		ALT_DEBUG(0, "END AltSoundInit()");
		return false;
	}

	string format = ini_proc.getAltsoundFormat();

	if (format == "g-sound") {
		// G-Sound only supports new CSV format. No need to specify format
		// in the constructor
		g_pProcessor = new GSoundProcessor(gameName, pinmame_path);
	}
	else if (format == "altsound" || format == "legacy") {
		g_pProcessor = new AltsoundProcessor(gameName, pinmame_path, format);
	}
	else {
		ALT_ERROR(0, "Unknown AltSound format: %s", format.c_str());
		ALT_OUTDENT;
		ALT_DEBUG(0, "END AltSoundInit()");
		return false;
	}

	if (!g_pProcessor) {
		ALT_ERROR(0, "FAILED: Unable to create AltSound Processor");
		ALT_OUTDENT;
		ALT_DEBUG(0, "END AltSoundInit()");
		return false;
	}

	ALT_INFO(0, "%s processor created", format.c_str());

	g_pProcessor->setMasterVol(1.0f);
	g_pProcessor->setGlobalVol(1.0f);
	g_pProcessor->romControlsVol(ini_proc.usingRomVolumeControl());
	g_pProcessor->recordSoundCmds(ini_proc.recordSoundCmds());
	g_pProcessor->setSkipCount(ini_proc.getSkipCount());
	g_newSoundCommands = ini_proc.newSoundCommands();
	g_pProcessor->setDuckReleaseMs(ini_proc.getDuckReleaseMs());

	// perform processor initialization (load samples, etc)
	g_pProcessor->init();

	g_cmdData.cmd_counter = 0;
	g_cmdData.stored_command = -1;
	g_cmdData.cmd_filter = 0;
	std::fill_n(g_cmdData.cmd_buffer, ALT_MAX_CMDS, ~0);

	altsound_ma_engine_start(g_engine);

	ALT_DEBUG(0, "END AltSoundInit()");
	return true;
}

/******************************************************
 * AltSoundProcessCommand
 ******************************************************/

ALTSOUNDAPI void AltSoundSetHardwareGen(ALTSOUND_HARDWARE_GEN hardwareGen)
{
	ALT_DEBUG(0, "BEGIN AltSoundSetHardwareGen()");
	ALT_INDENT;

	g_hardwareGen = hardwareGen;

	ALT_DEBUG(0, "MAME_GEN: 0x%013llx", (unsigned long long)g_hardwareGen);

	ALT_OUTDENT;
	ALT_DEBUG(0, "END AltSoundSetHardwareGen()");
}

/******************************************************
 * AltSoundSetAudioCallback
 ******************************************************/

ALTSOUNDAPI void AltSoundSetAudioCallback(AltSoundAudioCallback callback, void* userData)
{
	ALT_DEBUG(0, "BEGIN AltSoundSetAudioCallback()");
	ALT_INDENT;

	std::lock_guard<std::mutex> lock(g_audioMutex);
	g_audioCallback = callback;
	g_audioUserData = userData;

	ALT_DEBUG(0, "Audio callback %s", callback ? "set" : "cleared");

	ALT_OUTDENT;
	ALT_DEBUG(0, "END AltSoundSetAudioCallback()");
}

/******************************************************
 * AltSoundProcessCommand
 ******************************************************/

static unsigned int g_cmdBoard = 0; // sound board of the command being processed

ALTSOUNDAPI bool AltSoundProcessCommand(const unsigned int cmd, int attenuation)
{
	ALT_DEBUG(0, "BEGIN AltSoundProcessCommand()");

	if (!g_pProcessor) {
		ALT_ERROR(0, "AltSound not initialized. Processing skipped.");
		return false;
	}

	ALT_INDENT;

	// master volume from the attenuation in dB (negative values attenuate, positive ones are a gain),
	// set as is, not relative to the previous one
	const float master_vol = powf(10.f, (float)attenuation / 20.f);
	g_pProcessor->setMasterVol(master_vol);
	ALT_DEBUG(0, "Master Volume (Post Attenuation): %.02f", master_vol);

	g_cmdData.cmd_counter++;

	//Shift all commands up to free up slot 0
	for (int i = ALT_MAX_CMDS - 1; i > 0; --i)
		g_cmdData.cmd_buffer[i] = g_cmdData.cmd_buffer[i - 1];

	g_cmdData.cmd_buffer[0] = cmd; //add command to slot 0

	// pre-process commands based on ROM hardware platform
	altsound_preprocess_commands(cmd);

	if (g_cmdData.cmd_filter || (g_cmdData.cmd_counter & 1) != 0) {
		// Some commands are 16-bits collected from two 8-bit commands.  If
		// the command is filtered or we have not received enough data yet,
		// try again on the next command
		//
		// NOTE:
		// Command size and filter requirements are ROM hardware platform
		// dependent.  The command preprocessor will take care of the
		// bookkeeping

		// Store the command for accumulation
		g_cmdData.stored_command = cmd;

		if (g_cmdData.cmd_filter) {
			ALT_DEBUG(0, "Command filtered: %04X", cmd);
		}

		if ((g_cmdData.cmd_counter & 1) != 0) {
			ALT_DEBUG(0, "Command incomplete: %04X", cmd);
		}

		ALT_OUTDENT;
		ALT_DEBUG(0, "END AltSoundProcessCommand");
		return true;
	}
	ALT_DEBUG(0, "Command complete. Processing...");

	// combine stored command with the current
	unsigned int cmd_combined = (g_cmdData.stored_command << 8) | cmd;

	// System 11 machines with two sound boards: both take bytes 01..FF, so the commands of
	// board 1 (the separate sound board) are 0x01xx when the pack has a sample for them
	if (g_cmdBoard == 1 && (g_hardwareGen == ALTSOUND_HARDWARE_GEN_S11X || g_hardwareGen == ALTSOUND_HARDWARE_GEN_S11B2)
		&& g_pProcessor->hasSample(0x0100 | cmd_combined))
		cmd_combined |= 0x0100;

	// Handle the resulting command
	if (!ALT_CALL(g_pProcessor->handleCmd(cmd_combined))) {
		ALT_WARNING(0, "FAILED processor::handleCmd()");

		altsound_postprocess_commands(cmd_combined);

		ALT_OUTDENT;
		ALT_DEBUG(0, "END alt_sound_handle()");
		return false;
	}
	ALT_INFO(0, "SUCCESS processor::handleCmd()");

	altsound_postprocess_commands(cmd_combined);

	ALT_OUTDENT;
	ALT_DEBUG(0, "END AltSoundProcessCommand()");
	ALT_DEBUG(0, "");

	return true;
}

/******************************************************
 * AltSoundProcessBoardCommand
 ******************************************************/

ALTSOUNDAPI bool AltSoundProcessBoardCommand(const unsigned int board, const unsigned int cmd, int attenuation)
{
	g_cmdBoard = board;
	const bool result = AltSoundProcessCommand(cmd, attenuation);
	g_cmdBoard = 0;
	return result;
}

/******************************************************
 * AltSoundPause
 ******************************************************/

ALTSOUNDAPI void AltSoundPause(bool pause)
{
	ALT_DEBUG(0, "BEGIN alt_sound_pause()");
	ALT_INDENT;

	// under io_mutex, as the stream end callbacks (miniaudio thread) free streams
	std::lock_guard<std::mutex> guard(io_mutex);

	if (pause) {
		ALT_INFO(0, "Pausing stream playback (ALL)");

		// Pause the channels that play
		for (int i = 0; i < ALT_MAX_CHANNELS; ++i) {
			if (!channel_stream[i])
				continue;

			const unsigned int stream = channel_stream[i]->hstream;
			if (MiniAudio_ChannelIsActive(stream) == MINIAUDIO_ACTIVE_PLAYING && MiniAudio_ChannelPause(stream)) {
				ALT_INFO(0, "SUCCESS: Paused stream %u", stream);
			}
		}
	}
	else {
		ALT_INFO(0, "Resuming stream playback (ALL)");

		// Resume the paused channels (only those, not the ones that ended or never started)
		for (int i = 0; i < ALT_MAX_CHANNELS; ++i) {
			if (!channel_stream[i])
				continue;

			const unsigned int stream = channel_stream[i]->hstream;
			if (MiniAudio_ChannelIsActive(stream) == MINIAUDIO_ACTIVE_PAUSED && MiniAudio_ChannelPlay(stream, false)) {
				ALT_INFO(0, "SUCCESS: Resumed stream %u", stream);
			}
		}
	}

	ALT_OUTDENT;
	ALT_DEBUG(0, "END alt_sound_pause()");
}

/******************************************************
 * AltSoundShutdown
 ******************************************************/

ALTSOUNDAPI void AltSoundShutdown()
{
	ALT_DEBUG(0, "BEGIN AltSoundShutdown()");
	ALT_INDENT;

	// Stop miniAudio's audio thread first so no further mixing/onProcess
	// callbacks run while we tear down the streams and engine.
	if (g_engine)
		altsound_ma_engine_stop(g_engine);

	// Discard any end-of-stream notifications that were never drained; the
	// streams they reference are about to be freed.
	{
		std::lock_guard<std::mutex> lock(g_endedMutex);
		g_endedStreams.clear();
	}

	if (g_pProcessor) {
		delete g_pProcessor;
		g_pProcessor = NULL;
	}

	if (g_engine) {
		altsound_ma_engine_uninit(g_engine);
		delete g_engine;
		g_engine = nullptr;
	}

	if (g_context) {
		altsound_ma_context_uninit(g_context);
		delete g_context;
		g_context = nullptr;
	}

	std::lock_guard<std::mutex> lock(g_audioMutex);
	g_audioCallback = nullptr;
	g_audioUserData = nullptr;

	ALT_OUTDENT;
	ALT_DEBUG(0, "END AltSoundShutdown()");
}

