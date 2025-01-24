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

#include "precomp.h"
#include "registry.tmh"

#include "registry.h"

NTSTATUS cx_reg_get_value(
    _In_ WDFDEVICE dev,
    _In_ PCWSTR key_cwstr,
    _Out_ PULONG value
)
{
    NTSTATUS status = STATUS_SUCCESS;
    WDFKEY key = WDF_NO_HANDLE;
    *value = 0;

    status = WdfDeviceOpenRegistryKey(dev, PLUGPLAY_REGKEY_DEVICE, STANDARD_RIGHTS_ALL, WDF_NO_OBJECT_ATTRIBUTES, &key);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfDeviceOpenRegistryKey failed with status %!STATUS!", status);
        return status;
    }

    DECLARE_UNICODE_STRING_SIZE(key_uc, 128);
    status = RtlUnicodeStringCopyString(&key_uc, key_cwstr);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "RtlUnicodeStringCopyString failed with status %!STATUS!", status);
        WdfRegistryClose(key);
        return status;
    }

    status = WdfRegistryQueryULong(key, &key_uc, value);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfDeviceOpenRegistryKey failed with status %!STATUS!", status);
    }

    WdfRegistryClose(key);
    return status;
}

NTSTATUS cx_reg_set_value(
    _In_ WDFDEVICE dev,
    _In_ PCWSTR key_cwstr,
    _In_ ULONG value
)
{
    NTSTATUS status = STATUS_SUCCESS;
    WDFKEY key = WDF_NO_HANDLE;

    status = WdfDeviceOpenRegistryKey(dev, PLUGPLAY_REGKEY_DEVICE, STANDARD_RIGHTS_ALL, WDF_NO_OBJECT_ATTRIBUTES, &key);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfDeviceOpenRegistryKey failed with status %!STATUS!", status);
        return status;
    }

    DECLARE_UNICODE_STRING_SIZE(key_uc, 128);
    status = RtlUnicodeStringCopyString(&key_uc, key_cwstr);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "RtlUnicodeStringCopyString failed with status %!STATUS!", status);
        WdfRegistryClose(key);
        return status;
    }

    status = WdfRegistryAssignULong(key, &key_uc, value);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfRegistryAssignULong failed with status %!STATUS!", status);
    }

    WdfRegistryClose(key);
    return status;
}
