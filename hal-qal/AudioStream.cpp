/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of The Linux Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LOG_TAG "ahal_AudioStream"
#define ATRACE_TAG (ATRACE_TAG_AUDIO|ATRACE_TAG_HAL)
#define LOG_NDEBUG 0
/*#define VERY_VERY_VERBOSE_LOGGING*/
#ifdef VERY_VERY_VERBOSE_LOGGING
#define ALOGVV ALOGV
#else
#define ALOGVV(a...) do { } while(0)
#endif

#include "AudioDevice.h"

#include <log/log.h>

#include "QalApi.h"
#include <audio_effects/effect_aec.h>
#include <audio_effects/effect_ns.h>
#include "audio_extn.h"

#define COMPRESS_OFFLOAD_FRAGMENT_SIZE (32 * 1024)

const std::map<uint32_t, qal_audio_fmt_t> getFormatId {
	{AUDIO_FORMAT_PCM,                 QAL_AUDIO_FMT_DEFAULT_PCM},
	{AUDIO_FORMAT_MP3,                 QAL_AUDIO_FMT_MP3},
	{AUDIO_FORMAT_AAC,                 QAL_AUDIO_FMT_AAC},
	{AUDIO_FORMAT_AAC_ADTS,            QAL_AUDIO_FMT_AAC_ADTS},
	{AUDIO_FORMAT_AAC_ADIF,            QAL_AUDIO_FMT_AAC_ADIF},
	{AUDIO_FORMAT_AAC_LATM,            QAL_AUDIO_FMT_AAC_LATM},
	{AUDIO_FORMAT_WMA,                 QAL_AUDIO_FMT_WMA_STD},
	{AUDIO_FORMAT_ALAC,                QAL_AUDIO_FMT_ALAC},
	{AUDIO_FORMAT_APE,                 QAL_AUDIO_FMT_APE},
	{AUDIO_FORMAT_WMA_PRO,             QAL_AUDIO_FMT_WMA_PRO},
        {AUDIO_FORMAT_FLAC,                QAL_AUDIO_FMT_FLAC}
};

const uint32_t format_to_bitwidth_table[] = {
    [AUDIO_FORMAT_DEFAULT] = 0,
    [AUDIO_FORMAT_PCM_16_BIT] = 16,
    [AUDIO_FORMAT_PCM_8_BIT] = 8,
    [AUDIO_FORMAT_PCM_32_BIT] = 32,
    [AUDIO_FORMAT_PCM_8_24_BIT] = 32,
    [AUDIO_FORMAT_PCM_FLOAT] = sizeof(float) * 8,
    [AUDIO_FORMAT_PCM_24_BIT_PACKED] = 24,
};

const char * const use_case_table[AUDIO_USECASE_MAX] = {
    [USECASE_AUDIO_PLAYBACK_DEEP_BUFFER] = "deep-buffer-playback",
    [USECASE_AUDIO_PLAYBACK_LOW_LATENCY] = "low-latency-playback",
    [USECASE_AUDIO_PLAYBACK_WITH_HAPTICS] = "audio-with-haptics-playback",
    [USECASE_AUDIO_PLAYBACK_ULL]         = "audio-ull-playback",
    [USECASE_AUDIO_PLAYBACK_MULTI_CH]    = "multi-channel-playback",
    [USECASE_AUDIO_PLAYBACK_OFFLOAD] = "compress-offload-playback",
    //Enabled for Direct_PCM
    [USECASE_AUDIO_PLAYBACK_OFFLOAD2] = "compress-offload-playback2",
    [USECASE_AUDIO_PLAYBACK_OFFLOAD3] = "compress-offload-playback3",
    [USECASE_AUDIO_PLAYBACK_OFFLOAD4] = "compress-offload-playback4",
    [USECASE_AUDIO_PLAYBACK_OFFLOAD5] = "compress-offload-playback5",
    [USECASE_AUDIO_PLAYBACK_OFFLOAD6] = "compress-offload-playback6",
    [USECASE_AUDIO_PLAYBACK_OFFLOAD7] = "compress-offload-playback7",
    [USECASE_AUDIO_PLAYBACK_OFFLOAD8] = "compress-offload-playback8",
    [USECASE_AUDIO_PLAYBACK_OFFLOAD9] = "compress-offload-playback9",
    [USECASE_AUDIO_PLAYBACK_FM] = "play-fm",
    [USECASE_AUDIO_PLAYBACK_MMAP] = "mmap-playback",
    [USECASE_AUDIO_PLAYBACK_HIFI] = "hifi-playback",
    [USECASE_AUDIO_PLAYBACK_TTS] = "audio-tts-playback",

    [USECASE_AUDIO_RECORD] = "audio-record",
    [USECASE_AUDIO_RECORD_COMPRESS] = "audio-record-compress",
    [USECASE_AUDIO_RECORD_COMPRESS2] = "audio-record-compress2",
    [USECASE_AUDIO_RECORD_COMPRESS3] = "audio-record-compress3",
    [USECASE_AUDIO_RECORD_COMPRESS4] = "audio-record-compress4",
    [USECASE_AUDIO_RECORD_COMPRESS5] = "audio-record-compress5",
    [USECASE_AUDIO_RECORD_COMPRESS6] = "audio-record-compress6",
    [USECASE_AUDIO_RECORD_LOW_LATENCY] = "low-latency-record",
    [USECASE_AUDIO_RECORD_FM_VIRTUAL] = "fm-virtual-record",
    [USECASE_AUDIO_RECORD_MMAP] = "mmap-record",
    [USECASE_AUDIO_RECORD_HIFI] = "hifi-record",

    [USECASE_AUDIO_HFP_SCO] = "hfp-sco",
    [USECASE_AUDIO_HFP_SCO_WB] = "hfp-sco-wb",
    [USECASE_VOICE_CALL] = "voice-call",

    [USECASE_VOICE2_CALL] = "voice2-call",
    [USECASE_VOLTE_CALL] = "volte-call",
    [USECASE_QCHAT_CALL] = "qchat-call",
    [USECASE_VOWLAN_CALL] = "vowlan-call",
    [USECASE_VOICEMMODE1_CALL] = "voicemmode1-call",
    [USECASE_VOICEMMODE2_CALL] = "voicemmode2-call",
    [USECASE_COMPRESS_VOIP_CALL] = "compress-voip-call",
    [USECASE_INCALL_REC_UPLINK] = "incall-rec-uplink",
    [USECASE_INCALL_REC_DOWNLINK] = "incall-rec-downlink",
    [USECASE_INCALL_REC_UPLINK_AND_DOWNLINK] = "incall-rec-uplink-and-downlink",
    [USECASE_INCALL_REC_UPLINK_COMPRESS] = "incall-rec-uplink-compress",
    [USECASE_INCALL_REC_DOWNLINK_COMPRESS] = "incall-rec-downlink-compress",
    [USECASE_INCALL_REC_UPLINK_AND_DOWNLINK_COMPRESS] = "incall-rec-uplink-and-downlink-compress",

    [USECASE_INCALL_MUSIC_UPLINK] = "incall_music_uplink",
    [USECASE_INCALL_MUSIC_UPLINK2] = "incall_music_uplink2",
    [USECASE_AUDIO_SPKR_CALIB_RX] = "spkr-rx-calib",
    [USECASE_AUDIO_SPKR_CALIB_TX] = "spkr-vi-record",

    [USECASE_AUDIO_PLAYBACK_AFE_PROXY] = "afe-proxy-playback",
    [USECASE_AUDIO_RECORD_AFE_PROXY] = "afe-proxy-record",
    [USECASE_AUDIO_PLAYBACK_SILENCE] = "silence-playback",

    /* Transcode loopback cases */
    [USECASE_AUDIO_TRANSCODE_LOOPBACK_RX] = "audio-transcode-loopback-rx",
    [USECASE_AUDIO_TRANSCODE_LOOPBACK_TX] = "audio-transcode-loopback-tx",

    [USECASE_AUDIO_PLAYBACK_VOIP] = "audio-playback-voip",
    [USECASE_AUDIO_RECORD_VOIP] = "audio-record-voip",
    /* For Interactive Audio Streams */
    [USECASE_AUDIO_PLAYBACK_INTERACTIVE_STREAM1] = "audio-interactive-stream1",
    [USECASE_AUDIO_PLAYBACK_INTERACTIVE_STREAM2] = "audio-interactive-stream2",
    [USECASE_AUDIO_PLAYBACK_INTERACTIVE_STREAM3] = "audio-interactive-stream3",
    [USECASE_AUDIO_PLAYBACK_INTERACTIVE_STREAM4] = "audio-interactive-stream4",
    [USECASE_AUDIO_PLAYBACK_INTERACTIVE_STREAM5] = "audio-interactive-stream5",
    [USECASE_AUDIO_PLAYBACK_INTERACTIVE_STREAM6] = "audio-interactive-stream6",
    [USECASE_AUDIO_PLAYBACK_INTERACTIVE_STREAM7] = "audio-interactive-stream7",
    [USECASE_AUDIO_PLAYBACK_INTERACTIVE_STREAM8] = "audio-interactive-stream8",

    [USECASE_AUDIO_EC_REF_LOOPBACK] = "ec-ref-audio-capture",

    [USECASE_AUDIO_A2DP_ABR_FEEDBACK] = "a2dp-abr-feedback",

    [USECASE_AUDIO_PLAYBACK_MEDIA] = "media-playback",
    [USECASE_AUDIO_PLAYBACK_SYS_NOTIFICATION] = "sys-notification-playback",
    [USECASE_AUDIO_PLAYBACK_NAV_GUIDANCE] = "nav-guidance-playback",
    [USECASE_AUDIO_PLAYBACK_PHONE] = "phone-playback",
    [USECASE_AUDIO_FM_TUNER_EXT] = "fm-tuner-ext",
};

