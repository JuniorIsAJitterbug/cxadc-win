// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * cxadc-win - CX2388x ADC DMA driver for Windows
 *
 * Copyright (C) 2024 Jitterbug
 *
 * Based on the Linux version created by
 * Copyright (C) 2005-2007 Hew How Chee <how_chee@yahoo.com>
 * Copyright (C) 2013-2015 Chad Page <Chad.Page@gmail.com>
 * Copyright (C) 2019-2023 Adam Sampson <ats@offog.org>
 * Copyright (C) 2020-2022 Tony Anderson <tandersn@cs.washington.edu>
 */

#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <initguid.h>
#include <wdfobject.h>
#include <evntrace.h>
#include <Ntstrsafe.h>

#define CX_CDT_BUF_LEN          2048
#define CX_CDT_BUF_COUNT        8
#define CX_VBI_BUF_SIZE         (1024 * 1024 * 64)
#define CX_VBI_BUF_COUNT        (CX_VBI_BUF_SIZE / PAGE_SIZE)
#define CX_RISC_INSTR_BUF_SIZE  (CX_VBI_BUF_SIZE / CX_CDT_BUF_LEN) * 8 + PAGE_SIZE
#define READ_TIMEOUT            5000

typedef struct _DMA_DATA
{
    WDFCOMMONBUFFER buf;
    size_t len;
    PUCHAR va;
    PHYSICAL_ADDRESS la;
} DMA_DATA, *PDMA_DATA;

typedef struct _DEVICE_ATTRS
{
    LONG vmux;
    LONG level;
    LONG tenbit;
    LONG sixdb;
    LONG crystal;
    LONG center_offset;
} DEVICE_ATTRS, *PDEVICE_ATTRS;

typedef struct _DEVICE_CONTEXT
{
    WDFDEVICE dev;
    ULONG dev_idx;
    UNICODE_STRING symlink_path;

    WDFINTERRUPT intr;
    WDFQUEUE queue;
    KEVENT isr_event;

    ULONG gp_cnt;
    ULONG initial_page;
    size_t read_offset;
    size_t read_last_offset;
    WDFTIMER read_timer;
    BOOLEAN is_reading;

    DEVICE_ATTRS attrs;

    WDFDMAENABLER dma_enabler;

    PULONG mmio;
    ULONG mmio_len;

    DMA_DATA dma_risc_instr;
    DMA_DATA dma_risc_page[CX_VBI_BUF_COUNT + 1];
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, cx_device_get_ctx)

#if !defined(ASSOC_WRITE_REQUEST_WITH_DMA_TRANSACTION)
typedef struct TRANSACTION_CONTEXT {
    WDFREQUEST Request;
} TRANSACTION_CONTEXT, *PTRANSACTION_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(TRANSACTION_CONTEXT, cx_get_transaction_ctx)
#endif
