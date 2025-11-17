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

#ifndef _WIN32
    #include <sys/mman.h>
    #include <sys/types.h>
    #include <sys/syscall.h>
#endif

#include "ringbuffer.h"

int rb_init(ringbuffer_t* rb, char* name, size_t size) {
#ifdef _WIN32
    UNREFERENCED_PARAMETER(name);

    SYSTEM_INFO sysInfo;
    HANDLE h = NULL;
    void* maparea = NULL;

    // First, make sure the size is a multiple of the page size
    GetSystemInfo(&sysInfo);
    if ((size % sysInfo.dwAllocationGranularity) != 0) {
        return 1;
    }

    if ((maparea = (PCHAR)VirtualAlloc2(NULL, NULL, 2 * size, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, NULL, 0)) == NULL) {
        return 2;
    }

    if (_VirtualFreeSplitPlaceholder(maparea, size, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER) == FALSE) {
        return 3;
    }

    if ((h = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, (DWORD)size, NULL)) == NULL) {
        VirtualFree(_GetPtrOffset(maparea, size), 0, MEM_RELEASE);
        VirtualFree(maparea, 0, MEM_RELEASE);
        return 4;
    }

    if ((rb->buffer = MapViewOfFile3(h, NULL, maparea, 0, size, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0)) == NULL) {
        CloseHandle(h);
        VirtualFree(_GetPtrOffset(maparea, size), 0, MEM_RELEASE);
        VirtualFree(maparea, 0, MEM_RELEASE);
        return 5;
    }

    if ((MapViewOfFile3(h, NULL, _GetPtrOffset(maparea, size), 0, size, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0)) == NULL) {
        CloseHandle(h);
        VirtualFree(_GetPtrOffset(maparea, size), 0, MEM_RELEASE);
        UnmapViewOfFileEx(rb->buffer, 0);
        return 6;
    }
    CloseHandle(h);
#else
    // First, make sure the size is a multiple of the page size
    if (size % getpagesize() != 0) {
        return 1;
    }

    // Make an anonymous file and set its size
    if ((rb->fd = syscall(__NR_memfd_create, name, 0)) == -1) {
        return 2;
    }

    if (ftruncate(rb->fd, size) == -1) {
        return 3;
    }

    // Ask mmap for an address at a location where we can put both virtual copies of the buffer
    if ((rb->buffer = mmap(NULL, 2 * size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)) == MAP_FAILED) {
        return 4;
    }

    // Map the buffer at that address
    if (mmap(rb->buffer, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, rb->fd, 0) == MAP_FAILED) {
        return 5;
    }

    // Now map it again, in the next virtual page
    if (mmap(rb->buffer + size, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, rb->fd, 0) == MAP_FAILED) {
        return 6;
    }
#endif

    // Initialize our buffer indices
    rb->buffer_size = size;
    rb->head = 0;
    rb->tail = 0;
    rb->total_read = 0;
    rb->total_write = 0;
    return 0;
}

void* rb_write_ptr(ringbuffer_t* rb, size_t size) {
    if (rb->buffer_size - (rb->tail - rb->head) < size) {
        return NULL;
    }
    return &rb->buffer[rb->tail];
}

int rb_write_finished(ringbuffer_t* rb, size_t size) {
    if (rb->buffer_size - (rb->tail - rb->head) < size) {
        return 1;
    }
    rb->tail += size;
    rb->total_write += size;
    return 0;
}

void* rb_read_ptr(ringbuffer_t* rb, size_t size) {
    if (rb->tail - rb->head < size) {
        return NULL;
    }
    return &rb->buffer[rb->head];
}

int rb_read_finished(ringbuffer_t* rb, size_t size) {
    if (rb->tail - rb->head < size) {
        return 1;
    }
    rb->head += size;
    rb->total_read += size;
    if (rb->head > rb->buffer_size) {
        rb->head -= rb->buffer_size;
        rb->tail -= rb->buffer_size;
    }
    return 0;
}

void rb_close(ringbuffer_t* rb) {
#ifdef _WIN32
    UnmapViewOfFile(rb->buffer);
    UnmapViewOfFile(_GetPtrOffset(rb->buffer, rb->buffer_size));
#else
    munmap(rb->buffer, rb->buffer_size);
    munmap(rb->buffer + rb->buffer_size, rb->buffer_size);
#endif
}