void StreamOutPrimary::GetStreamHandle(audio_stream_out** stream) {
  *stream = (audio_stream_out*)stream_.get();
}

void StreamInPrimary::GetStreamHandle(audio_stream_in** stream) {
  *stream = (audio_stream_in*)stream_.get();
}

uint32_t StreamPrimary::GetSampleRate() {
    return config_.sample_rate;
}

audio_format_t StreamPrimary::GetFormat() {
    return config_.format;
}

uint32_t StreamPrimary::GetChannels() {
    return config_.channel_mask;
}

audio_io_handle_t StreamPrimary::GetHandle()
{
    return handle_;
}

int StreamPrimary::GetUseCase()
{
    return usecase_;
}

#if 0
static qal_stream_type_t GetQalStreamType(audio_output_flags_t flags) {
    std::ignore = flags;
    return QAL_STREAM_LOW_LATENCY;
}
#endif
//audio_hw_device_t* AudioDevice::device_ = NULL;
std::shared_ptr<AudioDevice> AudioDevice::adev_ = nullptr;
std::shared_ptr<audio_hw_device_t> AudioDevice::device_ = nullptr;

AudioDevice::~AudioDevice() {
}

static int32_t qal_callback(qal_stream_handle_t *stream_handle,
                            uint32_t event_id, uint32_t *event_data,
                            void *cookie)
{
    stream_callback_event_t event;
    StreamOutPrimary *astream_out = static_cast<StreamOutPrimary *> (cookie);

    ALOGD("%s: stream_handle (%p), event_id (%x), event_data (%p), cookie (%p)",
          __func__, stream_handle, event_id, event_data, cookie);

    switch (event_id)
    {
        case QAL_STREAM_CBK_EVENT_WRITE_READY:
        {
            std::lock_guard<std::mutex> write_guard (astream_out->write_wait_mutex_);
            astream_out->write_ready_ = true;
            ALOGE("%s: received WRITE_READY event\n",__func__);
            (astream_out->write_condition_).notify_all();
            event = STREAM_CBK_EVENT_WRITE_READY;
        }
        break;

    case QAL_STREAM_CBK_EVENT_DRAIN_READY:
        {
            std::lock_guard<std::mutex> drain_guard (astream_out->drain_wait_mutex_);
            astream_out->drain_ready_ = true;
            ALOGE("%s: received DRAIN_READY event\n",__func__);
            (astream_out->drain_condition_).notify_all();
            event = STREAM_CBK_EVENT_DRAIN_READY;
            }
        break;
    case QAL_STREAM_CBK_EVENT_ERROR:
        event = STREAM_CBK_EVENT_ERROR;
        break;
    default:
        ALOGE("%s: Invalid event id:%d\n",__func__, event_id);
        return -EINVAL;
    }

    if (astream_out && astream_out->client_callback)
        astream_out->client_callback(event, NULL, astream_out->client_cookie);

    return 0;
}


static uint32_t astream_out_get_sample_rate(const struct audio_stream *stream) {
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    std::shared_ptr<StreamOutPrimary> astream_out;

    if (adevice) {
        astream_out = adevice->OutGetStream((audio_stream_t*)stream);
    } else {
        ALOGE("%s: unable to get audio device",__func__);
        return 0;
    }

    if (astream_out)
        return astream_out->GetSampleRate();
    else
        return 0;
}

static int astream_set_sample_rate(struct audio_stream *stream __unused,
                                   uint32_t rate __unused) {
    return -ENOSYS;
}

static audio_format_t astream_out_get_format(
                                const struct audio_stream *stream) {
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    std::shared_ptr<StreamOutPrimary> astream_out;

    if (adevice)
        astream_out = adevice->OutGetStream((audio_stream_t*)stream);
    else
        ALOGE("%s: unable to get audio device",__func__);

    if (astream_out)
        return astream_out->GetFormat();
    else
        return AUDIO_FORMAT_DEFAULT;
}

static int astream_out_get_next_write_timestamp(
                                const struct audio_stream_out *stream __unused,
                                int64_t *timestamp __unused) {
    return -ENOSYS;
}

static int astream_set_format(struct audio_stream *stream __unused,
                              audio_format_t format __unused) {
    return -ENOSYS;
}

static size_t astream_out_get_buffer_size(const struct audio_stream *stream) {
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    std::shared_ptr<StreamOutPrimary> astream_out =
                                    adevice->OutGetStream((audio_stream_t*)stream);

    if (astream_out)
        return astream_out->GetBufferSize();
    else
        return 0;
}

static uint32_t astream_out_get_channels(const struct audio_stream *stream) {

    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    std::shared_ptr<StreamOutPrimary> astream_out;
    ALOGD("%s: stream_out(%p)",__func__, stream);
    if (adevice != nullptr) {
        astream_out = adevice->OutGetStream((audio_stream_t*)stream);
    } else {
        ALOGE("%s: unable to get audio device",__func__);
        return 0;
    }

    if (astream_out != nullptr) {
        return astream_out->GetChannels();
    } else {
        ALOGE("%s: unable to get audio stream",__func__);
        return 0;
    }
}

static int astream_pause(struct audio_stream_out *stream)
{
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    std::shared_ptr<StreamOutPrimary> astream_out;

    if (!adevice) {
        ALOGE("%s: unable to get audio device",__func__);
        return -EINVAL;
    }

    astream_out = adevice->OutGetStream((audio_stream_t*)stream);
    if (!astream_out) {
        ALOGE("%s: unable to get audio stream",__func__);
        return -EINVAL;
    }

    ALOGD("%s: pause",__func__);
    return astream_out->Pause();
}

static int astream_resume(struct audio_stream_out *stream)
{
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    std::shared_ptr<StreamOutPrimary> astream_out;

    if (!adevice) {
        ALOGE("%s: unable to get audio device",__func__);
        return -EINVAL;
    }

    astream_out = adevice->OutGetStream((audio_stream_t*)stream);
    if (!astream_out) {
        ALOGE("%s: unable to get audio stream",__func__);
        return -EINVAL;
    }

    return astream_out->Resume();
}

static int astream_flush(struct audio_stream_out *stream)
{
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    std::shared_ptr<StreamOutPrimary> astream_out;

    if (!adevice) {
        ALOGE("%s: unable to get audio device",__func__);
        return -EINVAL;
    }

    astream_out = adevice->OutGetStream((audio_stream_t*)stream);
    if (!astream_out) {
        ALOGE("%s: unable to get audio stream",__func__);
        return -EINVAL;
    }

    return astream_out->Flush();
}

static int astream_drain(struct audio_stream_out *stream, audio_drain_type_t type)
{
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    std::shared_ptr<StreamOutPrimary> astream_out;

    if (!adevice) {
        ALOGE("%s: unable to get audio device",__func__);
        return -EINVAL;
    }

    astream_out = adevice->OutGetStream((audio_stream_t*)stream);
    if (!astream_out) {
        ALOGE("%s: unable to get audio stream",__func__);
        return -EINVAL;
    }

    return astream_out->Drain(type);
}

static int astream_set_callback(struct audio_stream_out *stream, stream_callback_t callback, void *cookie)
{
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    std::shared_ptr<StreamOutPrimary> astream_out;

    if (!callback) {
        ALOGE("%s: NULL Callback passed",__func__);
        return -EINVAL;
    }

    if (!adevice) {
        ALOGE("%s: unable to get audio device",__func__);
        return -EINVAL;
    }

    astream_out = adevice->OutGetStream((audio_stream_t*)stream);
    if (!astream_out) {
        ALOGE("%s: unable to get audio stream",__func__);
        return -EINVAL;
    }

    astream_out->client_callback = callback;
    astream_out->client_cookie = cookie;

    return 0;
}

static int astream_out_standby(struct audio_stream *stream) {
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    std::shared_ptr<StreamOutPrimary> astream_out;

    if (adevice) {
        astream_out = adevice->OutGetStream((audio_stream_t*)stream);
    } else {
        ALOGE("%s: unable to get audio device",__func__);
        return -EINVAL;
    }

    ALOGD("%s: enter: stream (%p), usecase(%d: %s)", __func__, astream_out.get(),
          astream_out->GetUseCase(), use_case_table[astream_out->GetUseCase()]);

    if (astream_out) {
        return astream_out->Standby();
    } else {
        ALOGE("%s: unable to get audio stream",__func__);
        return -EINVAL;
    }
}

static int astream_dump(const struct audio_stream *stream, int fd) {
    std::ignore = stream;
    std::ignore = fd;
    ALOGD("%s: dump function not implemented",__func__);
    return 0;
}

static uint32_t astream_get_latency(const struct audio_stream_out *stream) {
    std::ignore = stream;
    return LOW_LATENCY_OUTPUT_PERIOD_SIZE;
}

