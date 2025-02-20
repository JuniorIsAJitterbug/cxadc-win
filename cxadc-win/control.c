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
#include "control.tmh"

#include "control.h"
#include "cx2388x.h"
#include "registry.h"

_Use_decl_annotations_
ULONG cx_ctrl_reset_ouflow_count(
    PDEVICE_CONTEXT dev_ctx
)
{
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "resetting over/underflow count (current: %d)", dev_ctx->state.ouflow_count);
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
    NTSTATUS status = STATUS_SUCCESS;

    if (value > CX_CTRL_CONFIG_VMUX_MAX)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "invalid vmux %d", value);
        return STATUS_INVALID_PARAMETER;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "setting vmux to %d", value);

    dev_ctx->config.vmux = value;
    status = cx_reg_set_value(dev_ctx->dev, CX_CTRL_CONFIG_VMUX_REG_KEY, value);
    cx_set_vmux(dev_ctx);

    return status;
}

_Use_decl_annotations_
NTSTATUS cx_ctrl_set_level(
    PDEVICE_CONTEXT dev_ctx,
    ULONG value
)
{
    NTSTATUS status = STATUS_SUCCESS;

    if (value > CX_CTRL_CONFIG_LEVEL_MAX)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "invalid level %d", value);
        return STATUS_INVALID_PARAMETER;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "setting level to %d", value);

    dev_ctx->config.level = value;
    status = cx_reg_set_value(dev_ctx->dev, CX_CTRL_CONFIG_LEVEL_REG_KEY, value);
    cx_set_level(dev_ctx);

    return status;
}

_Use_decl_annotations_
NTSTATUS cx_ctrl_set_tenbit(
    PDEVICE_CONTEXT dev_ctx,
    BOOLEAN value
)
{
    NTSTATUS status = STATUS_SUCCESS;
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "setting tenbit to %d", value);

    dev_ctx->config.tenbit = value;
    status = cx_reg_set_value(dev_ctx->dev, CX_CTRL_CONFIG_TENBIT_REG_KEY, value);
    cx_set_tenbit(dev_ctx);

    return status;
}

_Use_decl_annotations_
NTSTATUS cx_ctrl_set_sixdb(
    PDEVICE_CONTEXT dev_ctx,
    BOOLEAN value
)
{
    NTSTATUS status = STATUS_SUCCESS;
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "setting sixdb to %d", value);

    dev_ctx->config.sixdb = value;
    status = cx_reg_set_value(dev_ctx->dev, CX_CTRL_CONFIG_SIXDB_REG_KEY, value);
    cx_set_level(dev_ctx);

    return status;
}

_Use_decl_annotations_
NTSTATUS cx_ctrl_set_center_offset(
    PDEVICE_CONTEXT dev_ctx,
    ULONG value
)
{
    NTSTATUS status = STATUS_SUCCESS;

    if (value > CX_CTRL_CONFIG_CENTER_OFFSET_MAX)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "invalid center_offset %d", value);
        return STATUS_INVALID_PARAMETER;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "setting center_offset to %d", value);

    dev_ctx->config.center_offset = value;
    status = cx_reg_set_value(dev_ctx->dev, CX_CTRL_CONFIG_CENTER_OFFSET_REG_KEY, value);
    cx_set_center_offset(dev_ctx);

    return status;
}
