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

#define WPP_CHECK_FOR_NULL_STRING  //to prevent exceptions due to NULL strings

// EBC6EBEE-D896-4C53-B6DC-B732CB13E892
#define WPP_CONTROL_GUIDS \
    WPP_DEFINE_CONTROL_GUID(cx_trace_guid, (EBC6EBEE, D896, 4C53, B6DC, B732CB13E892),\
        WPP_DEFINE_BIT(DBG_GENERAL)             /* bit  0 = 0x00000001 */ \
        )

#define WPP_LEVEL_FLAGS_LOGGER(lvl,flags) WPP_LEVEL_LOGGER(flags)
#define WPP_LEVEL_FLAGS_ENABLED(lvl, flags) (WPP_LEVEL_ENABLED(flags) && WPP_CONTROL(WPP_BIT_ ## flags).Level  >= lvl)
