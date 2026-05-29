#include <miniaudio/miniaudio.h>

#include "miniaudio_bass_compat.hpp"
#include "miniaudio_private.h"
#include "altsound_data.hpp"
#include "altsound_logger.hpp"

#include <unordered_map>
#include <mutex>
#include <atomic>

extern StreamArray channel_stream;
extern std::unordered_map<unsigned int, _internal_stream_data> g_streamMap;
extern std::mutex g_streamMapMutex;
extern uint32_t g_nextStreamId;
extern uint32_t g_channels;
extern uint32_t g_sampleRate;
extern ma_engine* g_engine;

unsigned int MiniAudio_StreamCreateFile(bool mem, const char* file, unsigned long long offset, unsigned long long length, unsigned int flags)
{
	if (!file) {
		MiniAudio_ErrorSetCode(MA_INVALID_ARGS);
		return MINIAUDIO_NO_STREAM;
	}

	ma_decoder_config config = altsound_ma_decoder_config_init(ma_format_f32, g_channels, g_sampleRate);
	ma_decoder* decoder = new ma_decoder();
	ma_result result = altsound_ma_decoder_init_file(file, &config, decoder);

	if (result != MA_SUCCESS) {
		MiniAudio_ErrorSetCode(result);
		delete decoder;
		return MINIAUDIO_NO_STREAM;
	}

	ma_sound* sound = new ma_sound();
	result = altsound_ma_sound_init_from_decoder(g_engine, decoder, MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH, sound);
	if (result != MA_SUCCESS) {
		MiniAudio_ErrorSetCode(result);
		altsound_ma_decoder_uninit(decoder);
		delete decoder;
		delete sound;
		return MINIAUDIO_NO_STREAM;
	}

	const bool looping = (flags & MINIAUDIO_SAMPLE_LOOP) != 0;
	altsound_ma_sound_set_looping(sound, looping ? MA_TRUE : MA_FALSE);

	unsigned int hstream = g_nextStreamId++;

	std::lock_guard<std::mutex> lock(g_streamMapMutex);
	g_streamMap[hstream] = {
		.decoder = decoder,
		.sound = sound,
		.playing = false,
		.paused = false,
		.looping = looping,
		.sample_rate = decoder->outputSampleRate,
		.channels = decoder->outputChannels,
		.sync_callback = nullptr,
		.sync_userdata = nullptr
	};

	MiniAudio_ErrorSetCode(MA_SUCCESS);
	return hstream;
}

int MiniAudio_ChannelSetAttribute(unsigned int hstream, unsigned int attrib, float value)
{
	if (hstream == MINIAUDIO_NO_STREAM) {
		MiniAudio_ErrorSetCode(MA_INVALID_ARGS);
		return 0;
	}

	std::lock_guard<std::mutex> lock(g_streamMapMutex);
	auto it = g_streamMap.find(hstream);
	if (it == g_streamMap.end()) {
		MiniAudio_ErrorSetCode(MA_INVALID_ARGS);
		return 0;
	}

	switch (attrib) {
		case MINIAUDIO_ATTRIB_VOL:
			it->second.volume = value;
			if (it->second.sound)
				altsound_ma_sound_set_volume(it->second.sound, value);
			MiniAudio_ErrorSetCode(MA_SUCCESS);
			return 1;
	}
	MiniAudio_ErrorSetCode(MA_ERROR);
	return 0;
}

bool MiniAudio_ChannelGetAttribute(unsigned int hstream, unsigned int attrib, float* value)
{
	if (hstream == MINIAUDIO_NO_STREAM || !value) {
		MiniAudio_ErrorSetCode(MA_INVALID_ARGS);
		return false;
	}

	std::lock_guard<std::mutex> lock(g_streamMapMutex);
	auto it = g_streamMap.find(hstream);
	if (it == g_streamMap.end()) {
		MiniAudio_ErrorSetCode(MA_INVALID_ARGS);
		return false;
	}

	switch (attrib) {
		case MINIAUDIO_ATTRIB_VOL:
			*value = it->second.volume;
			MiniAudio_ErrorSetCode(MA_SUCCESS);
			return true;
	}
	MiniAudio_ErrorSetCode(MA_ERROR);
	return false;
}

unsigned int MiniAudio_ChannelSetSync(unsigned int hstream, unsigned int type, unsigned long long param, void* proc, void* user)
{
	if (hstream == MINIAUDIO_NO_STREAM || !proc) {
		MiniAudio_ErrorSetCode(MA_INVALID_ARGS);
		return 0;
	}

	std::lock_guard<std::mutex> lock(g_streamMapMutex);
	auto it = g_streamMap.find(hstream);
	if (it == g_streamMap.end()) {
		MiniAudio_ErrorSetCode(MA_INVALID_ARGS);
		return 0;
	}

	if (type & MINIAUDIO_SYNC_END) {
		it->second.sync_callback = reinterpret_cast<SYNCPROC>(proc);
		it->second.sync_userdata = user;

		static unsigned int sync_id = 1;
		unsigned int hsync = sync_id++;
		MiniAudio_ErrorSetCode(MA_SUCCESS);
		return hsync;
	}
	MiniAudio_ErrorSetCode(MA_ERROR);
	return 0;
}

