#pragma once

#include <miniaudio/miniaudio.h>

#ifdef __cplusplus
extern "C" {
#endif

ma_result altsound_ma_decoder_init_file(const char* pFilePath, const ma_decoder_config* pConfig, ma_decoder* pDecoder);
ma_result altsound_ma_decoder_init_memory(const void* pData, size_t dataSize, const ma_decoder_config* pConfig, ma_decoder* pDecoder);
ma_result altsound_ma_decoder_read_pcm_frames(ma_decoder* pDecoder, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
ma_result altsound_ma_decoder_seek_to_pcm_frame(ma_decoder* pDecoder, ma_uint64 frameIndex);
ma_result altsound_ma_decoder_get_length_in_pcm_frames(ma_decoder* pDecoder, ma_uint64* pLength);
void altsound_ma_decoder_uninit(ma_decoder* pDecoder);
ma_decoder_config altsound_ma_decoder_config_init(ma_format outputFormat, ma_uint32 outputChannels, ma_uint32 outputSampleRate);

ma_result altsound_ma_engine_init_null_device(ma_uint32 channels, ma_uint32 sampleRate, ma_uint32 periodSizeInFrames,
    ma_engine_process_proc onProcess, void* pProcessUserData, ma_context* pContext, ma_engine* pEngine);
void altsound_ma_engine_uninit(ma_engine* pEngine);
void altsound_ma_context_uninit(ma_context* pContext);
ma_result altsound_ma_engine_start(ma_engine* pEngine);
ma_result altsound_ma_engine_stop(ma_engine* pEngine);

ma_result altsound_ma_sound_init_from_decoder(ma_engine* pEngine, ma_decoder* pDecoder, ma_uint32 flags, ma_sound* pSound);
void altsound_ma_sound_uninit(ma_sound* pSound);
ma_result altsound_ma_sound_start(ma_sound* pSound);
ma_result altsound_ma_sound_stop(ma_sound* pSound);
void altsound_ma_sound_set_volume(ma_sound* pSound, float volume);
void altsound_ma_sound_set_looping(ma_sound* pSound, ma_bool32 loop);
ma_result altsound_ma_sound_seek_to_pcm_frame(ma_sound* pSound, ma_uint64 frameIndex);
ma_bool32 altsound_ma_sound_at_end(const ma_sound* pSound);

#ifdef __cplusplus
}
#endif