static int astream_out_get_presentation_position(
                               const struct audio_stream_out *stream,
                               uint64_t *frames, struct timespec *timestamp){
    std::ignore = stream;
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    std::shared_ptr<StreamOutPrimary> astream_out;
    int ret = 0;
    if (adevice) {
        astream_out = adevice->OutGetStream((audio_stream_t*)stream);
    } else {
        ALOGE("%s: unable to get audio device",__func__);
        return -EINVAL;
    }
    if (!timestamp) {
       ALOGE("%s: timestamp NULL",__func__);
       return -EINVAL;
    }
    if (astream_out) {
       switch (astream_out->GetQalStreamType(astream_out->flags_)) {
       case QAL_STREAM_COMPRESSED:
          ret = astream_out->GetFrames(frames);
          if (ret != 0) {
             ALOGE("%s: GetTimestamp failed %d",__func__, ret);
             return ret;
          }
          clock_gettime(CLOCK_MONOTONIC, timestamp);
          break;
       default:
          *frames = astream_out->GetFramesWritten(timestamp);
          break;
       }
    } else {
        //ALOGE("%s: unable to get audio stream",__func__);
        return -EINVAL;
    }
    ALOGV("%s: frames %lld played at %lld ",__func__, ((long long) *frames), timestamp->tv_sec * 1000000LL + timestamp->tv_nsec / 1000);

    return ret;
}

static int out_get_render_position(const struct audio_stream_out *stream,
                                   uint32_t *dsp_frames) {
    std::ignore = stream;
    std::ignore = dsp_frames;
    ALOGD("%s: enter",__func__);
    return 0;
}

static int astream_out_set_parameters(struct audio_stream *stream,
                                      const char *kvpairs) {
    int ret = 0;
    struct str_parms *parms;
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    std::shared_ptr<StreamOutPrimary> astream_out;
	if (adevice) {
        astream_out = adevice->OutGetStream((audio_stream_t*)stream);
    } else {
        ret = -EINVAL;
        ALOGE("%s: unable to get audio device",__func__);
        goto exit;
    }

    ALOGD("%s: enter: usecase(%d: %s) kvpairs: %s",
          __func__, astream_out->GetUseCase(), use_case_table[astream_out->GetUseCase()], kvpairs);

    parms = str_parms_create_str(kvpairs);
    if (!parms) {
       ret = -EINVAL;
       goto exit;
    }
    if(astream_out->flags_ ==
            (AUDIO_OUTPUT_FLAG_DIRECT | AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD |
             AUDIO_OUTPUT_FLAG_NON_BLOCKING)) {
       ret = astream_out->SetParameters(parms);
       if (ret) {
          ALOGE("Stream SetParameters Error (%x)", ret);
          goto exit;
       }
    }
exit:
    return ret;
}

static char* astream_out_get_parameters(const struct audio_stream *stream,
                                        const char *keys) {

    int ret = 0;
    struct str_parms *query = str_parms_create_str(keys);
    char value[256];
    char *str = (char*) nullptr;
    std::shared_ptr<StreamOutPrimary> astream_out;
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    struct str_parms *reply = str_parms_create();

    if (adevice) {
        astream_out = adevice->OutGetStream((audio_stream_t*)stream);
    } else {
        ret = -EINVAL;
        ALOGE("%s: unable to get audio device",__func__);
        goto exit;
    }

    if (!query || !reply) {
        if (reply)
            str_parms_destroy(reply);
        if (query)
            str_parms_destroy(query);
        ALOGE("out_get_parameters: failed to allocate mem for query or reply");
        return nullptr;
    }
    ALOGD("%s: keys: %s",__func__,keys);

    ret = str_parms_get_str(query, "is_direct_pcm_track", value, sizeof(value));
    if (ret >= 0) {
        value[0] = '\0';

        if (astream_out->flags_ & AUDIO_OUTPUT_FLAG_DIRECT &&
             !(astream_out->flags_ & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD)) {
            ALOGV("in direct_pcm");
            strlcat(value, "true", sizeof(value));
        } else {
            ALOGV("not in direct_pcm");
            strlcat(value, "false", sizeof(value));
        }
        str_parms_add_str(reply, "is_direct_pcm_track", value);
        if (str)
            free(str);
         str = str_parms_to_str(reply);
    }
exit:
    /* do we need new hooks inside qal? */
    return 0;
}

static int astream_out_set_volume(struct audio_stream_out *stream,
                                  float left, float right) {
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    std::shared_ptr<StreamOutPrimary> astream_out;

    if (adevice) {
        astream_out = adevice->OutGetStream((audio_stream_t*)stream);
    } else {
        ALOGE("%s: unable to get audio device",__func__);
        return -EINVAL;
    }

    if (astream_out) {
        return astream_out->SetVolume(left, right);
    } else {
        ALOGE("%s: unable to get audio stream",__func__);
        return -EINVAL;
    }
}

static int astream_out_add_audio_effect(
                                const struct audio_stream *stream __unused,
                                effect_handle_t effect __unused) {
    return 0;
}

static int astream_out_remove_audio_effect(
                                const struct audio_stream *stream __unused,
                                effect_handle_t effect __unused) {
    return 0;
}

static ssize_t in_read(struct audio_stream_in *stream, void *buffer,
                       size_t bytes) {

    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    std::shared_ptr<StreamInPrimary> astream_in;

    if (adevice) {
        astream_in = adevice->InGetStream((audio_stream_t*)stream);
    } else {
        ALOGE("%s: unable to get audio device",__func__);
        return -EINVAL;
    }

    if (astream_in) {
        return astream_in->Read(buffer, bytes);
    } else {
        ALOGE("%s: unable to get audio stream",__func__);
        return -EINVAL;
    }

    return 0;
}

static ssize_t out_write(struct audio_stream_out *stream, const void *buffer,
                         size_t bytes) {

    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    std::shared_ptr<StreamOutPrimary> astream_out;

    if (adevice) {
        astream_out = adevice->OutGetStream((audio_stream_t*)stream);
    } else {
        ALOGE("%s: unable to get audio device",__func__);
        return -EINVAL;
    }

    if (astream_out) {
        return astream_out->Write(buffer, bytes);
    } else {
        ALOGE("%s: unable to get audio stream",__func__);
        return -EINVAL;
    }

    return 0;
}

static int astream_in_set_microphone_direction(
                        const struct audio_stream_in *stream,
                        audio_microphone_direction_t dir){
    std::ignore = stream;
    std::ignore = dir;
    ALOGD("%s: function not implemented",__func__);
    return 0;
}

static int in_set_microphone_field_dimension(
                        const struct audio_stream_in *stream,
                        float zoom) {
    std::ignore = stream;
    std::ignore = zoom;
    ALOGD("%s: function not implemented",__func__);
    return 0;
}

static int astream_in_add_audio_effect(
                                const struct audio_stream *stream,
                                effect_handle_t effect)
{
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    std::shared_ptr<StreamInPrimary> astream_in;
    return 0;
    if (adevice) {
        astream_in = adevice->InGetStream((audio_stream_t*)stream);
    } else {
        ALOGE("%s: unable to get audio device",__func__);
        return -EINVAL;
    }
    if (astream_in) {
        return astream_in->addRemoveAudioEffect(stream, effect, true);
    } else {
        ALOGE("%s: unable to get audio stream",__func__);
        return -EINVAL;
    }
}

static int astream_in_remove_audio_effect(const struct audio_stream *stream,
                                          effect_handle_t effect)
{
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    std::shared_ptr<StreamInPrimary> astream_in;
    return 0;
    if (adevice) {
        astream_in = adevice->InGetStream((audio_stream_t*)stream);
    } else {
        ALOGE("%s: unable to get audio device",__func__);
        return -EINVAL;
    }
    if (astream_in) {
        return astream_in->addRemoveAudioEffect(stream, effect, false);
    } else {
        ALOGE("%s: unable to get audio stream",__func__);
        return -EINVAL;
    }
}

static int astream_in_get_capture_position(const struct audio_stream_in *stream,
                                           int64_t *frames, int64_t *time) {
    std::ignore = stream;
    std::ignore = frames;
    std::ignore = time;
    ALOGD("%s: position not implemented currently supported in qal",__func__);
    return 0;
}

static uint32_t astream_in_get_input_frames_lost(
                                struct audio_stream_in *stream __unused) {
    return 0;
}

static void in_update_sink_metadata(
                                struct audio_stream_in *stream,
                                const struct sink_metadata *sink_metadata) {
    std::ignore = stream;
    std::ignore = sink_metadata;

    ALOGD("%s: sink meta data update not  supported in qal", __func__);
}

static int astream_in_get_active_microphones(
                        const struct audio_stream_in *stream,
                        struct audio_microphone_characteristic_t *mic_array,
                        size_t *mic_count) {
    std::ignore = stream;
    std::ignore = mic_array;
    std::ignore = mic_count;
    ALOGD("%s: get active mics not currently supported in qal",__func__);
    return 0;
}

static uint32_t astream_in_get_sample_rate(const struct audio_stream *stream) {
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    std::shared_ptr<StreamInPrimary> astream_in;
    ALOGE("%s: Inside",__func__);

    if (adevice) {
        astream_in = adevice->InGetStream((audio_stream_t*)stream);
    } else {
        ALOGE("%s: unable to get audio device",__func__);
        return 0;
    }

    if (astream_in)
        return astream_in->GetSampleRate();
    else
        return 0;
}

static uint32_t astream_in_get_channels(const struct audio_stream *stream) {

    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    std::shared_ptr<StreamInPrimary> astream_in;

    if (adevice) {
        astream_in = adevice->InGetStream((audio_stream_t*)stream);
    } else {
        ALOGE("%s: unable to get audio device",__func__);
        return 0;
    }

    if (astream_in) {
        return astream_in->GetChannels();
    } else {
        ALOGE("%s: unable to get audio stream",__func__);
        return 0;
    }
}

