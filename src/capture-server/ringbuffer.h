// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * capture-server - Windows port of cxadc_vhs_server
 *
 * Copyright (C) 2025 Jitterbug <jitterbug@posteo.co.uk>
 *
 * Code originally from the MISRC project
 * Copyright (C) 2017 Travis Mick (http://web.archive.org/web/20171026083549/https://lo.calho.st/quick-hacks/employing-black-magic-in-the-linux-page-table)
 * Copyright (C) 2024 vrunk11, stefan_o (https://github.com/Stefan-Olt/MISRC)
 */

#pragma once

#include "common.h"

typedef struct {
    uint8_t* buffer;
    size_t buffer_size;
    int fd;
    _Atomic(size_t) head;
    _Atomic(size_t) tail;
    _Atomic(size_t) total_write;
    _Atomic(size_t) total_read;
} ringbuffer_t;

#ifdef _WIN32
    // Ignore MEM_RELEASE + non-zero dwSize warnings
    //
    // "To split a placeholder into two placeholders, specify MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER."
    // https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualfree#parameters
    #define _VirtualFreeSplitPlaceholder(...) \
        _Pragma("warning (push)") _Pragma("warning (disable: 28160 6333)") VirtualFree(__VA_ARGS__) _Pragma("warning (pop)")

    #define _GetPtrOffset(ptr, off)   ((void*)((ULONG_PTR)(ptr) + off))
#endif

int rb_init(ringbuffer_t* rb, char* name, size_t size);
void* rb_read_ptr(ringbuffer_t* rb, size_t size);
int rb_read_finished(ringbuffer_t* rb, size_t size);
void* rb_write_ptr(ringbuffer_t* rb, size_t size);
int rb_write_finished(ringbuffer_t* rb, size_t size);
void rb_close(ringbuffer_t* rb);
