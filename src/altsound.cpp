#define NOMINMAX

#include "altsound.h"

#include "altsound_data.hpp"
#include "altsound_ini_processor.hpp"
#include "altsound_processor_base.hpp"
#include "altsound_processor.hpp"
#include "gsound_processor.hpp"

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

/******************************************************
 * altsound_preprocess_commands
 ******************************************************/

void altsound_preprocess_commands(int cmd)
{
	ALT_DEBUG(0, "BEGIN altsound_preprocess_commands()");
	ALT_INDENT;

	ALT_DEBUG(0, "MAME_GEN: 0x%013x", (uint64_t)g_hardwareGen);

	switch (g_hardwareGen) {
		case ALTSOUND_HARDWARE_GEN_WPCDCS:
		case ALTSOUND_HARDWARE_GEN_WPCSECURITY:
		case ALTSOUND_HARDWARE_GEN_WPC95DCS:
		case ALTSOUND_HARDWARE_GEN_WPC95: {
			ALT_DEBUG(0, "Hardware Generation: WPCDCS, WPCSECURITY, WPC95DCS, WPC95");

			if (((g_cmdData.cmd_buffer[3] == 0x55) && (g_cmdData.cmd_buffer[2] == 0xAA))) { // change volume?
				// DAR@20240208 The check below is dangerous.  If this is still a 
				//              problem, it would be better to revisit it when it
				//              reappears to implement a more robust solution that
				//              works for all systems
				//              See https://github.com/vpinball/pinmame/issues/220
				//|| ((g_cmdData.cmd_buffer[2] == 0x00) && (g_cmdData.cmd_buffer[1] == 0x00) && (g_cmdData.cmd_buffer[0] == 0x00))) { // glitch in command buffer?
				if ((g_cmdData.cmd_buffer[3] == 0x55) && (g_cmdData.cmd_buffer[2] == 0xAA) && (g_cmdData.cmd_buffer[1] == (g_cmdData.cmd_buffer[0] ^ 0xFF))) { // change volume op (following first byte = volume, second = ~volume, if these don't match: ignore)
					if (g_pProcessor->romControlsVol()) {
						g_pProcessor->setGlobalVol(std::min((float)g_cmdData.cmd_buffer[1] / 127.f, 1.0f));
						ALT_INFO(0, "Change volume %.02f", g_pProcessor->getGlobalVol());
					}
				}
				else
					ALT_DEBUG(0, "filtered command %02X %02X %02X %02X", g_cmdData.cmd_buffer[3], g_cmdData.cmd_buffer[2], g_cmdData.cmd_buffer[1], g_cmdData.cmd_buffer[0]);

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

		default: break;
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

	ALT_DEBUG(0, "MAME_GEN: 0x%013x", (uint64_t)g_hardwareGen);

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
 * AltsoundSetLogger
 ******************************************************/

ALTSOUNDAPI void AltsoundSetLogger(const string& logPath, ALTSOUND_LOG_LEVEL logLevel, bool console)
{
	alog.setLogPath(logPath);
	alog.setLogLevel((AltsoundLogger::Level)logLevel);
	alog.enableConsole(console);
}

/******************************************************
 * AltsoundInit
 ******************************************************/

ALTSOUNDAPI bool AltsoundInit(const string& pinmamePath, const string& gameName)
{
	ALT_DEBUG(0, "BEGIN AltsoundInit()");
	ALT_INDENT;

	if (g_pProcessor) {
		ALT_ERROR(0, "Processor already defined");
		ALT_OUTDENT;
		ALT_DEBUG(0, "END AltsoundInit()");
		return false;
	}

	// initialize channel_stream storage
	std::fill(channel_stream.begin(), channel_stream.end(), nullptr);

	string szPinmamePath = pinmamePath;

	std::replace(szPinmamePath.begin(), szPinmamePath.end(), '\\', '/');

	if (szPinmamePath.back() != '/')
		szPinmamePath += '/';

	const string szAltSoundPath = szPinmamePath + "altsound/" + gameName + '/';

	// parse .ini file
	AltsoundIniProcessor ini_proc;
	if (!ini_proc.parse_altsound_ini(szAltSoundPath)) {
		// Error message and return
		ALT_ERROR(0, "Failed to parse_altsound_ini(%s)", szAltSoundPath.c_str());
		ALT_OUTDENT;
		ALT_DEBUG(0, "END AltsoundInit()");
		return false;
	}

	string format = ini_proc.getAltsoundFormat();

	if (format == "g-sound") {
		// G-Sound only supports new CSV format. No need to specify format
		// in the constructor
		g_pProcessor = new GSoundProcessor(gameName, szPinmamePath);
	}
	else if (format == "altsound" || format == "legacy") {
		g_pProcessor = new AltsoundProcessor(gameName, szPinmamePath, format);
	}
	else {
		ALT_ERROR(0, "Unknown AltSound format: %s", format.c_str());
		ALT_OUTDENT;
		ALT_DEBUG(0, "END AltsoundInit()");
		return false;
	}

	if (!g_pProcessor) {
		ALT_ERROR(0, "FAILED: Unable to create AltSound Processor");
		ALT_OUTDENT;
		ALT_DEBUG(0, "END AltsoundInit()");
		return false;
	}
	
	ALT_INFO(0, "%s processor created", format.c_str());

	g_pProcessor->setMasterVol(1.0f);
	g_pProcessor->setGlobalVol(1.0f);
	g_pProcessor->romControlsVol(ini_proc.usingRomVolumeControl());
	g_pProcessor->recordSoundCmds(ini_proc.recordSoundCmds());
	g_pProcessor->setSkipCount(ini_proc.getSkipCount());

	// perform processor initialization (load samples, etc)
	g_pProcessor->init();

	g_cmdData.cmd_counter = 0;
	g_cmdData.stored_command = -1;
	g_cmdData.cmd_filter = 0;
	std::fill_n(g_cmdData.cmd_buffer, ALT_MAX_CMDS, ~0);

	// Initialize BASS
	int DSidx = -1; // BASS default device

	if (!BASS_Init(DSidx, 44100, 0, NULL, NULL)) {
		ALT_ERROR(0, "BASS initialization error: %s", get_bass_err());
	}

	ALT_DEBUG(0, "END AltsoundInit()");
	return true;
}

/******************************************************
 * AltsoundProcessCommand
 ******************************************************/

ALTSOUNDAPI void AltsoundSetHardwareGen(ALTSOUND_HARDWARE_GEN hardwareGen)
{
	ALT_DEBUG(0, "BEGIN AltsoundSetHardwareGen()");
	ALT_INDENT;

	g_hardwareGen = hardwareGen;

	ALT_DEBUG(0, "MAME_GEN: 0x%013x", (uint64_t)g_hardwareGen);

	ALT_OUTDENT;
	ALT_DEBUG(0, "END AltsoundSetHardwareGen()");
}

/******************************************************
 * AltsoundProcessCommand
 ******************************************************/

ALTSOUNDAPI bool AltsoundProcessCommand(const unsigned int cmd, int attenuation)
{
	ALT_DEBUG(0, "BEGIN AltsoundProcessCommand()");

	float master_vol = g_pProcessor->getMasterVol();
	while (attenuation++ < 0) {
		master_vol /= 1.122018454f; // = (10 ^ (1/20)) = 1dB
	}
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
		ALT_DEBUG(0, "END AltsoundProcessCommand");
		return true;
	}
	ALT_DEBUG(0, "Command complete. Processing...");

	// combine stored command with the current
	const unsigned int cmd_combined = (g_cmdData.stored_command << 8) | cmd;

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
	ALT_DEBUG(0, "END AltsoundProcessCommand()");
	ALT_DEBUG(0, "");

	return true;
}

/******************************************************
 * AltsoundPause
 ******************************************************/

ALTSOUNDAPI void AltsoundPause(bool pause)
{
	ALT_DEBUG(0, "BEGIN alt_sound_pause()");
	ALT_INDENT;

	if (pause) {
		ALT_INFO(0, "Pausing stream playback (ALL)");

		// Pause all channels
		for (int i = 0; i < ALT_MAX_CHANNELS; ++i) {
			if (!channel_stream[i])
				continue;

			const HSTREAM stream = channel_stream[i]->hstream;
			if (BASS_ChannelIsActive(stream) == BASS_ACTIVE_PLAYING) {
				if (!BASS_ChannelPause(stream)) {
					ALT_WARNING(0, "FAILED BASS_ChannelPause(%u): %s", stream, get_bass_err());
				}
				else {
					ALT_INFO(0, "SUCCESS BASS_ChannelPause(%u)", stream);
				}
			}
		}
	}
	else {
		ALT_INFO(0, "Resuming stream playback (ALL)");

		// Resume all channels
		for (int i = 0; i < ALT_MAX_CHANNELS; ++i) {
			if (!channel_stream[i])
				continue;

			const HSTREAM stream = channel_stream[i]->hstream;
			if (BASS_ChannelIsActive(stream) == BASS_ACTIVE_PAUSED) {
				if (!BASS_ChannelPlay(stream, 0)) {
					ALT_WARNING(0, "FAILED BASS_ChannelPlay(%u): %s", stream, get_bass_err());
				}
				else {
					ALT_INFO(0, "SUCCESS BASS_ChannelPlay(%u)", stream);
				}
			}
		}
	}

	ALT_OUTDENT;
	ALT_DEBUG(0, "END alt_sound_pause()");
}

/******************************************************
 * AltsoundShutdown
 ******************************************************/

ALTSOUNDAPI void AltsoundShutdown()
{
	ALT_DEBUG(0, "BEGIN AltsoundShutdown()");
	ALT_INDENT;

	if (g_pProcessor) {
		delete g_pProcessor;
		g_pProcessor = NULL;
	}

	ALT_OUTDENT;
	ALT_DEBUG(0, "END AltsoundShutdown()");
}
