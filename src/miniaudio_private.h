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

#ifdef __cplusplus
}
#endif