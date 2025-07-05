/*
 * capture-server - Windows port of cxadc_vhs_server
 *
 * Copyright (C) 2025 Jitterbug <jitterbug@posteo.co.uk>
 */

#ifdef _WIN32

#include "audio.h"

#include <stdbool.h>
#include <stdio.h>

#include <avrt.h>
#include <devicetopology.h>
#include <mmeapi.h>

#pragma comment(lib, "avrt")
#pragma comment(lib, "ksuser")

#define REFTIMES_PER_SEC    10000000
#define OLD_FIRMWARE_NAME   L"usb#vid_1209&pid_0001"

const CLSID IID_MMDeviceEnumerator = { 0xBCDE0395, 0xE52F, 0x467C, { 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E } };
const CLSID IID_IMMDeviceEnumerator = { 0xA95664D2, 0x9614, 0x4F35, { 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6 } };
const CLSID IID_IDeviceTopology = { 0x2A07407E, 0x6497, 0x4A18, { 0x97, 0x87, 0x32, 0xF7, 0x9B, 0xD0, 0xD9, 0x8F } };
const CLSID IID_IAudioClient = { 0x1CB9AD4C, 0xDBFA, 0x4C32, {0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2 } };
const CLSID IID_IAudioCaptureClient = { 0xC8ADBD64, 0xE71E, 0x48a0, { 0xA4, 0xDE, 0x18, 0x5C, 0x39, 0x5C, 0xD3, 0x17 } };
const PROPERTYKEY PKEY_AudioEngine_DeviceFormat = { 0xF19F064D, 0x82C, 0x4E27, { 0xBC, 0x73, 0x68, 0x82, 0xA1, 0xBB, 0x8E, 0x4C }, 0 };

HRESULT audio_find_device(char* name, IMMDevice** out_device);
HRESULT audio_get_device(LPCWSTR endpoint_id, IMMDevice** out_device);
HRESULT audio_get_format(IMMDevice* device, struct audio_device_info* dev_info);
bool audio_is_device_match(IMMDevice* device, PCWSTR id_str);

// init device id and format for the given name
int audio_device_init(struct audio_device_info* dev_info, char* name) {
    HRESULT hr = S_OK;

    if (FAILED(hr = CoInitializeEx(NULL, COINIT_DISABLE_OLE1DDE))) {
        fprintf(stderr, "CoInitializeEx failed: %s", _audio_get_error_hr(hr));
        goto error_1;
    }

    IMMDevice* device = NULL;

    if (FAILED(hr = audio_find_device(name, &device))) {
        goto error_2;
    }

    LPWSTR endpoint_id = NULL;

    if (FAILED(hr = IMMDevice_GetId(device, &endpoint_id))) {
        fprintf(stderr, "IMMDevice::IMMDevice_GetId failed: %s", _audio_get_error_hr(hr));
        goto error_3;
    }

    if (FAILED(hr = audio_get_format(device, dev_info)))
    {
        goto error_4;
    }

    dev_info->endpoint_id = endpoint_id;
    IMMDevice_Release(device);

    return HRESULT_CODE(hr);

error_4:
    CoTaskMemFree(endpoint_id);

error_3:
    IMMDevice_Release(device);

error_2:
    CoUninitialize();

error_1:
    return HRESULT_CODE(hr);
}

void audio_device_deinit(struct audio_device_info* dev_info) {
    if (dev_info->endpoint_id != NULL) {
        CoTaskMemFree(dev_info->endpoint_id);
        dev_info->endpoint_id = NULL;

        CoUninitialize();
    }
}

