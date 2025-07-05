/*
 * This file is from MISRC (https://github.com/Stefan-Olt/MISRC/)
 *
 * Minor changes made by:
 * Copyright (C) 2025 Jitterbug <jitterbug@posteo.co.uk>
 *
 *
 * Original header:
 * MISRC tools
 * Copyright (C) 2024  vrunk11, stefan_o
 *
 * based on:
 * http://web.archive.org/web/20171026083549/https://lo.calho.st/quick-hacks/employing-black-magic-in-the-linux-page-table/
 * Copyright (C) 2017 by Travis Mick
 *
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
