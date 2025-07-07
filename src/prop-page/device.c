// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * cxadc-win-prop-page - Property Page for the cxadc-win driver
 *
 * Copyright (C) 2024-2025 Jitterbug <jitterbug@posteo.co.uk>
 */

#include "pch.h"
#include "device.h"

#pragma comment(lib, "setupapi.lib")

_Use_decl_annotations_
HRESULT cx_init_prop_data(
    PCX_PROP_DATA prop_data
)
{
    DEBUG_LOG("Updating prop_data values");

    prop_data->prev_state = prop_data->state;
    prop_data->prev_config = prop_data->config;

    HRESULT hr = S_OK;
    HANDLE dev_handle = INVALID_HANDLE_VALUE;

    if ((dev_handle = CreateFileW(
        prop_data->dev_path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL)) == INVALID_HANDLE_VALUE)
    {
        hr = HRESULT_FROM_LASTERROR;
        goto exit;
    }

    // get win32 path
    if (!DeviceIoControl(
        dev_handle,
        CX_IOCTL_STATE_WIN32_PATH_GET,
        NULL,
        0,
        &prop_data->win32_path,
        _countof(prop_data->win32_path),
        NULL,
        NULL))
    {
        hr = HRESULT_FROM_LASTERROR;
        goto exit;
    }

    // get state data
    if (!DeviceIoControl(
        dev_handle,
        CX_IOCTL_STATE_GET,
        NULL,
        0,
        &prop_data->state,
        sizeof(DEVICE_STATE),
        NULL,
        NULL))
    {
        hr = HRESULT_FROM_LASTERROR;
        goto exit;
    }

    // get config data
    if (!DeviceIoControl(
        dev_handle,
        CX_IOCTL_CONFIG_GET,
        NULL,
        0,
        &prop_data->config,
        sizeof(DEVICE_CONFIG),
        NULL,
        NULL))
    {
        hr = HRESULT_FROM_LASTERROR;
        goto exit;
    }

exit:
    _SafeCloseHandle(dev_handle);
    return hr;
}

_Use_decl_annotations_
HRESULT cx_ioctl_set(
    PCX_PROP_DATA prop_data,
    DWORD ioctl_code,
    DWORD data
)
{
    HRESULT hr = S_OK;
    HANDLE dev_handle = INVALID_HANDLE_VALUE;

    if ((dev_handle = CreateFileW(
        prop_data->dev_path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL)) == INVALID_HANDLE_VALUE)
    {
        hr = HRESULT_FROM_LASTERROR;
        goto exit;
    }

    // set data
    if (!DeviceIoControl(dev_handle, ioctl_code, &data, sizeof(DWORD), NULL, 0, NULL, NULL))
    {
        hr = HRESULT_FROM_LASTERROR;
        goto exit;
    }

exit:
    _SafeCloseHandle(dev_handle);
    return hr;
}

_Use_decl_annotations_
HRESULT cx_get_device_id(
    HDEVINFO class_info,
    PSP_DEVINFO_DATA class_info_data,
    PWCHAR* dev_id
)
{
    HRESULT hr = S_OK;
    *dev_id = NULL;

    // get device instance id length
    DEVPROPTYPE prop_type = 0;
    DWORD dev_id_len = 0;

    SetupDiGetDevicePropertyW(
        class_info,
        class_info_data,
        &DEVPKEY_Device_InstanceId,
        &prop_type,
        NULL,
        0,
        &dev_id_len,
        0);

    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || dev_id_len == 0)
    {
        hr = HRESULT_FROM_LASTERROR;
        goto error;
    }

    // get device instance id
    if ((*dev_id = _HeapAllocZero(dev_id_len)) == NULL)
    {
        hr = E_OUTOFMEMORY;
        goto error;
    }

    if (!SetupDiGetDevicePropertyW(
        class_info,
        class_info_data,
        &DEVPKEY_Device_InstanceId,
        &prop_type,
        (PBYTE)*dev_id,
        dev_id_len,
        NULL,
        0))
    {
        hr = HRESULT_FROM_LASTERROR;
        goto error;
    }

    return hr;

error:
    if (*dev_id != NULL)
    {
        _HeapFree(*dev_id);
        *dev_id = NULL;
    }

    return hr;
}

_Use_decl_annotations_
HRESULT cx_get_device_path(
    PCWSTR dev_id,
    PWCHAR* device_path
)
{
    HRESULT hr = S_OK;
    HDEVINFO dev_info = INVALID_HANDLE_VALUE;
    SP_DEVICE_INTERFACE_DATA dev_data =
    {
        .cbSize = sizeof(SP_DEVICE_INTERFACE_DATA)
    };
    PSP_DEVICE_INTERFACE_DETAIL_DATA dev_detail_data = NULL;
    *device_path = NULL;

    // get device info
    if ((dev_info = SetupDiGetClassDevsW(
        &GUID_DEVINTERFACE_CXADCWIN,
        dev_id,
        NULL,
        DIGCF_DEVICEINTERFACE)) == INVALID_HANDLE_VALUE)
    {
        hr = HRESULT_FROM_LASTERROR;
        goto exit;
    }

    if (!SetupDiEnumDeviceInterfaces(dev_info, NULL, &GUID_DEVINTERFACE_CXADCWIN, 0, &dev_data))
    {
        hr = HRESULT_FROM_LASTERROR;
        goto cleanup_1;
    }

    // get device interface detail length
    DWORD dev_detail_data_len = 0;

    SetupDiGetDeviceInterfaceDetailW(dev_info, &dev_data, NULL, 0, &dev_detail_data_len, NULL);

    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || dev_detail_data_len == 0)
    {
        hr = HRESULT_FROM_LASTERROR;
        goto cleanup_1;
    }

    // get device interface detail
    if ((dev_detail_data = _HeapAllocZero(dev_detail_data_len)) == NULL)
    {
        E_OUTOFMEMORY;
        goto cleanup_1;
    }

    dev_detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

    if (!SetupDiGetDeviceInterfaceDetailW(dev_info, &dev_data, dev_detail_data, dev_detail_data_len, NULL, NULL))
    {
        hr = HRESULT_FROM_LASTERROR;
        goto cleanup_2;
    }

    DEBUG_LOG("Found device %ws", dev_detail_data->DevicePath);

    size_t len = wcslen(dev_detail_data->DevicePath) + 1;

    if ((*device_path = _HeapAllocZero(len * sizeof(WCHAR))) == NULL)
    {
        hr = E_OUTOFMEMORY;
        goto cleanup_2;
    }

    if (wcscpy_s(*device_path, len, dev_detail_data->DevicePath) != 0)
    {
        hr = HRESULT_FROM_WIN32(_doserrno);
        _SafeHeapFree(*device_path);
    }

cleanup_2:
    _HeapFree(dev_detail_data);

cleanup_1:
    SetupDiDestroyDeviceInfoList(dev_info);

exit:
    return hr;
}