int audio_capture_init(struct audio_capture_state* state, const struct audio_device_info* dev_info) {
    HRESULT hr = S_OK;

    state->device = NULL;
    state->client = NULL;
    state->capture_client = NULL;
    state->thread_task = NULL;
    state->event_handle = NULL;

    if (FAILED(hr = CoInitializeEx(NULL, COINIT_DISABLE_OLE1DDE))) {
        fprintf(stderr, "CoInitializeEx failed: %s", _audio_get_error_hr(hr));
        goto error;
    }

    state->com_initialized = true;

    // boost thread prio
    DWORD taskIndex = 0;

    if ((state->thread_task = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex)) == NULL) {
        fprintf(stderr, "AvSetMmThreadCharacteristicsA failed: %s", _audio_get_error(GetLastError()));
        // just print error on permission err
        if (GetLastError() != ERROR_PRIVILEGE_NOT_HELD) {
            goto error;
        }
    }

    if (FAILED(hr = audio_get_device(dev_info->endpoint_id, &state->device))) {
        goto error;
    }

    if (FAILED(hr = IMMDevice_Activate(state->device, &IID_IAudioClient, CLSCTX_ALL, 0, &state->client))) {
        fprintf(stderr, "IMMDevice::Activate (IAudioClient) failed: %s", _audio_get_error_hr(hr));
        goto error;
    }

    if ((state->event_handle = CreateEventW(NULL, FALSE, FALSE, NULL)) == NULL) {
        fprintf(stderr, "CreateEventA failed: %s", _get_error());
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto error;
    }

    REFERENCE_TIME default_period = 0;
    REFERENCE_TIME min_period = 0;

    AudioClientProperties props = {
      .cbSize = sizeof(AudioClientProperties),
      .bIsOffload = FALSE,
      .eCategory = AudioCategory_Communications,
      .Options = AUDCLNT_STREAMOPTIONS_RAW
    };

    if (FAILED(hr = IAudioClient2_SetClientProperties(state->client, &props))) {
        fprintf(stderr, "IAudioClient2::SetClientProperties failed: %s", _audio_get_error_hr(hr));
        goto error;
    }


    if (FAILED(hr = IAudioClient2_GetDevicePeriod(state->client, &default_period, &min_period))) {
        fprintf(stderr, "IAudioClient2::GetDevicePeriod failed: %s", _audio_get_error_hr(hr));
        goto error;
    }

    REFERENCE_TIME duration = default_period;

retry:
    hr = IAudioClient2_Initialize(
        state->client,
        AUDCLNT_SHAREMODE_EXCLUSIVE,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        duration,
        duration,
        (const WAVEFORMATEX*)&dev_info->format,
        0
    );

    // align buffer and retry once
    if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED && duration == default_period) {
        unsigned frame_count = 0;

        if (FAILED(hr = IAudioClient2_GetBufferSize(state->client, &frame_count))) {
            fprintf(stderr, "IAudioClient2::GetBufferSize failed: %s", _audio_get_error_hr(hr));
            goto error;
        }

        duration = (REFERENCE_TIME)((REFTIMES_PER_SEC / dev_info->format.Format.nSamplesPerSec * frame_count) + 0.5);
        goto retry;
    }

    if (FAILED(hr)) {
        fprintf(stderr, "IAudioClient2::Initialize failed: %s", _audio_get_error_hr(hr));
        goto error;
    }

    if (FAILED(hr = IAudioClient2_GetBufferSize(state->client, &state->frame_count))) {
        fprintf(stderr, "IAudioClient2::GetBufferSize failed: %s", _audio_get_error_hr(hr));
        goto error;
    }

    if ((FAILED(hr = IAudioClient2_SetEventHandle(state->client, state->event_handle)))) {
        fprintf(stderr, "IAudioClient2::SetEventHandle failed: %s", _audio_get_error_hr(hr));
        goto error;
    }

    if (FAILED(hr = IAudioClient2_GetService(state->client, &IID_IAudioCaptureClient, &state->capture_client))) {
        fprintf(stderr, "IAudioClient2::GetService failed: %s", _audio_get_error_hr(hr));
        goto error;
    }

    state->dev_info = dev_info;

    return HRESULT_CODE(hr);

error:
    audio_capture_deinit(state);
    return HRESULT_CODE(hr);
}

void audio_capture_deinit(struct audio_capture_state* state) {
    if (state->event_handle != NULL) {
        CloseHandle(state->event_handle);
        state->event_handle = NULL;
    }

    if (state->capture_client != NULL) {
        IAudioCaptureClient_Release(state->capture_client);
        state->capture_client = NULL;
    }

    if (state->client != NULL) {
        IAudioClient2_Release(state->client);
        state->client = NULL;
    }

    if (state->device != NULL) {
        IMMDevice_Release(state->device);
        state->device = NULL;
    }

    if (state->thread_task != NULL) {
        AvRevertMmThreadCharacteristics(state->thread_task);
        state->thread_task = NULL;
    }

    if (state->com_initialized)
    {
        CoUninitialize();
        state->com_initialized = false;
    }
}

int audio_capture_start(const struct audio_capture_state* state) {
    HRESULT hr = S_OK;

    if (FAILED(hr = IAudioClient2_Start(state->client))) {
        fprintf(stderr, "IAudioClient2::Start failed: %s", _audio_get_error_hr(hr));
    }

    return HRESULT_CODE(hr);
}


ssize_t audio_capture_get_next_frame_count(const struct audio_capture_state* state) {
    if (WaitForSingleObject(state->event_handle, 2000) != WAIT_OBJECT_0) {
        fprintf(stderr, "IAudioCaptureClient event signal ended: %s", _get_error());
        return -1;
    }

    // exclusive mode returns the same buffer size each time
    return state->frame_count;
}

