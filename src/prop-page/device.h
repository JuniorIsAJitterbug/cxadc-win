// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * cxadc-win-prop-page - Property Page for the cxadc-win driver
 *
 * Copyright (C) 2024-2025 Jitterbug <jitterbug@posteo.co.uk>
 */

#pragma once

HRESULT cx_init_prop_data(_Inout_ PCX_PROP_DATA prop_data);
HRESULT cx_ioctl_set(_In_ PCX_PROP_DATA prop_data, _In_ DWORD ioctl_code, _In_ DWORD data);

_Success_(SUCCEEDED(return))
HRESULT cx_get_device_id(
    _In_ HDEVINFO class_info,
    _In_ PSP_DEVINFO_DATA class_info_data,
    _Outptr_result_nullonfailure_ PWCHAR* device_id
);

HRESULT cx_get_device_path(_In_ PCWSTR device_id, _Outptr_result_maybenull_ PWCHAR* device_path);
