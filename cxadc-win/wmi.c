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
#include "wmi.tmh"

#include <wmidata.h>
#include "wmi.h"
#include "control.h"

NTSTATUS cx_wmi_register(
    _In_ PDEVICE_CONTEXT dev_ctx
)
{
    NTSTATUS status = STATUS_SUCCESS;
    DECLARE_CONST_UNICODE_STRING(mof_resource_name, MOF_RESOURCE_NAME);

    status = WdfDeviceAssignMofResourceName(dev_ctx->dev, &mof_resource_name);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfDeviceAssignMofResourceName failed with status %!STATUS!", status);
        return status;
    }

    WDF_WMI_PROVIDER_CONFIG provider_cfg;
    WDF_WMI_INSTANCE_CONFIG instance_cfg;

    // state
    WDF_WMI_PROVIDER_CONFIG_INIT(&provider_cfg, &GUID_WMI_CXADCWIN_STATE);
    provider_cfg.MinInstanceBufferSize = sizeof(dev_ctx->state);

    WDF_WMI_INSTANCE_CONFIG_INIT_PROVIDER_CONFIG(&instance_cfg, &provider_cfg);
    instance_cfg.Register = TRUE;
    instance_cfg.EvtWmiInstanceQueryInstance = cx_wmi_state_query;
    instance_cfg.EvtWmiInstanceExecuteMethod = cx_wmi_state_exe;

    status = WdfWmiInstanceCreate(dev_ctx->dev, &instance_cfg, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfWmiInstanceCreate (state) failed with status %!STATUS!", status);
        return status;
    }

    // config
    WDF_WMI_PROVIDER_CONFIG_INIT(&provider_cfg, &GUID_WMI_CXADCWIN_CONFIG);
    provider_cfg.MinInstanceBufferSize = sizeof(dev_ctx->config);

    WDF_WMI_INSTANCE_CONFIG_INIT_PROVIDER_CONFIG(&instance_cfg, &provider_cfg);
    instance_cfg.Register = TRUE;
    instance_cfg.EvtWmiInstanceQueryInstance = cx_wmi_config_query;
    instance_cfg.EvtWmiInstanceSetInstance = cx_wmi_config_set;
    instance_cfg.EvtWmiInstanceSetItem = cx_wmi_config_set_item;

    status = WdfWmiInstanceCreate(dev_ctx->dev, &instance_cfg, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfWmiInstanceCreate (config) failed with status %!STATUS!", status);
        return status;
    }

    // path
    WDF_WMI_PROVIDER_CONFIG_INIT(&provider_cfg, &GUID_WMI_CXADCWIN_PATH);
    provider_cfg.MinInstanceBufferSize = sizeof(USHORT);

    WDF_WMI_INSTANCE_CONFIG_INIT_PROVIDER_CONFIG(&instance_cfg, &provider_cfg);
    instance_cfg.Register = TRUE;
    instance_cfg.EvtWmiInstanceQueryInstance = cx_wmi_path_query;

    status = WdfWmiInstanceCreate(dev_ctx->dev, &instance_cfg, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfWmiInstanceCreate (path) failed with status %!STATUS!", status);
        return status;
    }

    return status;
}

NTSTATUS cx_wmi_state_query(
    _In_ WDFWMIINSTANCE instance,
    _In_ ULONG out_buf_size,
    _Out_writes_bytes_to_(out_buf_size, *buf_used) PVOID out_buf,
    _Out_ PULONG buf_used
)
{
    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(WdfWmiInstanceGetDevice(instance));

    *buf_used = sizeof(dev_ctx->state);

    if (out_buf_size < sizeof(dev_ctx->state))
    {
        return STATUS_BUFFER_TOO_SMALL;
    }

    RtlZeroMemory(out_buf, out_buf_size);
    RtlCopyMemory(out_buf, &dev_ctx->state, sizeof(dev_ctx->state));

    return STATUS_SUCCESS;
}

NTSTATUS cx_wmi_state_exe(
    _In_ WDFWMIINSTANCE instance,
    _In_ ULONG method_id,
    _In_ ULONG in_buf_size,
    _In_ ULONG out_buf_size,
    _When_(in_buf_size >= out_buf_size, _Inout_updates_bytes_(in_buf_size))
    _When_(in_buf_size < out_buf_size, _Inout_updates_bytes_(out_buf_size))
    PVOID buf,
    _Out_ PULONG buf_used
)
{
    UNREFERENCED_PARAMETER(in_buf_size);

    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(WdfWmiInstanceGetDevice(instance));

    switch (method_id)
    {
    case CX_CTRL_STATE_RESET_OUFLOW_WMI_ID:
    {
        if (out_buf_size != sizeof(ULONG))
        {
            return STATUS_BUFFER_TOO_SMALL;
        }

        *buf_used = sizeof(ULONG);

        ULONG value = cx_ctrl_reset_ouflow_count(dev_ctx);

        RtlZeroMemory(buf, out_buf_size);
        RtlCopyMemory(buf, &value, sizeof(ULONG));

        break;
    }

    default:
        return STATUS_INVALID_PARAMETER;
    }

    return STATUS_SUCCESS;
}

