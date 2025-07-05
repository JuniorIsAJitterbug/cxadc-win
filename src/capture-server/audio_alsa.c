/*
 * capture-server - Windows port of cxadc_vhs_server
 *
 * Copyright (C) 2025 Jitterbug <jitterbug@posteo.co.uk>
 * Copyright (C) 2024 namazso <admin@namazso.eu>
 */

#ifdef __linux__

#include "audio.h"

#include <stdbool.h>
#include <stdio.h>

int audio_device_init(struct audio_device_info* dev_info, char* name) {
    static const int MODE = SND_PCM_NONBLOCK | SND_PCM_NO_AUTO_RESAMPLE | SND_PCM_NO_AUTO_CHANNELS | SND_PCM_NO_AUTO_FORMAT | SND_PCM_NO_SOFTVOL;
    int ret = 0;

    if ((ret = snd_pcm_open(&dev_info->handle, name, SND_PCM_STREAM_CAPTURE, MODE)) < 0) {
        fprintf(stderr, "cannot open ALSA device: %s", _audio_get_error(ret));
        goto error_1;
    }

    snd_pcm_hw_params_t* hw_params = NULL;
    snd_pcm_hw_params_alloca(&hw_params);

    if (hw_params == NULL) {
        goto error_1;
    }

    if ((ret = snd_pcm_hw_params_any(dev_info->handle, hw_params)) < 0) {
        fprintf(stderr, "cannot initialize hardware parameter structure: %s", _audio_get_error(ret));
        goto error_2;
    }

    if ((ret = snd_pcm_hw_params_set_access(dev_info->handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        fprintf(stderr, "cannot set access type: %s", _audio_get_error(ret));
        goto error_2;
    }

    if (!dev_info->rate) {
        if (snd_pcm_hw_params_get_rate(hw_params, &dev_info->rate, 0) < 0) {
            if ((ret = snd_pcm_hw_params_get_rate_max(hw_params, &dev_info->rate, 0)) < 0) {
                fprintf(stderr, "cannot get sample rate: %s", _audio_get_error(ret));
                goto error_2;
            }
        }
    }

    if ((ret = snd_pcm_hw_params_set_rate(dev_info->handle, hw_params, dev_info->rate, 0)) < 0) {
        fprintf(stderr, "cannot set sample rate: %s", _audio_get_error(ret));
        goto error_2;
    }

    if (dev_info->channels) {
        if ((ret = snd_pcm_hw_params_set_channels(dev_info->handle, hw_params, dev_info->channels)) < 0) {
            fprintf(stderr, "cannot set channel count: %s", _audio_get_error(ret));
            goto error_2;
        }
    }
    else {
        if ((ret = snd_pcm_hw_params_get_channels(hw_params, &dev_info->channels)) < 0) {
            fprintf(stderr, "cannot get channel count: %s", _audio_get_error(ret));
            goto error_2;
        }
    }

    if (dev_info->format != SND_PCM_FORMAT_UNKNOWN) {
        if ((ret = snd_pcm_hw_params_set_format(dev_info->handle, hw_params, dev_info->format)) < 0) {
            fprintf(stderr, "cannot set sample format: %s", _audio_get_error(ret));
            goto error_2;
        }
    }
    else {
        if ((ret = snd_pcm_hw_params_get_format(hw_params, &dev_info->format)) < 0) {
            fprintf(stderr, "cannot get sample format: %s", _audio_get_error(ret));
            goto error_2;
        }
    }

    if ((ret = snd_pcm_hw_params(dev_info->handle, hw_params)) < 0) {
        fprintf(stderr, "cannot set hw parameters: %s", _audio_get_error(ret));
        goto error_2;
    }

    snd_pcm_sw_params_t* sw_params = NULL;
    snd_pcm_sw_params_alloca(&sw_params);

    if (sw_params == NULL) {
        goto error_2;
    }

    if ((ret = snd_pcm_sw_params_current(dev_info->handle, sw_params)) < 0) {
        fprintf(stderr, "cannot query sw parameters: %s", _audio_get_error(ret));
        goto error_3;
    }

    if ((ret = snd_pcm_sw_params_set_tstamp_mode(dev_info->handle, sw_params, SND_PCM_TSTAMP_ENABLE)) < 0) {
        fprintf(stderr, "cannot set tstamp mode: %s", _audio_get_error(ret));
        goto error_3;
    }

    if ((ret = snd_pcm_sw_params_set_tstamp_type(dev_info->handle, sw_params, SND_PCM_TSTAMP_TYPE_MONOTONIC_RAW)) < 0) {
        fprintf(stderr, "cannot set tstamp type: %s", _audio_get_error(ret));
        goto error_3;
    }

    if ((ret = snd_pcm_sw_params(dev_info->handle, sw_params)) < 0) {
        fprintf(stderr, "cannot set sw parameters: %s", _audio_get_error(ret));
        goto error_3;
    }

    if ((ret = snd_pcm_prepare(dev_info->handle)) < 0) {
        fprintf(stderr, "cannot prepare audio interface for use: %s", _audio_get_error(ret));
        goto error_3;
    }

    return 0;

error_3:
    snd_pcm_sw_params_free(sw_params);

error_2:
    snd_pcm_hw_params_free(hw_params);

error_1:
    return 1;
}

void audio_device_deinit(struct audio_device_info* dev_info) {
    snd_pcm_close(dev_info->handle);
}

int audio_capture_init(struct audio_capture_state* state, const struct audio_device_info* dev_info) {
    (void)state;
    state->dev_info = dev_info;
    return 0;
}

void audio_capture_deinit(struct audio_capture_state* state) {
    (void)state;
}

int audio_capture_start(const struct audio_capture_state* state) {
    return snd_pcm_start(state->dev_info->handle);
}

ssize_t audio_capture_get_next_frame_count(const struct audio_capture_state* state) {
    return (ssize_t)snd_pcm_avail(state->dev_info->handle);
}

ssize_t audio_capture_fill_buffer(struct audio_capture_state* state, void* out_buffer, ssize_t buffer_sz) {
    size_t len_samples = snd_pcm_bytes_to_frames(state->dev_info->handle, (ssize_t)buffer_sz);

    long count = snd_pcm_readi(state->dev_info->handle, out_buffer, len_samples);

    if (count == 0 || count == -EAGAIN) {
        usleep(1);
        return 0;
    }

    if (count < 0) {
        fprintf(stderr, "snd_pcm_readi failed: %s\n", _audio_get_error((int)count));
        return -1;
    }

    return snd_pcm_frames_to_bytes(state->dev_info->handle, count);
}

void audio_capture_stop(struct audio_capture_state* state) {
    snd_pcm_drop(state->dev_info->handle);
}

size_t audio_get_sample_size(const struct audio_device_info* dev_info) {
    ssize_t format_size;

    if ((format_size = snd_pcm_format_size(dev_info->format, 1)) < 0) {
        fprintf(stderr, "cannot get format size: %s", _audio_get_error(format_size));
        return -1;
    }

    return format_size * dev_info->channels;
}

void audio_set_format_value(struct audio_device_info* dev_info, const char* format) {
    dev_info->format = snd_pcm_format_value(format);
}

const char* audio_get_format_name(const struct audio_device_info* dev_info) {
    return snd_pcm_format_name(dev_info->format);
}

#endif