ssize_t audio_capture_fill_buffer(struct audio_capture_state* state, void* out_buffer, ssize_t frame_count) {
    HRESULT hr = S_OK;
    BYTE* buffer;
    DWORD flags = 0;
    UINT64 dev_pos = 0;
    UINT64 qpc_pos = 0;

    if (FAILED(hr = IAudioCaptureClient_GetBuffer(state->capture_client, &buffer, (PUINT32)&frame_count, &flags, &dev_pos, &qpc_pos))) {
        fprintf(stderr, "IAudioCaptureClient::GetBuffer failed: %s", _audio_get_error_hr(hr));
        goto error;
    }

    size_t byte_count = 0;

    if (flags != AUDCLNT_BUFFERFLAGS_SILENT && out_buffer != NULL) {
        byte_count = state->dev_info->format.Format.nBlockAlign * frame_count;
        memcpy(out_buffer, buffer, byte_count);
    }

    if (FAILED(hr = IAudioCaptureClient_ReleaseBuffer(state->capture_client, (UINT32)frame_count))) {
        fprintf(stderr, "IAudioCaptureClient::ReleaseBuffer failed: %s", _audio_get_error_hr(hr));
        goto error;
    }

    return byte_count;

error:
    return -1;
}

void audio_capture_stop(struct audio_capture_state* state) {
    if (state->client != NULL) {
        IAudioClient2_Stop(state->client);
    }
}

HRESULT audio_get_format(IMMDevice* device, struct audio_device_info* dev_info) {
    HRESULT hr = S_OK;

    IAudioClient* client = NULL;

    if (FAILED(hr = IMMDevice_Activate(device, &IID_IAudioClient, CLSCTX_ALL, 0, &client))) {
        fprintf(stderr, "IMMDevice::Activate (IAudioClient) failed: %s", _audio_get_error_hr(hr));
        goto exit;
    }

    IPropertyStore* prop_store = NULL;

    if (FAILED(hr = IMMDevice_OpenPropertyStore(device, STGM_READ, &prop_store))) {
        fprintf(stderr, "IMMDevice::OpenPropertyStore failed: %s", _audio_get_error_hr(hr));
        goto error_1;
    }

    PROPVARIANT var;
    PropVariantInit(&var);

    if (FAILED(hr = IPropertyStore_GetValue(prop_store, &PKEY_AudioEngine_DeviceFormat, &var))) {
        fprintf(stderr, "IPropertyStore::GetValue failed: %s", _audio_get_error_hr(hr));
        goto error_2;
    }

    PWAVEFORMATEXTENSIBLE format = (PWAVEFORMATEXTENSIBLE)var.blob.pBlobData;

    if (dev_info->rate != 0) {
        format->Format.nSamplesPerSec = dev_info->rate;
    }
    else {
        dev_info->rate = format->Format.nSamplesPerSec;
    }

    if (dev_info->channels != 0) {
        format->Format.nChannels = (WORD)dev_info->channels;
    }
    else {
        dev_info->channels = (unsigned int)format->Format.nChannels;
    }

    format->Format.nAvgBytesPerSec = format->Format.nSamplesPerSec * format->Format.nChannels * (format->Format.wBitsPerSample / 8);

    if (FAILED(hr = IAudioClient2_IsFormatSupported(client, AUDCLNT_SHAREMODE_EXCLUSIVE, (PWAVEFORMATEX)format, NULL))) {
        fprintf(stderr, "IAudioClient2::IsFormatSupported failed: %s", _audio_get_error_hr(hr));
        goto error_3;
    }

    dev_info->format = *format;
    snprintf(dev_info->format_name, sizeof(dev_info->format_name) - 1, "S%d_%dLE", format->Format.wBitsPerSample, format->Format.nChannels);

error_3:
    PropVariantClear(&var);

error_2:
    IPropertyStore_Release(prop_store);

error_1:
    IAudioClient2_Release(client);

exit:
    return hr;
}

size_t audio_get_sample_size(const struct audio_device_info* dev_info) {
    return dev_info->format.Format.nBlockAlign;
}

void audio_set_format_value(struct audio_device_info* dev_info, const char* format) {
    // no win32 equivalent
    UNREFERENCED_PARAMETER(dev_info);
    UNREFERENCED_PARAMETER(format);

    fprintf(stderr, "lformat ignored: not supported on win32");
}

