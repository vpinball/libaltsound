#define NOMINMAX

#include "altsound.h"

#include "altsound_data.hpp"
#include "altsound_ini_processor.hpp"
#include "altsound_processor_base.hpp"
#include "altsound_processor.hpp"
#include "gsound_processor.hpp"
#include "miniaudio_bass_compat.hpp"

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

std::unordered_map<unsigned int, _internal_stream_data> g_streamMap;
std::mutex g_streamMapMutex;

static AltSoundAudioCallback g_audioCallback = nullptr;
static void* g_audioUserData = nullptr;
static std::mutex g_audioMutex;
uint32_t g_sampleRate = 44100;
uint32_t g_channels = 2;
uint32_t g_nextStreamId = 1;
int g_last_ma_err = 0;

static std::thread g_audioThread;
static std::atomic<bool> g_audioRunning{false};
static uint32_t g_bufferSizeFrames = 256;

static std::condition_variable g_audioWakeup;
static std::mutex g_audioWakeupMutex;

/******************************************************
 * Audio mixing thread
 ******************************************************/

static void AudioMixingThread()
{
    float* mixBuffer = new float[g_bufferSizeFrames * g_channels];
    float* tempBuffer = new float[g_bufferSizeFrames * g_channels];

    using clock = std::chrono::steady_clock;
    using namespace std::chrono;
    auto period = duration<double>(static_cast<double>(g_bufferSizeFrames) / std::max<uint32_t>(1, g_sampleRate));
    auto nextDue = clock::now();

    while (g_audioRunning.load()) {
        {
            std::unique_lock<std::mutex> lock(g_audioWakeupMutex);
            g_audioWakeup.wait_until(lock, nextDue);
        }

        for (size_t i = 0; i < g_bufferSizeFrames * g_channels; ++i)
            mixBuffer[i] = 0.0f;

        std::vector<std::pair<AltsoundStreamInfo*, _internal_stream_data*>> activeStreams;
        {
            std::lock_guard<std::mutex> lock(io_mutex);
            std::lock_guard<std::mutex> streamLock(g_streamMapMutex);
            for (int i = 0; i < ALT_MAX_CHANNELS; ++i) {
                AltsoundStreamInfo* stream = channel_stream[i];
                if (stream && stream->hstream != MINIAUDIO_NO_STREAM) {
                    auto it = g_streamMap.find(stream->hstream);
                    if (it != g_streamMap.end() && it->second.decoder && it->second.playing && !it->second.paused) {
                        activeStreams.push_back({stream, &it->second});
                    }
                }
            }
        }

        for (auto& streamPair : activeStreams) {
            AltsoundStreamInfo* stream = streamPair.first;
            _internal_stream_data* internal = streamPair.second;

            const float volume = stream->gain * stream->ducking * g_pProcessor->getGlobalVol() * g_pProcessor->getMasterVol();
            const uint32_t outCh = g_channels;
            const uint32_t inCh = internal->channels;

            size_t dstBaseFrame = 0;
            while (dstBaseFrame < g_bufferSizeFrames) {
                const ma_uint64 framesRequested = g_bufferSizeFrames - dstBaseFrame;
                ma_uint64 framesRead = 0;
                ma_result result = altsound_ma_decoder_read_pcm_frames(internal->decoder, tempBuffer, framesRequested, &framesRead);

                if (framesRead == 0) {
                    if (stream->loop) {
                        altsound_ma_decoder_seek_to_pcm_frame(internal->decoder, 0);
                        continue;
                    } else {
                        internal->playing = false;
                        if (internal->sync_callback) {
                            internal->sync_callback(stream->hsync, stream->hstream, 0, internal->sync_userdata);
                        }
                        break;
                    }
                }

                const size_t framesToMix = static_cast<size_t>(framesRead);
                if (inCh == outCh) {
                    for (size_t frame = 0; frame < framesToMix; ++frame) {
                        for (uint32_t ch = 0; ch < outCh; ++ch) {
                            size_t idx = (dstBaseFrame + frame) * outCh + ch;
                            mixBuffer[idx] += tempBuffer[frame * inCh + ch] * volume;
                        }
                    }
                } else {
                    for (size_t frame = 0; frame < framesToMix; ++frame) {
                        for (uint32_t ch = 0; ch < outCh; ++ch) {
                            size_t srcIdx = frame * inCh + (ch % inCh);
                            size_t dstIdx = (dstBaseFrame + frame) * outCh + ch;
                            mixBuffer[dstIdx] += tempBuffer[srcIdx] * volume;
                        }
                    }
                }

                if (framesRead < framesRequested) {
                    if (stream->loop) {
                        altsound_ma_decoder_seek_to_pcm_frame(internal->decoder, 0);
                        dstBaseFrame += framesToMix;
                        continue;
                    } else {
                        dstBaseFrame += framesToMix;
                        break;
                    }
                }

                dstBaseFrame += framesToMix;
            }
        }

        {
            std::lock_guard<std::mutex> lock(g_audioMutex);
            if (g_audioCallback)
                g_audioCallback(mixBuffer, g_bufferSizeFrames, g_sampleRate, g_channels, g_audioUserData);
        }

        nextDue += duration_cast<clock::duration>(period);
        auto now = clock::now();
        if (now >= nextDue)
            nextDue = now + duration_cast<clock::duration>(period);
    }

    delete[] mixBuffer;
    delete[] tempBuffer;
}

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
		ALT_DEBUG(0, "END AltSoundInit()");
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

	// perform processor initialization (load samples, etc)
	g_pProcessor->init();

	g_cmdData.cmd_counter = 0;
	g_cmdData.stored_command = -1;
	g_cmdData.cmd_filter = 0;
	std::fill_n(g_cmdData.cmd_buffer, ALT_MAX_CMDS, ~0);

	g_audioRunning.store(true);
	g_audioThread = std::thread(AudioMixingThread);

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

	ALT_DEBUG(0, "MAME_GEN: 0x%013x", (uint64_t)g_hardwareGen);

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

