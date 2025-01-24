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

#include "common.h"

ULONG cx_ctrl_reset_ouflow_count(_In_ PDEVICE_CONTEXT dev_ctx);
NTSTATUS cx_ctrl_set_vmux(_In_ PDEVICE_CONTEXT dev_ctx, _In_ ULONG value);
NTSTATUS cx_ctrl_set_level(_In_ PDEVICE_CONTEXT dev_ctx, _In_ ULONG value);
NTSTATUS cx_ctrl_set_tenbit(_In_ PDEVICE_CONTEXT dev_ctx, _In_ BOOLEAN value);
NTSTATUS cx_ctrl_set_sixdb(_In_ PDEVICE_CONTEXT dev_ctx, _In_ BOOLEAN value);
NTSTATUS cx_ctrl_set_center_offset(_In_ PDEVICE_CONTEXT dev_ctx, _In_ ULONG value);

// state
#define CX_CTRL_STATE_RESET_OUFLOW_WMI_ID       1

// vmux
#define CX_CTRL_CONFIG_VMUX_WMI_ID              1
#define CX_CTRL_CONFIG_VMUX_DEFAULT             2
#define CX_CTRL_CONFIG_VMUX_MIN                 0
#define CX_CTRL_CONFIG_VMUX_MAX                 3

// level
#define CX_CTRL_CONFIG_LEVEL_WMI_ID             2
#define CX_CTRL_CONFIG_LEVEL_DEFAULT            16
#define CX_CTRL_CONFIG_LEVEL_MIN                0
#define CX_CTRL_CONFIG_LEVEL_MAX                31

// tenbit
#define CX_CTRL_CONFIG_TENBIT_WMI_ID            3
#define CX_CTRL_CONFIG_TENBIT_DEFAULT           0
#define CX_CTRL_CONFIG_TENBIT_MIN               0
#define CX_CTRL_CONFIG_TENBIT_MAX               1

// sixdb
#define CX_CTRL_CONFIG_SIXDB_WMI_ID             4
#define CX_CTRL_CONFIG_SIXDB_DEFAULT            0
#define CX_CTRL_CONFIG_SIXDB_MIN                0
#define CX_CTRL_CONFIG_SIXDB_MAX                1

// center offset
#define CX_CTRL_CONFIG_CENTER_OFFSET_WMI_ID     5
#define CX_CTRL_CONFIG_CENTER_OFFSET_DEFAULT    0
#define CX_CTRL_CONFIG_CENTER_OFFSET_MIN        0
#define CX_CTRL_CONFIG_CENTER_OFFSET_MAX        63