static audio_format_t astream_in_get_format(const struct audio_stream *stream){
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    std::shared_ptr<StreamInPrimary> astream_in;

    if (adevice)
        astream_in = adevice->InGetStream((audio_stream_t*)stream);
    else
        ALOGE("%s: unable to get audio device",__func__);

    if (astream_in)
        return astream_in->GetFormat();
    else
        return AUDIO_FORMAT_DEFAULT;
}

static int astream_in_standby(struct audio_stream *stream) {
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    std::shared_ptr<StreamInPrimary> astream_in;

    if (adevice) {
        astream_in = adevice->InGetStream((audio_stream_t*)stream);
    } else {
        ALOGE("%s: unable to get audio device",__func__);
        return -EINVAL;
    }

    ALOGD("%s: enter: stream (%p) usecase(%d: %s)", __func__, astream_in.get(),
          astream_in->GetUseCase(), use_case_table[astream_in->GetUseCase()]);

    if (astream_in) {
        return astream_in->Standby();
    } else {
        ALOGE("%s: unable to get audio stream",__func__);
        return -EINVAL;
    }
}

static int astream_in_set_parameters(struct audio_stream *stream,
                                     const char *kvpairs) {
    std::ignore = stream;
    std::ignore = kvpairs;
    ALOGD("%s: function not implemented",__func__);
    return 0;
}

static char* astream_in_get_parameters(const struct audio_stream *stream,
                                       const char *keys) {
    std::ignore = stream;
    std::ignore = keys;
    ALOGD("%s: function not implemented",__func__);
    return 0;
}

static int astream_in_set_gain(struct audio_stream_in *stream, float gain) {
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    std::shared_ptr<StreamInPrimary> astream_in;

    if (adevice) {
        astream_in = adevice->InGetStream((audio_stream_t*)stream);
    } else {
        ALOGE("%s: unable to get audio device",__func__);
        return -EINVAL;
    }

    if (astream_in) {
        return astream_in->SetGain(gain);
    } else {
        ALOGE("%s: unable to get audio stream",__func__);
        return -EINVAL;
    }
}

static size_t astream_in_get_buffer_size(const struct audio_stream *stream) {
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    std::shared_ptr<StreamInPrimary> astream_in =
                            adevice->InGetStream((audio_stream_t*)stream);

    if (astream_in)
        return astream_in->GetBufferSize();
    else
        return 0;
}

qal_device_id_t StreamPrimary::GetQalDeviceId(audio_devices_t halDeviceId) {
    qal_device_id_t qalDeviceId = QAL_DEVICE_NONE;
    switch (halDeviceId) {
        case AUDIO_DEVICE_OUT_SPEAKER:
            qalDeviceId = QAL_DEVICE_OUT_SPEAKER;
            break;
        case AUDIO_DEVICE_OUT_EARPIECE:
            qalDeviceId = QAL_DEVICE_OUT_HANDSET;
            break;
        case AUDIO_DEVICE_OUT_WIRED_HEADSET:
            qalDeviceId = QAL_DEVICE_OUT_WIRED_HEADSET;
            break;
        case AUDIO_DEVICE_OUT_WIRED_HEADPHONE:
            qalDeviceId = QAL_DEVICE_OUT_WIRED_HEADPHONE;
            break;
        case AUDIO_DEVICE_OUT_BLUETOOTH_SCO:
            qalDeviceId = QAL_DEVICE_OUT_BLUETOOTH_SCO;
            break;
        case AUDIO_DEVICE_OUT_BLUETOOTH_A2DP:
            qalDeviceId = QAL_DEVICE_OUT_BLUETOOTH_A2DP;
            break;
        case AUDIO_DEVICE_OUT_HDMI:
            qalDeviceId = QAL_DEVICE_OUT_HDMI;
            break;
        case AUDIO_DEVICE_OUT_USB_DEVICE:
            qalDeviceId = QAL_DEVICE_OUT_USB_DEVICE;
            break;
        case AUDIO_DEVICE_OUT_LINE:
            qalDeviceId = QAL_DEVICE_OUT_LINE;
            break;
        case AUDIO_DEVICE_OUT_AUX_LINE:
            qalDeviceId = QAL_DEVICE_OUT_AUX_LINE;
            break;
        case AUDIO_DEVICE_OUT_PROXY:
            qalDeviceId = QAL_DEVICE_OUT_PROXY;
            break;
        case AUDIO_DEVICE_OUT_USB_HEADSET:
            qalDeviceId = QAL_DEVICE_OUT_USB_HEADSET;
            break;
        case AUDIO_DEVICE_OUT_FM:
            qalDeviceId = QAL_DEVICE_OUT_FM;
            break;
        case AUDIO_DEVICE_IN_BUILTIN_MIC:
            qalDeviceId = QAL_DEVICE_IN_HANDSET_MIC;
            break;
        case AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET:
            qalDeviceId = QAL_DEVICE_IN_BLUETOOTH_SCO_HEADSET;
            break;
        case AUDIO_DEVICE_IN_WIRED_HEADSET:
            qalDeviceId = QAL_DEVICE_IN_WIRED_HEADSET;
            break;
        case AUDIO_DEVICE_IN_HDMI:
            qalDeviceId =QAL_DEVICE_IN_HDMI;
            break;
        case AUDIO_DEVICE_IN_BACK_MIC:
            qalDeviceId = QAL_DEVICE_IN_SPEAKER_MIC;
            break;
        case AUDIO_DEVICE_IN_USB_ACCESSORY:
            qalDeviceId = QAL_DEVICE_IN_USB_ACCESSORY;
            break;
        case AUDIO_DEVICE_IN_USB_DEVICE:
            qalDeviceId = QAL_DEVICE_IN_USB_DEVICE;
            break;
        case AUDIO_DEVICE_IN_FM_TUNER:
            qalDeviceId = QAL_DEVICE_IN_FM_TUNER;
            break;
        case AUDIO_DEVICE_IN_LINE:
            qalDeviceId = QAL_DEVICE_IN_LINE;
            break;
        case AUDIO_DEVICE_IN_SPDIF:
            qalDeviceId = QAL_DEVICE_IN_SPDIF;
            break;
        case AUDIO_DEVICE_IN_PROXY:
            qalDeviceId = QAL_DEVICE_IN_PROXY;
            break;
        case AUDIO_DEVICE_IN_USB_HEADSET:
            qalDeviceId = QAL_DEVICE_IN_USB_HEADSET;
            break;
        default:
            qalDeviceId = QAL_DEVICE_NONE;
            ALOGE("%s: unsupported Device Id of %d\n", __func__, halDeviceId);
            break;
     }

     return qalDeviceId;
}

qal_stream_type_t StreamInPrimary::GetQalStreamType(
                                        audio_input_flags_t halStreamFlags) {
    qal_stream_type_t qalStreamType = QAL_STREAM_LOW_LATENCY;
    if ((halStreamFlags & AUDIO_INPUT_FLAG_VOIP_TX)!=0){
         qalStreamType = QAL_STREAM_VOIP_TX;
         return qalStreamType;
    }
    switch (halStreamFlags) {
        case AUDIO_INPUT_FLAG_FAST:
        case AUDIO_INPUT_FLAG_MMAP_NOIRQ:
            qalStreamType = QAL_STREAM_LOW_LATENCY;
            break;
        case AUDIO_INPUT_FLAG_RAW:
        case AUDIO_INPUT_FLAG_DIRECT:
            qalStreamType = QAL_STREAM_RAW;
            break;
        case AUDIO_INPUT_FLAG_VOIP_TX:
            qalStreamType = QAL_STREAM_VOIP_TX;
            break;
        default:
            /*
            unsupported from QAL
            AUDIO_INPUT_FLAG_NONE        = 0x0,
            AUDIO_INPUT_FLAG_HW_HOTWORD = 0x2,
            AUDIO_INPUT_FLAG_SYNC        = 0x8,
            AUDIO_INPUT_FLAG_HW_AV_SYNC = 0x40,
            */
            ALOGE("%s: flag %#x is not supported from QAL.\n" ,
                      __func__, halStreamFlags);
            break;
    }

    return qalStreamType;
}

