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

ULONG cx_ctrl_reset_ouflow_count(_In_ PDEVICE_CONTEXT dev_ctx);
NTSTATUS cx_ctrl_set_vmux(_In_ PDEVICE_CONTEXT dev_ctx, _In_ ULONG value);
NTSTATUS cx_ctrl_set_level(_In_ PDEVICE_CONTEXT dev_ctx, _In_ ULONG value);
NTSTATUS cx_ctrl_set_tenbit(_In_ PDEVICE_CONTEXT dev_ctx, _In_ BOOLEAN value);
NTSTATUS cx_ctrl_set_sixdb(_In_ PDEVICE_CONTEXT dev_ctx, _In_ BOOLEAN value);
NTSTATUS cx_ctrl_set_center_offset(_In_ PDEVICE_CONTEXT dev_ctx, _In_ ULONG value);
