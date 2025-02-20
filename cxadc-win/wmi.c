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
#include "wmi.tmh"
#include "wmi.h"

#include "control.h"

_Use_decl_annotations_
NTSTATUS cx_wmi_register(
    PDEVICE_CONTEXT dev_ctx
)
{
    DECLARE_CONST_UNICODE_STRING(mof_resource_name, MOF_RESOURCE_NAME);

    RETURN_NTSTATUS_IF_FAILED(WdfDeviceAssignMofResourceName(dev_ctx->dev, &mof_resource_name));

    WDF_WMI_PROVIDER_CONFIG provider_cfg;
    WDF_WMI_INSTANCE_CONFIG instance_cfg;

    // state
    WDF_WMI_PROVIDER_CONFIG_INIT(&provider_cfg, &GUID_WMI_CXADCWIN_STATE);
    provider_cfg.MinInstanceBufferSize = sizeof(dev_ctx->state);

    WDF_WMI_INSTANCE_CONFIG_INIT_PROVIDER_CONFIG(&instance_cfg, &provider_cfg);
    instance_cfg.Register = TRUE;
    instance_cfg.EvtWmiInstanceQueryInstance = cx_wmi_state_query;
    instance_cfg.EvtWmiInstanceExecuteMethod = cx_wmi_state_exe;

    RETURN_NTSTATUS_IF_FAILED(WdfWmiInstanceCreate(dev_ctx->dev, &instance_cfg, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE));

    // config
    WDF_WMI_PROVIDER_CONFIG_INIT(&provider_cfg, &GUID_WMI_CXADCWIN_CONFIG);
    provider_cfg.MinInstanceBufferSize = sizeof(dev_ctx->config);

    WDF_WMI_INSTANCE_CONFIG_INIT_PROVIDER_CONFIG(&instance_cfg, &provider_cfg);
    instance_cfg.Register = TRUE;
    instance_cfg.EvtWmiInstanceQueryInstance = cx_wmi_config_query;
    instance_cfg.EvtWmiInstanceSetInstance = cx_wmi_config_set;
    instance_cfg.EvtWmiInstanceSetItem = cx_wmi_config_set_item;

    RETURN_NTSTATUS_IF_FAILED(WdfWmiInstanceCreate(dev_ctx->dev, &instance_cfg, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE));

    // path
    WDF_WMI_PROVIDER_CONFIG_INIT(&provider_cfg, &GUID_WMI_CXADCWIN_PATH);
    provider_cfg.MinInstanceBufferSize = sizeof(USHORT);

    WDF_WMI_INSTANCE_CONFIG_INIT_PROVIDER_CONFIG(&instance_cfg, &provider_cfg);
    instance_cfg.Register = TRUE;
    instance_cfg.EvtWmiInstanceQueryInstance = cx_wmi_path_query;

    RETURN_NTSTATUS_IF_FAILED(WdfWmiInstanceCreate(dev_ctx->dev, &instance_cfg, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE));

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS cx_wmi_state_query(
    WDFWMIINSTANCE instance,
    ULONG out_buf_size,
    PVOID out_buf,
    PULONG buf_used
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

_Use_decl_annotations_
NTSTATUS cx_wmi_state_exe(
    WDFWMIINSTANCE instance,
    ULONG method_id,
    ULONG in_buf_size,
    ULONG out_buf_size,
    PVOID buf,
    PULONG buf_used
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

_Use_decl_annotations_
NTSTATUS cx_wmi_config_query(
    WDFWMIINSTANCE instance,
    ULONG out_buf_size,
    PVOID out_buf,
    PULONG buf_used
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

_Use_decl_annotations_
NTSTATUS cx_wmi_config_set(
    WDFWMIINSTANCE instance,
    ULONG in_buf_size,
    PVOID in_buf
)
{
    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(WdfWmiInstanceGetDevice(instance));

    if (in_buf_size < sizeof(dev_ctx->config))
    {
        return STATUS_INVALID_PARAMETER;
    }

    DEVICE_CONFIG config = *(PDEVICE_CONFIG)in_buf;

    RETURN_NTSTATUS_IF_FAILED(cx_ctrl_set_vmux(dev_ctx, config.vmux));
    RETURN_NTSTATUS_IF_FAILED(cx_ctrl_set_level(dev_ctx, config.level));
    RETURN_NTSTATUS_IF_FAILED(cx_ctrl_set_tenbit(dev_ctx, config.tenbit));
    RETURN_NTSTATUS_IF_FAILED(cx_ctrl_set_sixdb(dev_ctx, config.sixdb));
    RETURN_NTSTATUS_IF_FAILED(cx_ctrl_set_center_offset(dev_ctx, config.center_offset));

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS cx_wmi_config_set_item(
    WDFWMIINSTANCE instance,
    ULONG id,
    ULONG in_buf_size,
    PVOID in_buf
)
{
    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(WdfWmiInstanceGetDevice(instance));

    if (in_buf_size != sizeof(ULONG))
    {
        return STATUS_INVALID_PARAMETER;
    }

    ULONG value = *(PULONG)in_buf;

    switch (id)
    {
        case CX_CTRL_CONFIG_VMUX_WMI_ID:
            return cx_ctrl_set_vmux(dev_ctx, value);

        case CX_CTRL_CONFIG_LEVEL_WMI_ID:
            return cx_ctrl_set_level(dev_ctx, value);

        case CX_CTRL_CONFIG_TENBIT_WMI_ID:
            return cx_ctrl_set_tenbit(dev_ctx, value ? TRUE : FALSE);

        case CX_CTRL_CONFIG_SIXDB_WMI_ID:
            return cx_ctrl_set_sixdb(dev_ctx, value ? TRUE : FALSE);

        case CX_CTRL_CONFIG_CENTER_OFFSET_WMI_ID:
            return cx_ctrl_set_center_offset(dev_ctx, value);

        default:
            return STATUS_WMI_ITEMID_NOT_FOUND;
    }
}

_Use_decl_annotations_
NTSTATUS cx_wmi_path_query(
    WDFWMIINSTANCE instance,
    ULONG out_buf_size,
    PVOID out_buf,
    PULONG buf_used
)
{
    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(WdfWmiInstanceGetDevice(instance));

    DECLARE_UNICODE_STRING_SIZE(symlink_path, 128);
    RETURN_NTSTATUS_IF_FAILED(RtlUnicodeStringPrintf(&symlink_path, L"%ws%d", WIN32_PATH, dev_ctx->dev_idx));

    ULONG size = symlink_path.Length + sizeof(USHORT);

    if (out_buf_size < size)
    {
        return STATUS_BUFFER_TOO_SMALL;
    }

    *buf_used = size;
    RtlZeroMemory(out_buf, out_buf_size);
    RETURN_NTSTATUS_IF_FAILED(WDF_WMI_BUFFER_APPEND_STRING((PUCHAR)out_buf, size, &symlink_path, &size));

    return STATUS_SUCCESS;
}
