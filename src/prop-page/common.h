// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * cxadc-win-prop-page - Property Page for the cxadc-win driver
 *
 * Copyright (C) 2024-2025 Jitterbug <jitterbug@posteo.co.uk>
 */

#pragma once

#define BG_COLOR                        RGB(249, 249, 249)
#define FG_COLOR                        RGB(0, 0, 0)

#define _HeapAllocZero(size)            HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (size))
#define _HeapFree(p)                    HeapFree(GetProcessHeap(), 0, (p))
#define _SafeHeapFree(p)                do { if ((p) != NULL && _HeapFree(p)) { (p) = NULL; } } while(0)
#define _SafeCloseHandle(h)             do { if ((h) != NULL && (h) != INVALID_HANDLE_VALUE && CloseHandle(h)) { (h) = NULL; } } while(0)

#define DEBUG_LOG(str, ...)             _RPTFWN(_CRT_WARN, (L##str L", lasterr=%d, errno=%d\n")__VA_OPT__(,) __VA_ARGS__, GetLastError(), _doserrno)
#define DEBUG_ERROR(str, ...)           _RPTFWN(_CRT_ERROR, (L##str L", lasterr=%d, errno=%d\n")__VA_OPT__(,) __VA_ARGS__, GetLastError(), _doserrno)
#define RETURN_HR_IF_FAILED(expr)       do { HRESULT _hr = (expr); if (FAILED(_hr)) { DEBUG_ERROR(#expr); return _hr; } } while(0)

#define HRESULT_FROM_LASTERROR          HRESULT_FROM_WIN32(GetLastError())
#define HRESULT_FROM_ERRNO              HRESULT_FROM_WIN32(_doserrno)

#define DEVICE_ID_STR                   L"VEN_14F1&DEV_8800"

typedef struct _CX_PROP_DATA
{
    PWCHAR dev_id;
    PWCHAR dev_path;
    DEVICE_CONFIG config;
    DEVICE_CONFIG prev_config;
    DEVICE_STATE state;
    DEVICE_STATE prev_state;
    WCHAR win32_path[MAX_PATH];
} CX_PROP_DATA, * PCX_PROP_DATA;