qal_stream_type_t StreamOutPrimary::GetQalStreamType(
                                    audio_output_flags_t halStreamFlags) {
    qal_stream_type_t qalStreamType = QAL_STREAM_LOW_LATENCY;
     if ((halStreamFlags & AUDIO_OUTPUT_FLAG_VOIP_RX)!=0){
         qalStreamType = QAL_STREAM_VOIP_RX;
         return qalStreamType;
    }
    if ((halStreamFlags & AUDIO_OUTPUT_FLAG_FAST) != 0) {
        qalStreamType = QAL_STREAM_LOW_LATENCY;
    } else if (halStreamFlags ==
                    (AUDIO_OUTPUT_FLAG_FAST|AUDIO_OUTPUT_FLAG_RAW)) {
        qalStreamType = QAL_STREAM_RAW;
    } else if (halStreamFlags == AUDIO_OUTPUT_FLAG_DEEP_BUFFER) {
        qalStreamType = QAL_STREAM_DEEP_BUFFER;
    } else if (halStreamFlags ==
                    (AUDIO_OUTPUT_FLAG_DIRECT|AUDIO_OUTPUT_FLAG_MMAP_NOIRQ)) {
        // mmap_no_irq_out: to be confirmed
        qalStreamType = QAL_STREAM_LOW_LATENCY;
    } else if (halStreamFlags == (AUDIO_OUTPUT_FLAG_DIRECT|
                                      AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD|
                                  AUDIO_OUTPUT_FLAG_NON_BLOCKING)) {
        // hifi: to be confirmed
        qalStreamType = QAL_STREAM_COMPRESSED;
    } else if (halStreamFlags == AUDIO_OUTPUT_FLAG_DIRECT) {
        qalStreamType = QAL_STREAM_PCM_OFFLOAD;
    } else if (halStreamFlags == (AUDIO_OUTPUT_FLAG_DIRECT|
                                      AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD|
                                  AUDIO_OUTPUT_FLAG_NON_BLOCKING)) {
        qalStreamType = QAL_STREAM_COMPRESSED;
    } else if (halStreamFlags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) {
        // dsd_compress_passthrough
        qalStreamType = QAL_STREAM_COMPRESSED;
    } else if (halStreamFlags == (AUDIO_OUTPUT_FLAG_DIRECT|
                                      AUDIO_OUTPUT_FLAG_VOIP_RX)) {
        // voice rx
        qalStreamType = QAL_STREAM_VOICE_CALL_RX;
    } else if (halStreamFlags == AUDIO_OUTPUT_FLAG_VOIP_RX) {
        qalStreamType = QAL_STREAM_VOIP_RX;
    } else if (halStreamFlags == AUDIO_OUTPUT_FLAG_INCALL_MUSIC) {
        // incall_music_uplink
        qalStreamType = QAL_STREAM_VOICE_CALL_MUSIC;
    } else {
        qalStreamType = QAL_STREAM_GENERIC;
    }
    return qalStreamType;
}

int StreamOutPrimary::FillHalFnPtrs() {
    int ret = 0;

    stream_.get()->common.get_sample_rate = astream_out_get_sample_rate;
    stream_.get()->common.set_sample_rate = astream_set_sample_rate;
    stream_.get()->common.get_buffer_size = astream_out_get_buffer_size;
    stream_.get()->common.get_channels = astream_out_get_channels;
    stream_.get()->common.get_format = astream_out_get_format;
    stream_.get()->common.set_format = astream_set_format;
    stream_.get()->common.standby = astream_out_standby;
    stream_.get()->common.dump = astream_dump;
    stream_.get()->common.set_parameters = astream_out_set_parameters;
    stream_.get()->common.get_parameters = astream_out_get_parameters;
    stream_.get()->common.add_audio_effect = astream_out_add_audio_effect;
    stream_.get()->common.remove_audio_effect =
                                            astream_out_remove_audio_effect;
    stream_.get()->get_latency = astream_get_latency;
    stream_.get()->set_volume = astream_out_set_volume;
    stream_.get()->write = out_write;
    stream_.get()->get_render_position = out_get_render_position;
    stream_.get()->get_next_write_timestamp =
                                            astream_out_get_next_write_timestamp;
    stream_.get()->get_presentation_position =
                                            astream_out_get_presentation_position;
    stream_.get()->update_source_metadata = NULL;
    stream_.get()->pause = astream_pause;
    stream_.get()->resume = astream_resume;
    stream_.get()->drain = astream_drain;
    stream_.get()->flush = astream_flush;
    stream_.get()->set_callback = astream_set_callback;
    return ret;
}

int StreamOutPrimary::Pause() {
    int ret = 0;

    if (qal_stream_handle_) {
        ret = qal_stream_pause(qal_stream_handle_);
    }
    if (ret)
        return -EINVAL;
    else
        return ret;
}

int StreamOutPrimary::Resume() {
    int ret = 0;

    if (qal_stream_handle_) {
        ret = qal_stream_resume(qal_stream_handle_);
    }
    if (ret)
        return -EINVAL;
    else
        return ret;
}

int StreamOutPrimary::Flush() {
    int ret = 0;
    ALOGD("%s: Enter \n", __func__);
    if (qal_stream_handle_) {
        ret = qal_stream_flush(qal_stream_handle_);
        if (!ret)
            ret = qal_stream_resume(qal_stream_handle_);
    }

    if (ret)
        return -EINVAL;
    else
        return ret;
}

int StreamOutPrimary::Drain(audio_drain_type_t type) {
    int ret = 0;
    qal_drain_type_t qalDrainType;

    switch (type) {
      case AUDIO_DRAIN_ALL:
           qalDrainType = QAL_DRAIN;
           break;
      case AUDIO_DRAIN_EARLY_NOTIFY:
           qalDrainType = QAL_DRAIN_PARTIAL;
           break;
    default:
           ALOGE("%s: Invalid drain type:%d\n", __func__, type);
           return -EINVAL;
    }

    if (qal_stream_handle_)
        ret = qal_stream_drain(qal_stream_handle_, qalDrainType);

    if (ret) {
        ALOGE("%s: Invalid drain type:%d\n", __func__, type);
    }

    return ret;
}

int StreamOutPrimary::Standby() {
    int ret = 0;

    if (qal_stream_handle_) {
        ret = qal_stream_stop(qal_stream_handle_);
        if (ret) {
            ALOGE("%s: failed to stop stream.\n", __func__);
            return -EINVAL;
        }
    }

    stream_started_ = false;
    if (streamAttributes_.type == QAL_STREAM_COMPRESSED)
        ret = StopOffloadEffects(handle_, qal_stream_handle_);

    if (qal_stream_handle_) {
        ret = qal_stream_close(qal_stream_handle_);
        qal_stream_handle_ = NULL;
    }

    if (ret)
        return -EINVAL;
    else
        return ret;
}

int StreamOutPrimary::SetParameters(struct str_parms *parms) {
   int ret = -EINVAL;
   ALOGE("%s: g\n", __func__);

   ret = AudioExtn::audio_extn_parse_compress_metadata(&config_, &qparam_payload, parms, &msample_rate, &mchannels);
   if (ret) {
      ALOGE("parse_compress_metadata Error (%x)", ret);
   }
   ALOGE("%s: exit %d\n", __func__, ret);
   return ret;
}

int StreamOutPrimary::SetVolume(float left , float right) {
    int ret = 0;
    ALOGE("%s: g\n", __func__);

    /* free previously cached volume if any */
    if (volume_) {
        free(volume_);
        volume_ = NULL;
    }

    if (left == right) {
        volume_ = (struct qal_volume_data *)malloc(sizeof(struct qal_volume_data)
                    +sizeof(struct qal_channel_vol_kv));
        volume_->no_of_volpair = 1;
        volume_->volume_pair[0].channel_mask = 0x03;
        volume_->volume_pair[0].vol = left;
    } else {
        volume_ = (struct qal_volume_data *)malloc(sizeof(struct qal_volume_data)
                    +sizeof(struct qal_channel_vol_kv) * 2);
        volume_->no_of_volpair = 2;
        volume_->volume_pair[0].channel_mask = 0x01;
        volume_->volume_pair[0].vol = left;
        volume_->volume_pair[1].channel_mask = 0x10;
        volume_->volume_pair[1].vol = right;
    }

    /* if stream is not opened already cache the volume and set on open */
    if (qal_stream_handle_) {
        ret = qal_stream_set_volume(qal_stream_handle_, volume_);
        if (ret) {
            ALOGE("Qal Stream volume Error (%x)", ret);
        }
    }
    return ret;
}

/* Delay in Us */
/* Delay in Us, only to be used for PCM formats */
int64_t StreamOutPrimary::platform_render_latency(audio_output_flags_t flags_)
{
    struct qal_stream_attributes streamAttributes_;
    streamAttributes_.type = StreamOutPrimary::GetQalStreamType(flags_);
    ALOGE("%s:%d type %d", __func__, __LINE__, streamAttributes_.type);
    switch (streamAttributes_.type) {
         case QAL_STREAM_DEEP_BUFFER:
             return DEEP_BUFFER_PLATFORM_DELAY;
         case QAL_STREAM_LOW_LATENCY:
             return LOW_LATENCY_PLATFORM_DELAY;
         case QAL_STREAM_COMPRESSED:
         case QAL_STREAM_PCM_OFFLOAD:
              return PCM_OFFLOAD_PLATFORM_DELAY;
    //TODO: Add more usecases/type as in current hal, once they are available in qal
         default:
             return 0;
     }
}

int64_t StreamOutPrimary::GetFramesWritten(struct timespec *timestamp)
{
    int64_t signed_frames = 0;
    int64_t written = 0;
    if (!qal_stream_handle_) {
       //ALOGE("%s: qal_stream_handle_ NULL",__func__);
       return 0;
    }
    if (!timestamp) {
       ALOGE("%s: timestamp NULL",__func__);
       return 0;
    }
    written = total_bytes_written_/audio_bytes_per_frame(
        audio_channel_count_from_out_mask(config_.channel_mask), config_.format);
    ALOGE("%s: total_bytes_written_ %lld, written %lld",__func__, ((long long) total_bytes_written_), ((long long) written));
    signed_frames = written; //- kernel_buffer_size + avail;
    signed_frames -= (platform_render_latency(flags_) * (streamAttributes_.out_media_config.sample_rate) / 1000000LL);
    
    if (signed_frames < 0) {
       ALOGE("%s: signed_frames -ve %lld",__func__, ((long long) signed_frames));
       clock_gettime(CLOCK_MONOTONIC, timestamp);
       signed_frames = 0;
    } else {
       *timestamp = writeAt;
    }
    return signed_frames;
}

int StreamOutPrimary::get_compressed_buffer_size()
{
	ALOGE("%s:%d config_ %x", __func__, __LINE__, config_.format);
    return COMPRESS_OFFLOAD_FRAGMENT_SIZE;
}