ALTSOUNDAPI bool AltSoundProcessCommand(const unsigned int cmd, int attenuation)
{
	ALT_DEBUG(0, "BEGIN AltSoundProcessCommand()");

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
		ALT_DEBUG(0, "END AltSoundProcessCommand");
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

	// Wake up audio thread immediately for responsive sound triggering
	g_audioWakeup.notify_one();

	altsound_postprocess_commands(cmd_combined);

	ALT_OUTDENT;
	ALT_DEBUG(0, "END AltSoundProcessCommand()");
	ALT_DEBUG(0, "");

	return true;
}

/******************************************************
 * AltSoundPause
 ******************************************************/

ALTSOUNDAPI void AltSoundPause(bool pause)
{
	ALT_DEBUG(0, "BEGIN alt_sound_pause()");
	ALT_INDENT;

	if (pause) {
		ALT_INFO(0, "Pausing stream playback (ALL)");

		// Pause all channels
		for (int i = 0; i < ALT_MAX_CHANNELS; ++i) {
			if (!channel_stream[i])
				continue;

			if (MiniAudio_ChannelPause(channel_stream[i]->hstream)) {
				ALT_INFO(0, "SUCCESS: Paused stream %u", channel_stream[i]->hstream);
			}
		}
	}
	else {
		ALT_INFO(0, "Resuming stream playback (ALL)");

		// Resume all channels
		for (int i = 0; i < ALT_MAX_CHANNELS; ++i) {
			if (!channel_stream[i])
				continue;

			if (MiniAudio_ChannelPlay(channel_stream[i]->hstream, false)) {
				ALT_INFO(0, "SUCCESS: Resumed stream %u", channel_stream[i]->hstream);
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

	g_audioRunning.store(false);
	if (g_audioThread.joinable()) {
		g_audioThread.join();
	}

	if (g_pProcessor) {
		delete g_pProcessor;
		g_pProcessor = NULL;
	}

	std::lock_guard<std::mutex> lock(g_audioMutex);
	g_audioCallback = nullptr;
	g_audioUserData = nullptr;

	ALT_OUTDENT;
	ALT_DEBUG(0, "END AltSoundShutdown()");
}

