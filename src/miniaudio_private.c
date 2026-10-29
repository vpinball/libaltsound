#define MA_API static
#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#define MA_ENABLE_CUSTOM
#define MA_ENABLE_NULL

#define STB_VORBIS_HEADER_ONLY
#include <miniaudio/extras/stb_vorbis_static.c>

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio/miniaudio.h>

#undef STB_VORBIS_HEADER_ONLY
#include <miniaudio/extras/stb_vorbis_static.c>

ma_result altsound_ma_decoder_init_file(const char* pFilePath, const ma_decoder_config* pConfig, ma_decoder* pDecoder)
{
    return ma_decoder_init_file(pFilePath, pConfig, pDecoder);
}

ma_result altsound_ma_decoder_init_memory(const void* pData, size_t dataSize, const ma_decoder_config* pConfig, ma_decoder* pDecoder)
{
    return ma_decoder_init_memory(pData, dataSize, pConfig, pDecoder);
}

ma_result altsound_ma_decoder_read_pcm_frames(ma_decoder* pDecoder, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    return ma_decoder_read_pcm_frames(pDecoder, pFramesOut, frameCount, pFramesRead);
}

ma_result altsound_ma_decoder_seek_to_pcm_frame(ma_decoder* pDecoder, ma_uint64 frameIndex)
{
    return ma_decoder_seek_to_pcm_frame(pDecoder, frameIndex);
}

ma_result altsound_ma_decoder_get_length_in_pcm_frames(ma_decoder* pDecoder, ma_uint64* pLength)
{
    return ma_decoder_get_length_in_pcm_frames(pDecoder, pLength);
}

void altsound_ma_decoder_uninit(ma_decoder* pDecoder)
{
    ma_decoder_uninit(pDecoder);
}

ma_decoder_config altsound_ma_decoder_config_init(ma_format outputFormat, ma_uint32 outputChannels, ma_uint32 outputSampleRate)
{
    return ma_decoder_config_init(outputFormat, outputChannels, outputSampleRate);
}

ma_result altsound_ma_engine_init_null_device(ma_uint32 channels, ma_uint32 sampleRate, ma_uint32 periodSizeInFrames,
    ma_engine_process_proc onProcess, void* pProcessUserData, ma_context* pContext, ma_engine* pEngine)
{
    // A null device gives us miniAudio's own realtime-paced audio thread (timing,
    // throttling and buffering) without ever touching the hardware. The mixed
    // output is delivered through onProcess and forwarded to the host.
    ma_backend backends[] = { ma_backend_null };
    ma_context_config contextConfig = ma_context_config_init();
    ma_result result = ma_context_init(backends, 1, &contextConfig, pContext);
    if (result != MA_SUCCESS)
        return result;

    ma_engine_config config = ma_engine_config_init();
    config.pContext = pContext;
    config.channels = channels;
    config.sampleRate = sampleRate;
    config.periodSizeInFrames = periodSizeInFrames;
    config.onProcess = onProcess;
    config.pProcessUserData = pProcessUserData;
    config.noAutoStart = MA_TRUE;

    result = ma_engine_init(&config, pEngine);
    if (result != MA_SUCCESS) {
        ma_context_uninit(pContext);
        return result;
    }
    return MA_SUCCESS;
}

void altsound_ma_engine_uninit(ma_engine* pEngine)
{
    ma_engine_uninit(pEngine);
}

void altsound_ma_context_uninit(ma_context* pContext)
{
    ma_context_uninit(pContext);
}

ma_result altsound_ma_engine_start(ma_engine* pEngine)
{
    return ma_engine_start(pEngine);
}

ma_result altsound_ma_engine_stop(ma_engine* pEngine)
{
    return ma_engine_stop(pEngine);
}

ma_result altsound_ma_sound_init_from_decoder(ma_engine* pEngine, ma_decoder* pDecoder, ma_uint32 flags, ma_sound* pSound)
{
    return ma_sound_init_from_data_source(pEngine, (ma_data_source*)pDecoder, flags, NULL, pSound);
}

void altsound_ma_sound_uninit(ma_sound* pSound)
{
    ma_sound_uninit(pSound);
}

ma_result altsound_ma_sound_start(ma_sound* pSound)
{
    return ma_sound_start(pSound);
}

ma_result altsound_ma_sound_stop(ma_sound* pSound)
{
    return ma_sound_stop(pSound);
}

void altsound_ma_sound_set_volume(ma_sound* pSound, float volume)
{
    ma_sound_set_volume(pSound, volume);
}

void altsound_ma_sound_set_looping(ma_sound* pSound, ma_bool32 loop)
{
    ma_sound_set_looping(pSound, loop);
}

ma_result altsound_ma_sound_seek_to_pcm_frame(ma_sound* pSound, ma_uint64 frameIndex)
{
    return ma_sound_seek_to_pcm_frame(pSound, frameIndex);
}

void altsound_ma_sound_set_end_callback(ma_sound* pSound, ma_sound_end_proc callback, void* pUserData)
{
    ma_sound_set_end_callback(pSound, callback, pUserData);
}

ma_result altsound_ma_decoder_set_loop_point(ma_decoder* pDecoder, ma_uint64 loopBeg, ma_uint64 loopEnd)
{
    return ma_data_source_set_loop_point_in_pcm_frames((ma_data_source*)pDecoder, loopBeg, loopEnd);
}

typedef struct {
    ma_bool32 hasStart;
    ma_bool32 hasLength;
    ma_uint64 start;
    ma_uint64 length;
} altsound_loop_tags;

