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

#include <ntddk.h>
#include <wdf.h>
#include <initguid.h>
#include <wdfobject.h>
#include <evntrace.h>
#include <Ntstrsafe.h>

#define SYMLINK_PATH            L"\\DosDevices\\cxadc"
#define WIN32_PATH              L"\\\\.\\cxadc"

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

typedef struct _DEVICE_CONFIG
{
    LONG vmux;
    LONG level;
    BOOLEAN tenbit;
    BOOLEAN sixdb;
    LONG center_offset;
} DEVICE_CONFIG, *PDEVICE_CONFIG;

typedef struct _DEVICE_STATE
{
    LONG last_gp_cnt;
    LONG initial_page;
    
    ULONG ouflow_count;

    LONG reader_count;
    BOOLEAN is_capturing;
} DEVICE_STATE, *PDEVICE_STATE;

typedef struct _DEVICE_CONTEXT
{
    WDFDEVICE dev;
    ULONG dev_idx;
    ULONG bus_number;
    ULONG dev_addr;

    WDFINTERRUPT intr;
    WDFQUEUE control_queue;
    WDFQUEUE read_queue;
    KEVENT isr_event;

    DEVICE_CONFIG config;
    DEVICE_STATE state;

    WDFDMAENABLER dma_enabler;

    PULONG mmio;
    ULONG mmio_len;
    PMDL user_mdl;

    DMA_DATA dma_risc_instr;
    DMA_DATA dma_risc_page[CX_VBI_BUF_COUNT];
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, cx_device_get_ctx)

typedef struct _MMAP_DATA
{
    PVOID ptr;
} MMAP_DATA, *PMMAP_DATA;

typedef struct _FILE_CONTEXT
{
    LONG64 read_offset;
    MMAP_DATA mmap_data;
} FILE_CONTEXT, *PFILE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(FILE_CONTEXT, cx_file_get_ctx)