int StreamOutPrimary::get_pcm_offload_buffer_size()
{
    uint8_t channels = audio_channel_count_from_out_mask(config_.channel_mask);
    uint8_t bytes_per_sample = audio_bytes_per_sample(config_.format);
    uint32_t fragment_size = 0;

    ALOGE("%s:%d config_ format:%x, SR %d ch_mask 0x%x",
            __func__, __LINE__, config_.format, config_.sample_rate,
            config_.channel_mask);
    fragment_size = PCM_OFFLOAD_BUFFER_DURATION *
        config_.sample_rate * bytes_per_sample * channels;
    fragment_size /= 1000;

    if (fragment_size < MIN_PCM_OFFLOAD_FRAGMENT_SIZE)
        fragment_size = MIN_PCM_OFFLOAD_FRAGMENT_SIZE;
    else if (fragment_size > MAX_PCM_OFFLOAD_FRAGMENT_SIZE)
        fragment_size = MAX_PCM_OFFLOAD_FRAGMENT_SIZE;

    fragment_size = ALIGN(fragment_size, (bytes_per_sample * channels * 32));

    ALOGE("%s: fragment size: %d", __func__, fragment_size);
    return fragment_size;
}

static int voip_get_buffer_size(uint32_t sample_rate)
{
    if (sample_rate == 48000)
        return COMPRESS_VOIP_IO_BUF_SIZE_FB;
    else if (sample_rate == 32000)
        return COMPRESS_VOIP_IO_BUF_SIZE_SWB;
    else if (sample_rate == 16000)
        return COMPRESS_VOIP_IO_BUF_SIZE_WB;
    else
        return COMPRESS_VOIP_IO_BUF_SIZE_NB;

}

uint32_t StreamOutPrimary::GetBufferSize() {
    struct qal_stream_attributes streamAttributes_;

    streamAttributes_.type = StreamOutPrimary::GetQalStreamType(flags_);
    ALOGE("%s:%d type %d", __func__, __LINE__, streamAttributes_.type);
    if (streamAttributes_.type == QAL_STREAM_VOIP_RX) {
        return voip_get_buffer_size(config_.sample_rate);
    } else if (streamAttributes_.type == QAL_STREAM_COMPRESSED) {
        return get_compressed_buffer_size();
    } else if (streamAttributes_.type == QAL_STREAM_PCM_OFFLOAD) {
        return get_pcm_offload_buffer_size();
    } else {
       return BUF_SIZE_PLAYBACK * NO_OF_BUF;
    }
}

int StreamOutPrimary::Open() {
    int ret = -EINVAL;
    uint8_t channels = 0;
    struct qal_device qalDevice;
    struct qal_channel_info *ch_info = NULL;
    uint32_t inBufSize = 0;
    uint32_t outBufSize = 0;
    uint32_t inBufCount = NO_OF_BUF;
    uint32_t outBufCount = NO_OF_BUF;
    memset(&qalDevice, 0, sizeof(qalDevice));
/*
    ret = qal_init();
    if ( ret ) {
      ALOGD("%s:(%d) qal_init failed ret=(%d)",__func__,__LINE__, ret);
      return -EINVAL;
    }
*/

    /* TODO: Update channels based on device */
    qalDevice.id = qal_device_id_; //To-Do: convert into QAL Device
    qalDevice.config.sample_rate = config_.sample_rate;

    channels = audio_channel_count_from_out_mask(config_.channel_mask);
    ch_info = (struct qal_channel_info *)calloc(
                            1, sizeof(uint16_t) + sizeof(uint8_t)*channels);
    if (ch_info == NULL) {
      ALOGE("Allocation failed for channel map");
      ret = -ENOMEM;
      goto error_open;
    }

    ch_info->channels = channels;
    ch_info->ch_map[0] = QAL_CHMAP_CHANNEL_FL;
    if (ch_info->channels > 1 )
      ch_info->ch_map[1] = QAL_CHMAP_CHANNEL_FR;

    streamAttributes_.type = StreamOutPrimary::GetQalStreamType(flags_);
    streamAttributes_.flags = (qal_stream_flags_t)flags_;
    streamAttributes_.direction = QAL_AUDIO_OUTPUT;
    streamAttributes_.out_media_config.sample_rate = config_.sample_rate;
    streamAttributes_.out_media_config.bit_width = CODEC_BACKEND_DEFAULT_BIT_WIDTH;
    streamAttributes_.out_media_config.aud_fmt_id = QAL_AUDIO_FMT_DEFAULT_PCM;
    streamAttributes_.out_media_config.ch_info = ch_info;

    if (streamAttributes_.type == QAL_STREAM_COMPRESSED) {
       streamAttributes_.flags = (qal_stream_flags_t)(1 << QAL_STREAM_FLAG_NON_BLOCKING);
       if (config_.offload_info.format == 0)
          config_.offload_info.format = config_.format;
       if (config_.offload_info.sample_rate == 0)
          config_.offload_info.sample_rate = config_.sample_rate;
       streamAttributes_.out_media_config.sample_rate = config_.offload_info.sample_rate;
       if (msample_rate)
          streamAttributes_.out_media_config.sample_rate = msample_rate;
       if (mchannels)
          streamAttributes_.out_media_config.ch_info->channels = mchannels;
       streamAttributes_.out_media_config.aud_fmt_id = getFormatId.at(config_.format & AUDIO_FORMAT_MAIN_MASK);
    } else if (streamAttributes_.type == QAL_STREAM_PCM_OFFLOAD) {
        streamAttributes_.out_media_config.bit_width = format_to_bitwidth_table[config_.format];
        if (streamAttributes_.out_media_config.bit_width == 0)
            streamAttributes_.out_media_config.bit_width = 16;
    }
    ALOGE("channels %d samplerate %d format id %d \n",
            streamAttributes_.out_media_config.ch_info->channels,
            streamAttributes_.out_media_config.sample_rate,
          streamAttributes_.out_media_config.aud_fmt_id);
    ALOGE("chanels %d \n", streamAttributes_.out_media_config.ch_info->channels);
    ALOGE("msample_rate %d mchannels %d \n", msample_rate, mchannels);
    ret = qal_stream_open (&streamAttributes_,
                          1,
                          &qalDevice,
                          0,
                          NULL,
                          &qal_callback,
                          (void *)this,
                          &qal_stream_handle_);

    ALOGD("%s:(%x:ret)%d",__func__,ret, __LINE__);
    if (ret) {
        ALOGE("Qal Stream Open Error (%x)", ret);
        ret = -EINVAL;
    }
    if (streamAttributes_.type == QAL_STREAM_COMPRESSED) {
       ret = qal_stream_set_param(qal_stream_handle_, 0, &qparam_payload);
       if (ret) {
          ALOGE("Qal Set Param Error (%x)\n", ret);
       }
    }

    outBufSize = StreamOutPrimary::GetBufferSize();
//    if (streamAttributes_.type != QAL_STREAM_VOIP_RX) {
 //       outBufSize = outBufSize/NO_OF_BUF;
 //   }
    ret = qal_stream_set_buffer_size(qal_stream_handle_,(size_t*)&inBufSize,inBufCount,(size_t*)&outBufSize,outBufCount);
    if (ret) {
        ALOGE("Qal Stream set buffer size Error  (%x)", ret);
    }

error_open:
    if (ch_info)
        free(ch_info);
    return ret;
}


int StreamOutPrimary::GetFrames(uint64_t *frames) {
    int ret = 0;
    if (!qal_stream_handle_) {
       ALOGE("%s: qal_stream_handle_ NULL",__func__);
       *frames = 0;
       return 0;
    }
    qal_session_time tstamp;
    uint64_t timestamp = 0;
    ret = qal_get_timestamp(qal_stream_handle_, &tstamp);
    if (ret != 0){
       ALOGE("%s: qal_get_timestamp failed %d",__func__, ret);
       goto exit;
    }
    timestamp = (uint64_t)tstamp.session_time.value_msw;
    timestamp = timestamp  << 32 | tstamp.session_time.value_lsw;
    ALOGE("%s: session msw %u",__func__, tstamp.session_time.value_msw);
    ALOGE("%s: session lsw %u",__func__, tstamp.session_time.value_lsw);
    ALOGE("%s: session timespec %lld",__func__, ((long long) timestamp));
    timestamp *= (streamAttributes_.out_media_config.sample_rate);
    ALOGE("%s: timestamp %lld",__func__, ((long long) timestamp));
    *frames = timestamp/1000000;
exit:
    return ret;
}

int StreamOutPrimary::GetOutputUseCase(audio_output_flags_t halStreamFlags)
{
    // TODO: just covered current supported usecases in QAL
    // need to update other usecases in future
    int usecase = USECASE_AUDIO_PLAYBACK_LOW_LATENCY;
    if (halStreamFlags & AUDIO_OUTPUT_FLAG_VOIP_RX)
        usecase = USECASE_AUDIO_PLAYBACK_VOIP;
    else if ((halStreamFlags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) ||
             (halStreamFlags == AUDIO_OUTPUT_FLAG_DIRECT)) {
        if (halStreamFlags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD)
            usecase = USECASE_AUDIO_PLAYBACK_OFFLOAD;
        else
            usecase = USECASE_AUDIO_PLAYBACK_OFFLOAD2;
    } else if (halStreamFlags & AUDIO_OUTPUT_FLAG_RAW)
        usecase = USECASE_AUDIO_PLAYBACK_ULL;
    else if (halStreamFlags & AUDIO_OUTPUT_FLAG_FAST)
        usecase = USECASE_AUDIO_PLAYBACK_LOW_LATENCY;
    else if (halStreamFlags & AUDIO_OUTPUT_FLAG_DEEP_BUFFER)
        usecase = USECASE_AUDIO_PLAYBACK_DEEP_BUFFER;

    return usecase;
}