static ma_bool32 altsound_key_equals(const char* pKey, size_t keyLen, const char* pName)
{
    size_t i;
    for (i = 0; i < keyLen; ++i) {
        const char c = (pKey[i] >= 'a' && pKey[i] <= 'z') ? (char)(pKey[i] - 'a' + 'A') : pKey[i];
        if (pName[i] == '\0' || c != pName[i])
            return MA_FALSE;
    }
    return pName[keyLen] == '\0';
}

static void altsound_parse_loop_tag(altsound_loop_tags* pTags, const char* pComment, size_t length)
{
    char buf[64];
    const char* pValue;
    size_t keyLen;

    if (length >= sizeof(buf))
        return;
    memcpy(buf, pComment, length);
    buf[length] = '\0';

    pValue = strchr(buf, '=');
    if (pValue == NULL)
        return;
    keyLen = (size_t)(pValue - buf);
    pValue++;
    if (*pValue < '0' || *pValue > '9')
        return;

    if (altsound_key_equals(buf, keyLen, "LOOPSTART")) {
        pTags->start = strtoull(pValue, NULL, 10);
        pTags->hasStart = MA_TRUE;
    }
    else if (altsound_key_equals(buf, keyLen, "LOOPLENGTH")) {
        pTags->length = strtoull(pValue, NULL, 10);
        pTags->hasLength = MA_TRUE;
    }
}

static void altsound_flac_meta_callback(void* pUserData, ma_dr_flac_metadata* pMetadata)
{
    altsound_loop_tags* pTags = (altsound_loop_tags*)pUserData;
    ma_dr_flac_vorbis_comment_iterator it;
    const char* pComment;
    ma_uint32 length;

    if (pMetadata->type != MA_DR_FLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT)
        return;

    ma_dr_flac_init_vorbis_comment_iterator(&it, pMetadata->data.vorbis_comment.commentCount, pMetadata->data.vorbis_comment.pComments);
    while ((pComment = ma_dr_flac_next_vorbis_comment(&it, &length)) != NULL)
        altsound_parse_loop_tag(pTags, pComment, length);
}

// Loop points of a sample file, in PCM frames at the file's own sample rate:
// the first loop of a WAV "smpl" chunk, or the LOOPSTART/LOOPLENGTH tags of a
// FLAC or Ogg Vorbis file. The loop is [*pLoopBeg, *pLoopEnd), end excluded.
// Returns MA_FALSE when the file has no usable loop.
ma_bool32 altsound_read_loop_points(const char* pFilePath, ma_uint64* pLoopBeg, ma_uint64* pLoopEnd, ma_uint32* pSampleRate)
{
    ma_uint64 beg = 0, end = 0, total = 0;
    ma_uint32 sampleRate = 0;
    ma_bool32 found = MA_FALSE;
    altsound_loop_tags tags = { MA_FALSE, MA_FALSE, 0, 0 };
    ma_dr_wav wav;
    ma_dr_flac* pFlac;
    stb_vorbis* pVorbis;
    int vorbisError = 0;

    if (ma_dr_wav_init_file_with_metadata(&wav, pFilePath, 0, NULL)) {
        ma_uint32 i;
        for (i = 0; i < wav.metadataCount; ++i) {
            const ma_dr_wav_metadata* pMeta = &wav.pMetadata[i];
            if (pMeta->type == ma_dr_wav_metadata_type_smpl && pMeta->data.smpl.sampleLoopCount > 0 && pMeta->data.smpl.pLoops != NULL) {
                const ma_dr_wav_smpl_loop* pLoop = &pMeta->data.smpl.pLoops[0];
                if (pLoop->type == ma_dr_wav_smpl_loop_type_forward) {
                    // the smpl end point is the last frame played in the loop
                    beg = pLoop->firstSampleOffset;
                    end = (ma_uint64)pLoop->lastSampleOffset + 1;
                    found = MA_TRUE;
                }
                break;
            }
        }
        sampleRate = wav.sampleRate;
        total = wav.totalPCMFrameCount;
        ma_dr_wav_uninit(&wav);
    }
    else if ((pFlac = ma_dr_flac_open_file_with_metadata(pFilePath, altsound_flac_meta_callback, &tags, NULL)) != NULL) {
        sampleRate = pFlac->sampleRate;
        total = pFlac->totalPCMFrameCount;
        ma_dr_flac_close(pFlac);
    }
    else if ((pVorbis = stb_vorbis_open_filename(pFilePath, &vorbisError, NULL)) != NULL) {
        const stb_vorbis_comment comments = stb_vorbis_get_comment(pVorbis);
        int i;
        for (i = 0; i < comments.comment_list_length; ++i)
            altsound_parse_loop_tag(&tags, comments.comment_list[i], strlen(comments.comment_list[i]));
        sampleRate = stb_vorbis_get_info(pVorbis).sample_rate;
        total = stb_vorbis_stream_length_in_samples(pVorbis);
        stb_vorbis_close(pVorbis);
    }

    if (!found && tags.hasStart && tags.hasLength) {
        beg = tags.start;
        end = tags.start + tags.length;
        found = MA_TRUE;
    }

    // a loop that does not fit in the file is ignored (whole-file looping)
    if (!found || beg >= end || end > total || sampleRate == 0)
        return MA_FALSE;

    *pLoopBeg = beg;
    *pLoopEnd = end;
    *pSampleRate = sampleRate;
    return MA_TRUE;
}
