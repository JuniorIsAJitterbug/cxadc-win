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

    // CxadcWin_State (1F539677-2EA1-432E-A3BC-42D73BB9DA5C)
    {
        WDF_WMI_PROVIDER_CONFIG provider_cfg;
        WDF_WMI_PROVIDER_CONFIG_INIT(&provider_cfg, &GUID_WMI_CXADCWIN_STATE);
        provider_cfg.MinInstanceBufferSize = sizeof(dev_ctx->state);

        WDF_WMI_INSTANCE_CONFIG instance_cfg;
        WDF_WMI_INSTANCE_CONFIG_INIT_PROVIDER_CONFIG(&instance_cfg, &provider_cfg);
        instance_cfg.Register = TRUE;
        instance_cfg.EvtWmiInstanceQueryInstance = cx_wmi_state_query;
        instance_cfg.EvtWmiInstanceExecuteMethod = cx_wmi_state_exe;

        RETURN_NTSTATUS_IF_FAILED(WdfWmiInstanceCreate(dev_ctx->dev, &instance_cfg, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE));
    }

    // CxadcWin_Config (E333ADBB-4FD1-4D13-A61A-144EADC2DC4D)
    {
        WDF_WMI_PROVIDER_CONFIG provider_cfg;
        WDF_WMI_PROVIDER_CONFIG_INIT(&provider_cfg, &GUID_WMI_CXADCWIN_CONFIG);
        provider_cfg.MinInstanceBufferSize = sizeof(dev_ctx->config);

        WDF_WMI_INSTANCE_CONFIG instance_cfg;
        WDF_WMI_INSTANCE_CONFIG_INIT_PROVIDER_CONFIG(&instance_cfg, &provider_cfg);
        instance_cfg.Register = TRUE;
        instance_cfg.EvtWmiInstanceQueryInstance = cx_wmi_config_query;
        instance_cfg.EvtWmiInstanceSetInstance = cx_wmi_config_set;
        instance_cfg.EvtWmiInstanceSetItem = cx_wmi_config_set_item;

        RETURN_NTSTATUS_IF_FAILED(WdfWmiInstanceCreate(dev_ctx->dev, &instance_cfg, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE));
    }

    // CxadcWin_Path (C5DE3644-CB5A-4A18-9F42-47348AC8FC68)
    {
        WDF_WMI_PROVIDER_CONFIG provider_cfg;
        WDF_WMI_PROVIDER_CONFIG_INIT(&provider_cfg, &GUID_WMI_CXADCWIN_PATH);
        provider_cfg.MinInstanceBufferSize = sizeof(USHORT);

        WDF_WMI_INSTANCE_CONFIG instance_cfg;
        WDF_WMI_INSTANCE_CONFIG_INIT_PROVIDER_CONFIG(&instance_cfg, &provider_cfg);
        instance_cfg.Register = TRUE;
        instance_cfg.EvtWmiInstanceQueryInstance = cx_wmi_path_query;

        RETURN_NTSTATUS_IF_FAILED(WdfWmiInstanceCreate(dev_ctx->dev, &instance_cfg, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE));
    }

    // CxadcWin_Registers (6B08A64E-25FB-4112-A5C2-86D704A0527E)
    {
        WDF_WMI_PROVIDER_CONFIG provider_cfg;
        WDF_WMI_PROVIDER_CONFIG_INIT(&provider_cfg, &GUID_WMI_CXADCWIN_REGISTERS);
        provider_cfg.MinInstanceBufferSize = sizeof(ULONG);

        WDF_WMI_INSTANCE_CONFIG instance_cfg;
        WDF_WMI_INSTANCE_CONFIG_INIT_PROVIDER_CONFIG(&instance_cfg, &provider_cfg);
        instance_cfg.Register = TRUE;
        instance_cfg.EvtWmiInstanceExecuteMethod = cx_wmi_registers_exe;

        RETURN_NTSTATUS_IF_FAILED(WdfWmiInstanceCreate(dev_ctx->dev, &instance_cfg, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE));
    }

    // CxadcWin_Hardware (A1D72BCC-01A9-4AED-A370-B376AB3C90AC)
    {
        WDF_WMI_PROVIDER_CONFIG provider_cfg;
        WDF_WMI_PROVIDER_CONFIG_INIT(&provider_cfg, &GUID_WMI_CXADCWIN_HARDWARE);
        provider_cfg.MinInstanceBufferSize = sizeof(HARDWARE_DATA);

        WDF_WMI_INSTANCE_CONFIG instance_cfg;
        WDF_WMI_INSTANCE_CONFIG_INIT_PROVIDER_CONFIG(&instance_cfg, &provider_cfg);
        instance_cfg.Register = TRUE;
        instance_cfg.EvtWmiInstanceQueryInstance = cx_wmi_hardware_query;

        RETURN_NTSTATUS_IF_FAILED(WdfWmiInstanceCreate(dev_ctx->dev, &instance_cfg, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE));
    }

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
        case CX_CTRL_STATE_OUFLOW_RESET_WMI_ID:
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

    DECLARE_UNICODE_STRING_SIZE(symlink_path, MAX_PATH);
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

_Use_decl_annotations_
NTSTATUS cx_wmi_registers_exe(
    WDFWMIINSTANCE instance,
    ULONG method_id,
    ULONG in_buf_size,
    ULONG out_buf_size,
    PVOID buf,
    PULONG buf_used
)
{
    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(WdfWmiInstanceGetDevice(instance));

    switch (method_id)
    {
    case CX_CTRL_HW_REGISTER_GET_WMI_ID:
    {
        if (in_buf_size != sizeof(ULONG))
        {
            return STATUS_INVALID_PARAMETER;
        }

        if (out_buf_size != sizeof(ULONG))
        {
            return STATUS_BUFFER_TOO_SMALL;
        }

        ULONG in_buf = *(PULONG)buf;
        PULONG out_buf = (PULONG)buf;
        RETURN_NTSTATUS_IF_FAILED(cx_ctrl_get_register(dev_ctx, in_buf, out_buf));

        *buf_used = sizeof(ULONG);
        break;
    }

    case CX_CTRL_HW_REGISTER_SET_WMI_ID:
    {
        if (in_buf_size != sizeof(SET_REGISTER_DATA))
        {
            TRACE_INFO("invalid register set input, size %d", in_buf_size);
            return STATUS_INVALID_PARAMETER;
        }

        PSET_REGISTER_DATA in_buf = (PSET_REGISTER_DATA)buf;
        RETURN_NTSTATUS_IF_FAILED(cx_ctrl_set_register(dev_ctx, in_buf));

        *buf_used = 0;
        break;
    }

    default:
        return STATUS_INVALID_PARAMETER;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS cx_wmi_hardware_query(
    WDFWMIINSTANCE instance,
    ULONG out_buf_size,
    PVOID out_buf,
    PULONG buf_used
)
{
    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(WdfWmiInstanceGetDevice(instance));

    if (out_buf_size < sizeof(HARDWARE_DATA))
    {
        return STATUS_BUFFER_TOO_SMALL;
    }

    PHARDWARE_DATA hw_data = (PHARDWARE_DATA)out_buf;

    RtlZeroMemory(out_buf, out_buf_size);
    hw_data->bus_number = dev_ctx->bus_number;
    hw_data->dev_address = dev_ctx->dev_addr;

    *buf_used = sizeof(HARDWARE_DATA);

    return STATUS_SUCCESS;
}