NTSTATUS cx_wmi_config_query(
    _In_ WDFWMIINSTANCE instance,
    _In_ ULONG out_buf_size,
    _Out_writes_bytes_to_(out_buf_size, *buf_used) PVOID out_buf,
    _Out_ PULONG buf_used
)
{
    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(WdfWmiInstanceGetDevice(instance));

    *buf_used = sizeof(dev_ctx->config);

    if (out_buf_size < sizeof(dev_ctx->config))
    {
        return STATUS_BUFFER_TOO_SMALL;
    }

    RtlZeroMemory(out_buf, out_buf_size);
    RtlCopyMemory(out_buf, &dev_ctx->config, sizeof(dev_ctx->config));

    return STATUS_SUCCESS;
}

NTSTATUS cx_wmi_config_set(
    _In_ WDFWMIINSTANCE instance,
    _In_ ULONG in_buf_size,
    _In_reads_bytes_(in_buf_size) PVOID in_buf
)
{
    NTSTATUS status = STATUS_SUCCESS;
    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(WdfWmiInstanceGetDevice(instance));

    if (in_buf_size < sizeof(dev_ctx->config))
    {
        return STATUS_INVALID_PARAMETER;
    }

    DEVICE_CONFIG config = *(PDEVICE_CONFIG)in_buf;

    status = cx_ctrl_set_vmux(dev_ctx, config.vmux);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = cx_ctrl_set_level(dev_ctx, config.level);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = cx_ctrl_set_tenbit(dev_ctx, config.tenbit);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = cx_ctrl_set_sixdb(dev_ctx, config.sixdb);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = cx_ctrl_set_center_offset(dev_ctx, config.center_offset);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    return status;
}

NTSTATUS cx_wmi_config_set_item(
    _In_ WDFWMIINSTANCE instance,
    _In_ ULONG id,
    _In_ ULONG in_buf_size,
    _In_reads_bytes_(in_buf_size) PVOID in_buf
)
{
    NTSTATUS status = STATUS_SUCCESS;
    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(WdfWmiInstanceGetDevice(instance));

    if (in_buf_size != sizeof(ULONG))
    {
        return STATUS_INVALID_PARAMETER;
    }

    ULONG value = *(PULONG)in_buf;

    switch (id)
    {
    case CX_CTRL_CONFIG_VMUX_WMI_ID:
        status = cx_ctrl_set_vmux(dev_ctx, value);
        break;

    case CX_CTRL_CONFIG_LEVEL_WMI_ID:
        status = cx_ctrl_set_level(dev_ctx, value);
        break;

    case CX_CTRL_CONFIG_TENBIT_WMI_ID:
        status = cx_ctrl_set_tenbit(dev_ctx, value ? TRUE : FALSE);
        break;

    case CX_CTRL_CONFIG_SIXDB_WMI_ID:
        status = cx_ctrl_set_sixdb(dev_ctx, value ? TRUE : FALSE);
        break;

    case CX_CTRL_CONFIG_CENTER_OFFSET_WMI_ID:
        status = cx_ctrl_set_center_offset(dev_ctx, value);
        break;

    default:
        status = STATUS_WMI_ITEMID_NOT_FOUND;
        break;
    }

    return status;
}

NTSTATUS cx_wmi_path_query(
    _In_ WDFWMIINSTANCE instance,
    _In_ ULONG out_buf_size,
    _Out_writes_bytes_to_(out_buf_size, *buf_used) PVOID out_buf,
    _Out_ PULONG buf_used
)
{
    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(WdfWmiInstanceGetDevice(instance));

    DECLARE_UNICODE_STRING_SIZE(symlink_path, 128);
    RtlUnicodeStringPrintf(&symlink_path, L"%ws%d", WIN32_PATH, dev_ctx->dev_idx);

    ULONG size = symlink_path.Length + sizeof(USHORT);

    if (out_buf_size < size)
    {
        return STATUS_BUFFER_TOO_SMALL;
    }

    *buf_used = size;
    RtlZeroMemory(out_buf, out_buf_size);

    return WDF_WMI_BUFFER_APPEND_STRING((PUCHAR)out_buf, size, &symlink_path, &size);
}
