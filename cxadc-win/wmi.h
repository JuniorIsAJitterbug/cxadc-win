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

#define MOF_RESOURCE_NAME L"CxadcWinWMI"

DEFINE_GUID(GUID_WMI_CXADCWIN_STATE,
    0x1F539677, 0x2EA1, 0x432E, 0xA3, 0xBC, 0x42, 0xD7, 0x3B, 0xB9, 0xDA, 0x5C);

DEFINE_GUID(GUID_WMI_CXADCWIN_CONFIG,
    0xE333ADBB, 0x4FD1, 0x4D13, 0xA6, 0x1A, 0x14, 0x4E, 0xAD, 0xC2, 0xDC, 0x4D);

DEFINE_GUID(GUID_WMI_CXADCWIN_PATH,
    0xC5DE3644, 0xCB5A, 0x4A18, 0x9F, 0x42, 0x47, 0x34, 0x8A, 0xC8, 0xFC, 0x68);

NTSTATUS cx_wmi_register(_In_ PDEVICE_CONTEXT dev_ctx);

EVT_WDF_WMI_INSTANCE_QUERY_INSTANCE cx_wmi_state_query;
EVT_WDF_WMI_INSTANCE_EXECUTE_METHOD cx_wmi_state_exe;

EVT_WDF_WMI_INSTANCE_QUERY_INSTANCE cx_wmi_config_query;
EVT_WDF_WMI_INSTANCE_SET_INSTANCE cx_wmi_config_set;
EVT_WDF_WMI_INSTANCE_SET_ITEM cx_wmi_config_set_item;

EVT_WDF_WMI_INSTANCE_QUERY_INSTANCE cx_wmi_path_query;
