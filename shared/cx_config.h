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

DEFINE_GUID(GUID_DEVINTERFACE_CXADCWIN,
    0x13EF40B0, 0x05FF, 0x4173, 0xB6, 0x13, 0x31, 0x41, 0xAD, 0x2E, 0x37, 0x62);

typedef struct _DEVICE_CONFIG
{
    ULONG vmux;
    ULONG level;
    BOOLEAN tenbit;
    BOOLEAN sixdb;
    ULONG center_offset;
} DEVICE_CONFIG, * PDEVICE_CONFIG;

typedef struct _DEVICE_STATE
{
    ULONG last_gp_cnt;
    ULONG initial_page;

    ULONG ouflow_count;

    ULONG reader_count;
    BOOLEAN is_capturing;
} DEVICE_STATE, * PDEVICE_STATE;
