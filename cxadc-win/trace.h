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

// EBC6EBEE-D896-4C53-B6DC-B732CB13E892
#define WPP_CONTROL_GUIDS \
    WPP_DEFINE_CONTROL_GUID(cx_trace_guid, (EBC6EBEE, D896, 4C53, B6DC, B732CB13E892), \
        WPP_DEFINE_BIT(GENERAL)     \
        WPP_DEFINE_BIT(IOCTL))

#define WPP_LEVEL_FLAGS_LOGGER(lvl, flags)  \
    WPP_LEVEL_LOGGER(flags)

#define WPP_LEVEL_FLAGS_ENABLED(lvl, flags) \
    (WPP_LEVEL_ENABLED(flags) && WPP_CONTROL(WPP_BIT_ ## flags).Level  >= lvl)


// TRACE_INFO
// log info
//
// begin_wpp config
// FUNC TRACE_INFO{LEVEL = TRACE_LEVEL_INFORMATION, FLAGS = GENERAL}(MSG, ...);
// USEPREFIX (TRACE_INFO, "%!STDPREFIX!:%!SPACE!");
// end_wpp


// TRACE_ERROR
// log err
//
// begin_wpp config
// FUNC TRACE_ERROR{LEVEL = TRACE_LEVEL_ERROR, FLAGS = GENERAL}(MSG, ...);
// USEPREFIX (TRACE_ERROR, "%!STDPREFIX! ERROR in %!FUNC! [%!FILE! @ %!LINE!]:%!SPACE!");
// end_wpp


// TRACE_STATUS_ERROR
// log status err
//
// begin_wpp config
// FUNC TRACE_STATUS_ERROR{LEVEL = TRACE_LEVEL_ERROR, FLAGS = GENERAL} (STATUS, MSG, ...);
// USEPREFIX (TRACE_STATUS_ERROR, "%!STDPREFIX! ERROR in %!FUNC! [%!FILE! @ %!LINE!]:%!SPACE!");
// USESUFFIX (TRACE_STATUS_ERROR, ", status=%!STATUS!", STATUS);
// end_wpp
#define WPP_RECORDER_LEVEL_FLAGS_STATUS_ARGS(LEVEL, FLAGS, STATUS)      \
    WPP_RECORDER_LEVEL_FLAGS_ARGS(LEVEL, FLAGS)

#define WPP_RECORDER_LEVEL_FLAGS_STATUS_FILTER(LEVEL, FLAGS, STATUS)    \
    WPP_RECORDER_LEVEL_FLAGS_FILTER(LEVEL, FLAGS)


// RETURN_NTSTATUS_IF_FAILED
// log err and return status if !NT_SUCCESS
//
// begin_wpp config
// FUNC RETURN_NTSTATUS_IF_FAILED{LEVEL = TRACE_LEVEL_ERROR, FLAGS = GENERAL} (RET_STATUS);
// USEPREFIX (RETURN_NTSTATUS_IF_FAILED, "%!STDPREFIX! ERROR in %!FUNC! [%!FILE! @ %!LINE!]: ");
// USESUFFIX (RETURN_NTSTATUS_IF_FAILED, " status=%!STATUS!", __status);
// end_wpp
#define WPP_LEVEL_FLAGS_RET_STATUS_PRE(LEVEL, FLAGS, RET_STATUS)    \
    do {                                                            \
        NTSTATUS __status = (RET_STATUS);                           \
        if (!NT_SUCCESS(__status))                                  \
        {

#define WPP_LEVEL_FLAGS_RET_STATUS_POST(LEVEL, FLAGS, RET_STATUS)   \
            ;                                                       \
            return __status;                                        \
        }                                                           \
    } while(0)

#define WPP_RECORDER_LEVEL_FLAGS_RET_STATUS_ARGS(LEVEL, FLAGS, RET_STATUS)      \
    WPP_RECORDER_LEVEL_FLAGS_ARGS(LEVEL, FLAGS)

#define WPP_RECORDER_LEVEL_FLAGS_RET_STATUS_FILTER(LEVEL, FLAGS, RET_STATUS)    \
    WPP_RECORDER_LEVEL_FLAGS_FILTER(LEVEL, FLAGS)


// RETURN_COMPLETE_WDFREQUEST_IF_FAILED
// log err, complete wdf req and return if !NT_SUCCESS
//
// begin_wpp config
// FUNC RETURN_COMPLETE_WDFREQUEST_IF_FAILED{LEVEL = TRACE_LEVEL_ERROR, FLAGS = IOCTL} (WDF_REQ, RET_STATUS);
// USEPREFIX (RETURN_COMPLETE_WDFREQUEST_IF_FAILED, "%!STDPREFIX! ERROR in %!FUNC! [%!FILE! @ %!LINE!]: ");
// USESUFFIX (RETURN_COMPLETE_WDFREQUEST_IF_FAILED, " status=%!STATUS!", __status);
// end_wpp
#define WPP_LEVEL_FLAGS_WDF_REQ_RET_STATUS_PRE(LEVEL, FLAGS, WDF_REQ, RET_STATUS)   \
    do {                                                                            \
        NTSTATUS __status = (RET_STATUS);                                           \
        if (!NT_SUCCESS(__status))                                                  \
        {

#define WPP_LEVEL_FLAGS_WDF_REQ_RET_STATUS_POST(LEVEL, FLAGS, WDF_REQ, RET_STATUS)  \
            ;                                                                       \
            WdfRequestComplete((WDF_REQ), __status);                                \
            return;                                                                 \
        }                                                                           \
    } while(0)

#define WPP_RECORDER_LEVEL_FLAGS_WDF_REQ_RET_STATUS_ARGS(LEVEL, FLAGS, WDF_REQ, RET_STATUS)   \
    WPP_RECORDER_LEVEL_FLAGS_ARGS(LEVEL, FLAGS)

#define WPP_RECORDER_LEVEL_FLAGS_WDF_REQ_RET_STATUS_FILTER(LEVEL, FLAGS, WDF_REQ, RET_STATUS) \
    WPP_RECORDER_LEVEL_FLAGS_FILTER(LEVEL, FLAGS)
