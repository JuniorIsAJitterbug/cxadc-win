/*
 * capture-server - Windows port of cxadc_vhs_server
 *
 * Copyright (C) 2025 Jitterbug <jitterbug@posteo.co.uk>
 */

#pragma once

#include "common.h"

#ifdef _WIN32
    #define COBJMACROS

    #pragma warning(push)
    #pragma warning(disable: 4668) // ignore missing macro
    #include <audioclient.h>
    #pragma warning(pop)

    #include <mmdeviceapi.h>
#else
    #include <alsa/asoundlib.h>
#endif

#include "ringbuffer.h"

struct audio_capture_state {
#ifdef _WIN32
    IMMDevice* device;
    IAudioClient2* client;
    IAudioCaptureClient* capture_client;
    HANDLE thread_task;
    HANDLE event_handle;

    bool com_initialized;
    unsigned frame_count;
#endif

    const struct audio_device_info* dev_info;
};

struct audio_device_info {
#ifdef _WIN32
    LPWSTR endpoint_id;
    WAVEFORMATEXTENSIBLE format;
    char format_name[16];
#else
    snd_pcm_t* handle;
    snd_pcm_format_t format;
#endif

    unsigned rate;
    unsigned channels;
};

#ifdef _WIN32
    #define AUDIO_DEFAULT_NAME	        "usb#vid_1209&pid_0002"
    #define AUDIO_DEFAULT_FORMAT        (WAVEFORMATEXTENSIBLE){ 0 }

    #define _audio_get_error(id)		win32_get_err_msg((id))
    #define _audio_get_error_hr(hr)		_audio_get_error(HRESULT_CODE((hr)))
#else
    #define AUDIO_DEFAULT_NAME			"hw:CARD=CXADCADCClockGe"
    #define AUDIO_DEFAULT_FORMAT		SND_PCM_FORMAT_UNKNOWN

    #define _audio_get_error(errnum)	snd_strerror((errnum))
#endif

int audio_device_init(struct audio_device_info* device, char* name);
void audio_device_deinit(struct audio_device_info* dev_info);

int audio_capture_init(struct audio_capture_state* state, const struct audio_device_info* dev_info);
void audio_capture_deinit(struct audio_capture_state* state);

int audio_capture_start(const struct audio_capture_state* state);
ssize_t audio_capture_get_next_frame_count(const struct audio_capture_state* state);
ssize_t audio_capture_fill_buffer(struct audio_capture_state* state, void* out_buffer, ssize_t frame_count);
void audio_capture_stop(struct audio_capture_state* state);

size_t audio_get_sample_size(const struct audio_device_info* dev_info);
void audio_set_format_value(struct audio_device_info* dev_info, const char* format);
const char* audio_get_format_name(const struct audio_device_info* dev_info);
