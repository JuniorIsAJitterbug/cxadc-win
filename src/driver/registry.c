// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * cxadc-win - CX2388x ADC DMA driver for Windows
 *
 * Copyright (C) 2024-2025 Jitterbug <jitterbug@posteo.co.uk>
 *
 * Based on the Linux version created by
 * Copyright (C) 2005-2007 Hew How Chee <how_chee@yahoo.com>
 * Copyright (C) 2013-2015 Chad Page <Chad.Page@gmail.com>
 * Copyright (C) 2019-2023 Adam Sampson <ats@offog.org>
 * Copyright (C) 2020-2022 Tony Anderson <tandersn@cs.washington.edu>
 */

#include "pch.h"
#include "registry.tmh"
#include "registry.h"

_Use_decl_annotations_
NTSTATUS cx_reg_get_value(
    WDFDEVICE dev,
    PCWSTR key_cwstr,
    PULONG value
)
{
    WDFKEY key = WDF_NO_HANDLE;
    RETURN_NTSTATUS_IF_FAILED(WdfDeviceOpenRegistryKey(dev, PLUGPLAY_REGKEY_DEVICE, STANDARD_RIGHTS_ALL, WDF_NO_OBJECT_ATTRIBUTES, &key));
    NTSTATUS status = cx_reg_get_ulong(key, key_cwstr, value);
    WdfRegistryClose(key);
    return status;
}

_Use_decl_annotations_
NTSTATUS cx_reg_set_value(
    WDFDEVICE dev,
    PCWSTR key_cwstr,
    ULONG value
)
{
    WDFKEY key = WDF_NO_HANDLE;

    RETURN_NTSTATUS_IF_FAILED(WdfDeviceOpenRegistryKey(dev, PLUGPLAY_REGKEY_DEVICE, STANDARD_RIGHTS_ALL, WDF_NO_OBJECT_ATTRIBUTES, &key));
    NTSTATUS status = cx_reg_set_ulong(key, key_cwstr, value);
    WdfRegistryClose(key);
    return status;
}

_Use_decl_annotations_
NTSTATUS cx_reg_get_ulong(
    WDFKEY key,
    PCWSTR key_cwstr,
    PULONG value
)
{
    DECLARE_UNICODE_STRING_SIZE(key_uc, MAX_PATH);
    RETURN_NTSTATUS_IF_FAILED(RtlUnicodeStringCopyString(&key_uc, key_cwstr));
    RETURN_NTSTATUS_IF_FAILED(WdfRegistryQueryULong(key, &key_uc, value));
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS cx_reg_set_ulong(
    WDFKEY key,
    PCWSTR key_cwstr,
    ULONG value
)
{
    DECLARE_UNICODE_STRING_SIZE(key_uc, MAX_PATH);
    RETURN_NTSTATUS_IF_FAILED(RtlUnicodeStringCopyString(&key_uc, key_cwstr));
    RETURN_NTSTATUS_IF_FAILED(WdfRegistryAssignULong(key, &key_uc, value));
    return STATUS_SUCCESS;
}
