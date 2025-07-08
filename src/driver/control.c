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
#include "control.tmh"
#include "control.h"

#include "cx2388x.h"
#include "registry.h"

_Use_decl_annotations_
ULONG cx_ctrl_reset_ouflow_count(
    PDEVICE_CONTEXT dev_ctx
)
{
    TRACE_INFO("resetting over/underflow count (current: %d)", dev_ctx->state.ouflow_count);
    ULONG cur = dev_ctx->state.ouflow_count;
    dev_ctx->state.ouflow_count = 0;

    return cur;
}

_Use_decl_annotations_
NTSTATUS cx_ctrl_set_vmux(
    PDEVICE_CONTEXT dev_ctx,
    ULONG value
)
{
    if (value > CX_CTRL_CONFIG_VMUX_MAX)
    {
        TRACE_ERROR("invalid vmux %d", value);
        return STATUS_INVALID_PARAMETER;
    }

    TRACE_INFO("setting vmux to %d (currently %d)", value, dev_ctx->config.vmux);

    InterlockedExchange((PLONG)&dev_ctx->config.vmux, value);
    cx_set_vmux(&dev_ctx->mmio, value);

    return cx_reg_set_value(dev_ctx->dev, CX_CTRL_CONFIG_VMUX_REG_KEY, value);
}

_Use_decl_annotations_
NTSTATUS cx_ctrl_set_level(
    PDEVICE_CONTEXT dev_ctx,
    ULONG value
)
{
    if (value > CX_CTRL_CONFIG_LEVEL_MAX)
    {
        TRACE_ERROR("invalid level %d", value);
        return STATUS_INVALID_PARAMETER;
    }

    TRACE_INFO("setting level to %d (currently %d)", value, dev_ctx->config.level);

    InterlockedExchange((PLONG)&dev_ctx->config.level, value);
    cx_set_level(&dev_ctx->mmio, value, dev_ctx->config.sixdb);

    return cx_reg_set_value(dev_ctx->dev, CX_CTRL_CONFIG_LEVEL_REG_KEY, value);
}

_Use_decl_annotations_
NTSTATUS cx_ctrl_set_tenbit(
    PDEVICE_CONTEXT dev_ctx,
    BOOLEAN value
)
{
    TRACE_INFO("setting tenbit to %d (currently %d)", value, dev_ctx->config.tenbit);

    InterlockedExchange8((PCHAR)&dev_ctx->config.tenbit, value);
    cx_set_tenbit(&dev_ctx->mmio, value);

    return cx_reg_set_value(dev_ctx->dev, CX_CTRL_CONFIG_TENBIT_REG_KEY, value);
}

_Use_decl_annotations_
NTSTATUS cx_ctrl_set_sixdb(
    PDEVICE_CONTEXT dev_ctx,
    BOOLEAN value
)
{
    TRACE_INFO("setting sixdb to %d (currently %d)", value, dev_ctx->config.sixdb);

    InterlockedExchange8((PCHAR)&dev_ctx->config.sixdb, value);
    cx_set_level(&dev_ctx->mmio, dev_ctx->config.level, value ? TRUE : FALSE);

    return cx_reg_set_value(dev_ctx->dev, CX_CTRL_CONFIG_SIXDB_REG_KEY, value);
}

_Use_decl_annotations_
NTSTATUS cx_ctrl_set_center_offset(
    PDEVICE_CONTEXT dev_ctx,
    ULONG value
)
{
    if (value > CX_CTRL_CONFIG_CENTER_OFFSET_MAX)
    {
        TRACE_ERROR("invalid center_offset %d", value);
        return STATUS_INVALID_PARAMETER;
    }

    TRACE_INFO("setting center_offset to %d (currently %d)", value, dev_ctx->config.center_offset);

    InterlockedExchange((PLONG)&dev_ctx->config.center_offset, value);
    cx_set_center_offset(&dev_ctx->mmio, value);

    return cx_reg_set_value(dev_ctx->dev, CX_CTRL_CONFIG_CENTER_OFFSET_REG_KEY, value);
}

_Use_decl_annotations_
NTSTATUS cx_ctrl_get_register(
    PDEVICE_CONTEXT dev_ctx,
    ULONG address,
    PULONG value
)
{
    if (address < CX_REGISTER_BASE || address > CX_REGISTER_END)
    {
        TRACE_ERROR("get register %08X out of range", address);
        return STATUS_INVALID_PARAMETER;
    }

    *value = cx_read(&dev_ctx->mmio, address);
    TRACE_INFO("register %08X is %08X", address, *value);

    return STATUS_SUCCESS;
}


_Use_decl_annotations_
NTSTATUS cx_ctrl_set_register(
    PDEVICE_CONTEXT dev_ctx,
    PSET_REGISTER_DATA data
)
{
    if (data->addr < CX_REGISTER_BASE || data->addr  > CX_REGISTER_END)
    {
        TRACE_ERROR("set register %08X out of range", data->addr);
        return STATUS_INVALID_PARAMETER;
    }

    TRACE_INFO("setting register %08X to %08X", data->addr, data->val);
    cx_write(&dev_ctx->mmio, data->addr, data->val);

    return STATUS_SUCCESS;
}