HRESULT audio_find_device(char* name, IMMDevice** out_device) {
    HRESULT hr = S_OK;
    *out_device = NULL;

    IMMDeviceEnumerator* device_enumerator = NULL;

    if (FAILED(hr = CoCreateInstance(&IID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator, &device_enumerator))) {
        fprintf(stderr, "CoCreateInstance (IMMDeviceEnumerator) failed: %s", _audio_get_error_hr(hr));
        goto exit;
    }

    IMMDeviceCollection* device_collection = NULL;

    if (FAILED(hr = IMMDeviceEnumerator_EnumAudioEndpoints(device_enumerator, eCapture, DEVICE_STATE_ACTIVE, &device_collection))) {
        fprintf(stderr, "IMMDeviceEnumerator::EnumAudioEndpoints failed: %s", _audio_get_error_hr(hr));
        goto error_1;
    }

    UINT dev_count = 0;

    if (FAILED(hr = IMMDeviceCollection_GetCount(device_collection, &dev_count))) {
        fprintf(stderr, "IMMDeviceCollection::GetCount failed: %s", _audio_get_error_hr(hr));
        goto error_2;
    }

    // matching path is easier than opening and checking descriptor...
    wchar_t id_str[64];

    if (MultiByteToWideChar(CP_UTF8, 0, name, -1, id_str, 64) == 0) {
        fprintf(stderr, "MultiByteToWideChar failed: %s", _get_error());
        goto error_2;
    }

    IMMDevice* current_device = NULL;

    for (unsigned i = 0; i < dev_count; i++) {
        if (FAILED(hr = IMMDeviceCollection_Item(device_collection, i, &current_device))) {
            fprintf(stderr, "IMMDeviceCollection::Item failed: %s", _audio_get_error_hr(hr));
            goto error_2;
        }

        if (audio_is_device_match(current_device, id_str)) {
            break;
        }

        if (audio_is_device_match(current_device, OLD_FIRMWARE_NAME)) {
            fprintf(stderr, "ClockGen is using an older firmware, audio capture will have issues...");
            break;
        }

        IMMDevice_Release(current_device);
        current_device = NULL;
    }

    if (current_device == NULL) {
        fprintf(stderr, "Unable to find device audio device %s", name);
        hr = E_NOTFOUND;
        goto error_2;
    }

    // success
    *out_device = current_device;

error_2:
    IMMDeviceCollection_Release(device_collection);

error_1:
    IMMDeviceEnumerator_Release(device_enumerator);

exit:
    return hr;
}

// Get IMMDevice object from endpoint id string
HRESULT audio_get_device(LPCWSTR endpoint_id, IMMDevice** out_device) {
    HRESULT hr = S_OK;

    IMMDeviceEnumerator* device_enumerator = NULL;

    if (FAILED(hr = CoCreateInstance(&IID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator, &device_enumerator))) {
        fprintf(stderr, "CoCreateInstance (IMMDeviceEnumerator) failed: %s", _audio_get_error_hr(hr));
        goto error_1;
    }

    if (FAILED(hr = IMMDeviceEnumerator_GetDevice(device_enumerator, endpoint_id, out_device))) {
        fprintf(stderr, "IMMDeviceEnumerator::GetDevice failed: %s", _audio_get_error_hr(hr));
        goto error_2;
    }

error_2:
    IMMDeviceEnumerator_Release(device_enumerator);

error_1:
    return hr;
}

bool audio_is_device_match(IMMDevice* device, PCWSTR id_str) {
    HRESULT hr = S_OK;
    IDeviceTopology* topology = NULL;
    bool is_match = false;

    if (FAILED(hr = IMMDevice_Activate(device, &IID_IDeviceTopology, CLSCTX_ALL, NULL, &topology))) {
        fprintf(stderr, "IMMDevice::Activate (IDeviceTopology) failed: %s", _audio_get_error_hr(hr));
        goto exit;
    }

    IConnector* connector = NULL;

    if (FAILED(hr = IDeviceTopology_GetConnector(topology, 0, &connector))) {
        fprintf(stderr, "IDeviceTopology::GetConnector failed: %s", _audio_get_error_hr(hr));
        goto error_1;
    }

    LPWSTR device_id = NULL;

    if (FAILED(hr = IConnector_GetDeviceIdConnectedTo(connector, &device_id))) {
        fprintf(stderr, "IConnector::GetDeviceIdConnectedTo failed: %s", _audio_get_error_hr(hr));
        goto error_2;
    }

    is_match = wcsstr(device_id, id_str) != NULL;
    CoTaskMemFree(device_id);

error_2:
    IConnector_Release(connector);

error_1:
    IDeviceTopology_Release(topology);

exit:
    return is_match;
}

const char* audio_get_format_name(const struct audio_device_info* dev_info) {
    return dev_info->format_name;
}

#endif
