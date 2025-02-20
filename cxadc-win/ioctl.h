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

EVT_WDF_DEVICE_FILE_CREATE cx_evt_file_create;
EVT_WDF_FILE_CLOSE cx_evt_file_close;
EVT_WDF_FILE_CLEANUP cx_evt_file_cleanup;

NTSTATUS cx_evt_set_output(_In_ WDFREQUEST req, _Inout_updates_(buf_len)  PVOID buf, _In_ size_t buf_len);
NTSTATUS cx_evt_get_input(_In_ WDFREQUEST req, _In_reads_(buf_len) PVOID buf, _In_ size_t buf_len);

VOID cx_evt_io_ctrl(_In_ WDFQUEUE queue, _In_ WDFREQUEST req, _In_ size_t out_len, _In_ size_t in_len, _In_ ULONG ctrl_code);
VOID cx_evt_io_read(_In_ WDFQUEUE queue, _In_ WDFREQUEST req, _In_ size_t len);

__inline ULONG cx_get_page_no(_In_ ULONG initial_page, _In_ size_t off);

typedef struct _SET_REGISTER_DATA
{
    ULONG addr;
    ULONG val;
} SET_REGISTER_DATA, * PSET_REGISTER_DATA;
