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

#pragma once

VOID cx_init(_Inout_ PDEVICE_CONTEXT dev_ctx);
VOID cx_disable(_Inout_ PMMIO mmio);
VOID cx_reset(_Inout_ PMMIO mmio);
VOID cx_init_cdt(_Inout_ PMMIO mmio);
VOID cx_init_risc(_Inout_ PRISC risc);
VOID cx_init_cmds(_Inout_ PMMIO mmio, _In_ PRISC risc);

EVT_WDF_INTERRUPT_ISR cx_evt_isr;
EVT_WDF_INTERRUPT_DPC cx_evt_dpc;
EVT_WDF_INTERRUPT_ENABLE cx_evt_intr_enable;
EVT_WDF_INTERRUPT_DISABLE cx_evt_intr_disable;

VOID cx_start_capture(_Inout_ PDEVICE_CONTEXT dev_ctx);
VOID cx_stop_capture(_Inout_ PDEVICE_CONTEXT dev_ctx);
VOID cx_set_vmux(_Inout_ PMMIO mmio, _In_ ULONG vmux);
VOID cx_set_level(_Inout_ PMMIO mmio, _In_ ULONG level, _In_ BOOLEAN enable_sixdb);
VOID cx_set_tenbit(_Inout_ PMMIO mmio, _In_ BOOLEAN enable_tenbit);
VOID cx_set_center_offset(_Inout_ PMMIO mmio, _In_ ULONG center_offset);
BOOLEAN cx_get_ouflow_state(_Inout_ PMMIO mmio);
VOID cx_reset_ouflow_state(_Inout_ PMMIO mmio);

inline static
ULONG cx_read(
    _Inout_ PMMIO mmio,
    _In_ ULONG off
)
{
    return READ_REGISTER_ULONG(&mmio->base[off >> 2]);
}

inline static
VOID cx_write(
    _Inout_ PMMIO mmio,
    _In_ ULONG off,
    _In_ ULONG val
)
{
    WRITE_REGISTER_ULONG(&mmio->base[off >> 2], val);
}

inline static
VOID cx_write_buf(
    _Inout_ PMMIO mmio,
    _In_ ULONG off,
    _In_reads_(count) PUCHAR buf,
    _In_ ULONG count
)
{
    WRITE_REGISTER_BUFFER_UCHAR((volatile PUCHAR)&mmio->base[off >> 2], buf, count);
}