ssize_t StreamOutPrimary::Write(const void *buffer, size_t bytes){
    int ret = 0;
    struct qal_buffer qalBuffer;
    int local_bytes_written = 0;

    qalBuffer.buffer = (void*)buffer;
    qalBuffer.size = bytes;
    qalBuffer.offset = 0;

    ALOGD("%s: handle_ %x Bytes:(%zu)",__func__,handle_, bytes);
    if (!qal_stream_handle_){
        ret = Open();
        if (ret) {
            ALOGE("%s: failed to open stream.\n", __func__);
            return -EINVAL;
        }
    }

    if (!stream_started_) {
        ret = qal_stream_start(qal_stream_handle_);
        if (ret) {
            ALOGE("%s:failed to start stream. ret=%d\n", __func__, ret);
            qal_stream_close(qal_stream_handle_);
            qal_stream_handle_ = NULL;
            return -EINVAL;
        }

        stream_started_ = true;

        if (streamAttributes_.type == QAL_STREAM_COMPRESSED) {
            ret = StartOffloadEffects(handle_, qal_stream_handle_);
        }

        /* set cached volume if any, dont return failure back up */
        if (volume_) {
            ret = qal_stream_set_volume(qal_stream_handle_, volume_);
            if (ret) {
                ALOGE("Qal Stream volume Error (%x)", ret);
            }
        }
    }

    local_bytes_written = qal_stream_write(qal_stream_handle_, &qalBuffer);
    total_bytes_written_ += local_bytes_written;
    clock_gettime(CLOCK_MONOTONIC, &writeAt);
    return local_bytes_written;
}

int StreamOutPrimary::StartOffloadEffects(
                                    audio_io_handle_t ioHandle,
                                    qal_stream_handle_t* qal_stream_handle) {
    int ret  = 0;
    if (fnp_offload_effect_start_output_) {
        ret = fnp_offload_effect_start_output_(ioHandle, qal_stream_handle);
        if (ret) {
            ALOGE("%s: failed to start offload effect.\n", __func__);
        }
    } else {
        ALOGE("%s: function pointer is null.\n", __func__);
        return -EINVAL;
    }

    return ret;
}

int StreamOutPrimary::StopOffloadEffects(
                                    audio_io_handle_t ioHandle,
                                    qal_stream_handle_t* qal_stream_handle) {
    int ret  = 0;
    if (fnp_offload_effect_stop_output_) {
        ret = fnp_offload_effect_stop_output_(ioHandle, qal_stream_handle);
        if (ret) {
            ALOGE("%s: failed to start offload effect.\n", __func__);
        }
    } else {
        ALOGE("%s: function pointer is null.\n", __func__);
        return -EINVAL;
    }

    return ret;
}

StreamOutPrimary::StreamOutPrimary(
                        audio_io_handle_t handle,
                        audio_devices_t devices,
                        audio_output_flags_t flags,
                        struct audio_config *config,
                        const char *address,
                        offload_effects_start_output start_offload_effect,
                        offload_effects_stop_output stop_offload_effect):
    StreamPrimary(handle, devices, config),
    flags_(flags)
{
    stream_ = std::shared_ptr<audio_stream_out> (new audio_stream_out());
    if (!stream_) {
        ALOGE("%s: No memory allocated for stream_",__func__);
    }

    handle_ = handle;
    qal_device_id_ = GetQalDeviceId(devices);
    flags_ = flags;
    usecase_ = GetOutputUseCase(flags);

    if (config) {
        ALOGD("%s: enter: handle (%x) format(%#x) sample_rate(%d)\
            channel_mask(%#x) devices(%#x) flags(%#x) address(%s)",
            __func__, handle, config->format, config->sample_rate,
            config->channel_mask, devices, flags, address);
        memcpy(&config_, config, sizeof(struct audio_config));
    } else {
        ALOGD("%s: enter: devices(%#x) flags(%#x)", __func__,devices, flags);
    }

    if (address) {
        strlcpy((char *)&address_, address, AUDIO_DEVICE_MAX_ADDRESS_LEN);
    } else {
        ALOGD("%s: invalid address", __func__);
    }

    fnp_offload_effect_start_output_ = start_offload_effect;
    fnp_offload_effect_stop_output_ = stop_offload_effect;
    writeAt.tv_sec = 0;
    writeAt.tv_nsec = 0;
    total_bytes_written_ = 0;

    (void)FillHalFnPtrs();
}

StreamOutPrimary::~StreamOutPrimary() {
    ALOGD("%s: close stream, handle(%x), qal_stream_handle (%p)", __func__,
          handle_, qal_stream_handle_);

    if (qal_stream_handle_) {
        if (streamAttributes_.type == QAL_STREAM_COMPRESSED) {
            StopOffloadEffects(handle_, qal_stream_handle_);
        }
    }

    qal_stream_close(qal_stream_handle_);
    qal_stream_handle_ = nullptr;
}

int StreamInPrimary::Standby() {
    int ret = 0;

    if (is_st_session && is_st_session_active) {
        audio_extn_sound_trigger_stop_lab(this);
        return ret;
    }

    if (qal_stream_handle_) {
        ret = qal_stream_stop(qal_stream_handle_);
    }

    stream_started_ = false;

    if (qal_stream_handle_) {
        ret = qal_stream_close(qal_stream_handle_);
        qal_stream_handle_ = NULL;
    }

    if (ret)
        return -EINVAL;
    else
        return ret;
}

int StreamInPrimary::addRemoveAudioEffect(const struct audio_stream *stream __unused,
                                   effect_handle_t effect,
                                   bool enable)
{
    int status = 0;
    effect_descriptor_t desc;

    status = (*effect)->get_descriptor(effect, &desc);
    if (status != 0)
        return status;


    if (source_ == AUDIO_SOURCE_VOICE_COMMUNICATION) {

        if (memcmp(&desc.type, FX_IID_AEC, sizeof(effect_uuid_t)) == 0) {
            if (enable) {
                if (isECEnabled) {
                    ALOGE("%s: EC already enabled", __func__);
                    status  = -EINVAL;
                    goto exit;
                } else if (isNSEnabled) {
                    ALOGV("%s: Got EC enable and NS is already active. Enabling ECNS", __func__);
                    status = qal_add_remove_effect(qal_stream_handle_,QAL_AUDIO_EFFECT_ECNS,true);
                    isECEnabled = true;
                    goto exit;
                } else {
                    ALOGV("%s: Got EC enable. Enabling EC", __func__);
                    status = qal_add_remove_effect(qal_stream_handle_,QAL_AUDIO_EFFECT_EC,true);
                    isECEnabled = true;
                    goto exit;
               }
            } else {
                if (isECEnabled) {
                    if (isNSEnabled) {
                        ALOGV("%s: ECNS is running. Disabling EC and enabling NS alone", __func__);
                        status = qal_add_remove_effect(qal_stream_handle_,QAL_AUDIO_EFFECT_NS,true);
                        isECEnabled = false;
                        goto exit;
                    } else {
                        ALOGV("%s: EC is running. Disabling it", __func__);
                        status = qal_add_remove_effect(qal_stream_handle_,QAL_AUDIO_EFFECT_EC,false);
                        isECEnabled = false;
                        goto exit;
                    }
                } else {
                    ALOGE("%s: EC is not enabled", __func__);
                    status = -EINVAL;
                    goto exit;
               }
            }
        }

        if (memcmp(&desc.type, FX_IID_NS, sizeof(effect_uuid_t)) == 0) {
            if (enable) {
                if (isNSEnabled) {
                    ALOGE("%s: NS already enabled", __func__);
                    status  = -EINVAL;
                    goto exit;
                } else if (isECEnabled) {
                    ALOGV("%s: Got NS enable and EC is already active. Enabling ECNS", __func__);
                    status = qal_add_remove_effect(qal_stream_handle_,QAL_AUDIO_EFFECT_ECNS,true);
                    isNSEnabled = true;
                    goto exit;
                } else {
                    ALOGV("%s: Got NS enable. Enabling NS", __func__);
                    status = qal_add_remove_effect(qal_stream_handle_,QAL_AUDIO_EFFECT_NS,true);
                    isNSEnabled = true;
                    goto exit;
               }
            } else {
                if (isNSEnabled) {
                    if (isECEnabled) {
                        ALOGV("%s: ECNS is running. Disabling NS and enabling EC alone", __func__);
                        status = qal_add_remove_effect(qal_stream_handle_,QAL_AUDIO_EFFECT_EC,true);
                        isNSEnabled = false;
                        goto exit;
                    } else {
                        ALOGV("%s: NS is running. Disabling it", __func__);
                        status = qal_add_remove_effect(qal_stream_handle_,QAL_AUDIO_EFFECT_NS,false);
                        isNSEnabled = false;
                        goto exit;
                    }
                } else {
                    ALOGE("%s: NS is not enabled", __func__);
                    status = -EINVAL;
                    goto exit;
               }
            }
        }
    }
exit:
    status = 0;
    return status;
}