int MiniAudio_ChannelPlay(unsigned int hstream, bool restart)
{
	if (hstream == MINIAUDIO_NO_STREAM) {
		MiniAudio_ErrorSetCode(MA_INVALID_ARGS);
		return 0;
	}

	std::lock_guard<std::mutex> lock(g_streamMapMutex);
	auto it = g_streamMap.find(hstream);
	if (it == g_streamMap.end()) {
		MiniAudio_ErrorSetCode(MA_INVALID_ARGS);
		return 0;
	}

	if (restart && it->second.sound) {
		altsound_ma_sound_seek_to_pcm_frame(it->second.sound, 0);
	}

	if (it->second.sound) {
		altsound_ma_sound_set_volume(it->second.sound, it->second.volume);
		altsound_ma_sound_start(it->second.sound);
	}

	it->second.playing = true;
	it->second.paused = false;
	MiniAudio_ErrorSetCode(MA_SUCCESS);
	return 1;
}

int MiniAudio_ChannelPause(unsigned int hstream)
{
	if (hstream == MINIAUDIO_NO_STREAM) {
		MiniAudio_ErrorSetCode(MA_INVALID_ARGS);
		return 0;
	}

	std::lock_guard<std::mutex> lock(g_streamMapMutex);
	auto it = g_streamMap.find(hstream);
	if (it == g_streamMap.end()) {
		MiniAudio_ErrorSetCode(MA_INVALID_ARGS);
		return 0;
	}

	if (it->second.sound)
		altsound_ma_sound_stop(it->second.sound);

	it->second.paused = true;
	MiniAudio_ErrorSetCode(MA_SUCCESS);
	return 1;
}

int MiniAudio_ChannelStop(unsigned int hstream)
{
	if (hstream == MINIAUDIO_NO_STREAM) {
		MiniAudio_ErrorSetCode(MA_INVALID_ARGS);
		return 0;
	}

	std::lock_guard<std::mutex> lock(g_streamMapMutex);
	auto it = g_streamMap.find(hstream);
	if (it == g_streamMap.end()) {
		MiniAudio_ErrorSetCode(MA_INVALID_ARGS);
		return 0;
	}

	if (it->second.sound) {
		altsound_ma_sound_stop(it->second.sound);
		altsound_ma_sound_seek_to_pcm_frame(it->second.sound, 0);
	}

	it->second.playing = false;
	it->second.paused = false;
	MiniAudio_ErrorSetCode(MA_SUCCESS);
	return 1;
}

int MiniAudio_StreamFree(unsigned int hstream)
{
	if (hstream == MINIAUDIO_NO_STREAM) {
		MiniAudio_ErrorSetCode(MA_INVALID_ARGS);
		return 0;
	}

	std::lock_guard<std::mutex> lock(g_streamMapMutex);
	auto it = g_streamMap.find(hstream);
	if (it == g_streamMap.end()) {
		MiniAudio_ErrorSetCode(MA_INVALID_ARGS);
		return 0;
	}

	if (it->second.sound) {
		altsound_ma_sound_uninit(it->second.sound);
		delete it->second.sound;
	}

	if (it->second.decoder) {
		altsound_ma_decoder_uninit(it->second.decoder);
		delete it->second.decoder;
	}

	g_streamMap.erase(it);

	for (int i = 0; i < ALT_MAX_CHANNELS; ++i) {
		if (channel_stream[i] && channel_stream[i]->hstream == hstream) {
			delete channel_stream[i];
			channel_stream[i] = nullptr;
			break;
		}
	}

	MiniAudio_ErrorSetCode(MA_SUCCESS);
	return 1;
}

unsigned int MiniAudio_ChannelIsActive(unsigned int hstream)
{
	if (hstream == MINIAUDIO_NO_STREAM) {
		MiniAudio_ErrorSetCode(MA_INVALID_ARGS);
		return MINIAUDIO_ACTIVE_STOPPED;
	}

	std::lock_guard<std::mutex> lock(g_streamMapMutex);
	auto it = g_streamMap.find(hstream);
	if (it != g_streamMap.end()) {
		const _internal_stream_data& internal = it->second;
		if (internal.paused) {
			MiniAudio_ErrorSetCode(MA_SUCCESS);
			return MINIAUDIO_ACTIVE_PAUSED;
		}
		else if (internal.playing) {
			MiniAudio_ErrorSetCode(MA_SUCCESS);
			return MINIAUDIO_ACTIVE_PLAYING;
		}
	}

	MiniAudio_ErrorSetCode(MA_SUCCESS);
	return MINIAUDIO_ACTIVE_STOPPED;
}