int StreamInPrimary::SetGain(float gain) {
    struct qal_volume_data* volume;
    int ret = 0;

    volume = (struct qal_volume_data*)malloc(sizeof(uint32_t)
                +sizeof(struct qal_channel_vol_kv));
    volume->no_of_volpair = 1;
    volume->volume_pair[0].channel_mask = 0x03;
    volume->volume_pair[0].vol = gain;
    ret = qal_stream_set_volume(&qal_stream_handle_, volume);

    free(volume);
    if (ret) {
        ALOGE("Qal Stream volume Error (%x)", ret);
    }

    return ret;
}

int StreamInPrimary::Open() {
    int ret = -EINVAL;
    uint8_t channels = 0;
    struct qal_device qalDevice;
    struct qal_channel_info *ch_info = NULL;
    uint32_t inBufSize = 0;
    uint32_t outBufSize = 0;
    uint32_t inBufCount = NO_OF_BUF;
    uint32_t outBufCount = NO_OF_BUF;
    memset(&streamAttributes_, 0, sizeof(streamAttributes_));
    memset(&qalDevice, 0, sizeof(qalDevice));

    audio_extn_sound_trigger_check_and_get_session(this);
    if (is_st_session) {
        return 0;
    }

    channels = audio_channel_count_from_out_mask(config_.channel_mask);
    ch_info =(struct qal_channel_info *) calloc(1,sizeof(uint16_t) + sizeof(uint8_t)*channels);
    if (!ch_info) {
       ALOGE("malloc failed for ch_info");
       return -ENOMEM;
    }
    //need to convert channel mask to qal channel mask
    if (channels == 3) {
      ch_info->channels = 3;
      ch_info->ch_map[0] = QAL_CHMAP_CHANNEL_FL;
      ch_info->ch_map[1] = QAL_CHMAP_CHANNEL_FR;
      ch_info->ch_map[2] = QAL_CHMAP_CHANNEL_C;
    } else if (channels == 2) {
      ch_info->channels = 2;
      ch_info->ch_map[0] = QAL_CHMAP_CHANNEL_FL;
      ch_info->ch_map[1] = QAL_CHMAP_CHANNEL_FR;
    } else {
      ch_info->channels = 1;
      ch_info->ch_map[0] = QAL_CHMAP_CHANNEL_FL;
    }

    streamAttributes_.type = StreamInPrimary::GetQalStreamType(flags_);;
    streamAttributes_.flags = (qal_stream_flags_t)0;
    streamAttributes_.direction = QAL_AUDIO_INPUT;
    streamAttributes_.in_media_config.sample_rate = config_.sample_rate;
    streamAttributes_.in_media_config.bit_width = CODEC_BACKEND_DEFAULT_BIT_WIDTH;
    streamAttributes_.in_media_config.aud_fmt_id = QAL_AUDIO_FMT_DEFAULT_PCM; // TODO: need to convert this from output format

    streamAttributes_.in_media_config.ch_info = ch_info;
    qalDevice.id = qal_device_id_;

    ALOGD("%s:(%x:ret)%d",__func__,ret, __LINE__);
    ret = qal_stream_open(&streamAttributes_,
                          1,
                          &qalDevice,
                          0,
                          NULL,
                          &qal_callback,
                          (void *)this,
                          &qal_stream_handle_);

    ALOGD("%s:(%x:ret)%d",__func__,ret, __LINE__);

    if (ret) {
        ALOGE("Qal Stream Open Error (%x)", ret);
        ret = -EINVAL;
        goto error_open;
    }

    inBufSize = StreamInPrimary::GetBufferSize();
    if (streamAttributes_.type != QAL_STREAM_VOIP_TX) {
        inBufSize = inBufSize/NO_OF_BUF;
    }
    ret = qal_stream_set_buffer_size(qal_stream_handle_,(size_t*)&inBufSize,inBufCount,(size_t*)&outBufSize,outBufCount);
    if (ret) {
        ALOGE("Qal Stream set buffer size Error  (%x)", ret);
    }

    total_bytes_read_ = 0; // reset at each open

error_open:
    if (ch_info)
        free(ch_info);
    return ret;
}


uint32_t StreamInPrimary::GetBufferSize() {
    struct qal_stream_attributes streamAttributes_;

    streamAttributes_.type = StreamInPrimary::GetQalStreamType(flags_);
    if (streamAttributes_.type == QAL_STREAM_VOIP_TX) {
        return voip_get_buffer_size(config_.sample_rate);
    } else {
       return BUF_SIZE_CAPTURE * NO_OF_BUF;
    }
}

int StreamInPrimary::GetInputUseCase(audio_input_flags_t halStreamFlags, audio_source_t source)
{
    // TODO: cover other usecases
    int usecase = USECASE_AUDIO_RECORD;
    if ((halStreamFlags & AUDIO_INPUT_FLAG_TIMESTAMP) == 0 &&
        (halStreamFlags & AUDIO_INPUT_FLAG_COMPRESS) == 0 &&
        (halStreamFlags & AUDIO_INPUT_FLAG_FAST) != 0)
        usecase = USECASE_AUDIO_RECORD_LOW_LATENCY;

    if ((halStreamFlags & AUDIO_INPUT_FLAG_MMAP_NOIRQ) != 0)
        usecase = USECASE_AUDIO_RECORD_MMAP;
    else if (source == AUDIO_SOURCE_VOICE_COMMUNICATION &&
             halStreamFlags & AUDIO_INPUT_FLAG_VOIP_TX)
        usecase = USECASE_AUDIO_RECORD_VOIP;

    return usecase;
}

ssize_t StreamInPrimary::Read(const void *buffer, size_t bytes){
    int ret = 0;
    struct qal_buffer qalBuffer;
    uint32_t local_bytes_read = 0;
    qalBuffer.buffer = (void*)buffer;
    qalBuffer.size = bytes;
    qalBuffer.offset = 0;

    ALOGD("%s: Bytes:(%zu)",__func__,bytes);
    if (!qal_stream_handle_){
        ret = Open();
    }

    if (is_st_session) {
        audio_extn_sound_trigger_read(this, (void *)buffer, bytes);
        return bytes;
    }

    if (!stream_started_) {
        ret = qal_stream_start(qal_stream_handle_);
        stream_started_ = true;
        /* set cached volume if any, dont return failure back up */
        if (volume_) {
            ret = qal_stream_set_volume(qal_stream_handle_, volume_);
            if (ret) {
                ALOGE("Qal Stream volume Error (%x)", ret);
            }
        }
    }

    local_bytes_read = qal_stream_read(qal_stream_handle_, &qalBuffer);
    total_bytes_read_ += local_bytes_read;

    return local_bytes_read;
}

StreamPrimary::StreamPrimary(audio_io_handle_t handle,
    audio_devices_t devices, struct audio_config *config):
    qal_stream_handle_(NULL),
    handle_(handle),
    qal_device_id_(GetQalDeviceId(devices)),
    config_(*config),
    volume_(NULL)
{
    memset(&streamAttributes_, 0, sizeof(streamAttributes_));
    memset(&address_, 0, sizeof(address_));
}

StreamPrimary::~StreamPrimary(void)
{
    if (volume_) {
        free(volume_);
        volume_ = NULL;
    }
}

StreamInPrimary::StreamInPrimary(audio_io_handle_t handle,
    audio_devices_t devices,
    audio_input_flags_t flags,
    struct audio_config *config,
    audio_source_t source) :
    StreamPrimary(handle, devices, config),
    flags_(flags)
{
    stream_ = std::shared_ptr<audio_stream_in> (new audio_stream_in());

    if (config) {
        ALOGD("%s: enter: handle (%x) format(%#x) sample_rate(%d)\
            channel_mask(%#x) devices(%#x) flags(%#x)", __func__,
            handle, config->format, config->sample_rate, config->channel_mask,
            devices, flags);
        memcpy(&config_, config, sizeof(struct audio_config));
    } else {
        ALOGD("%s: enter: devices(%#x) flags(%#x)", __func__,devices, flags);
    }
    source_ = source;
    config_ = *config;
    usecase_ = GetInputUseCase(flags, source);

    (void)FillHalFnPtrs();
}

StreamInPrimary::~StreamInPrimary() {
}

int StreamInPrimary::FillHalFnPtrs() {
    int ret = 0;

    stream_.get()->common.get_sample_rate = astream_in_get_sample_rate;
    stream_.get()->common.set_sample_rate = astream_set_sample_rate;
    stream_.get()->common.get_buffer_size = astream_in_get_buffer_size;
    stream_.get()->common.get_channels = astream_in_get_channels;
    stream_.get()->common.get_format = astream_in_get_format;
    stream_.get()->common.set_format = astream_set_format;
    stream_.get()->common.standby = astream_in_standby;
    stream_.get()->common.dump = astream_dump;
    stream_.get()->common.set_parameters = astream_in_set_parameters;
    stream_.get()->common.get_parameters = astream_in_get_parameters;
    stream_.get()->common.add_audio_effect = astream_in_add_audio_effect;
    stream_.get()->common.remove_audio_effect = astream_in_remove_audio_effect;
    stream_.get()->set_gain = astream_in_set_gain;
    stream_.get()->read = in_read;
    stream_.get()->get_input_frames_lost = astream_in_get_input_frames_lost;
    stream_.get()->get_capture_position = astream_in_get_capture_position;
    stream_.get()->get_active_microphones = astream_in_get_active_microphones;
    stream_.get()->set_microphone_direction =
                                            astream_in_set_microphone_direction;
    stream_.get()->set_microphone_field_dimension =
                                            in_set_microphone_field_dimension;
    stream_.get()->update_sink_metadata = in_update_sink_metadata;

    return ret;
}

