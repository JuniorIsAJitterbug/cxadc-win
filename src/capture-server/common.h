/*
 * capture-server - Windows port of cxadc_vhs_server
 *
 * Copyright (C) 2025 Jitterbug <jitterbug@posteo.co.uk>
 */

#pragma once

#ifdef _WIN32
    #define _CRT_SECURE_NO_WARNINGS

    #ifdef __STDC_NO_ATOMICS__
        #undef __STDC_NO_ATOMICS__
    #endif

    #ifdef __INTELLISENSE__
        #define _Atomic(t) t  // intellisense chokes on _Atomic t, this allows _Atomic(t) to be used without warnings
    #endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <threads.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#define _sleep_us(us)   thrd_sleep(&(struct timespec){.tv_nsec = (us) * 1000}, NULL)
#define _sleep_ms(ms)   thrd_sleep(&(struct timespec){.tv_nsec = (ms) * 1000 * 1000}, NULL)

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <winioctl.h>
    #include <io.h>
    #include <malloc.h>
    #include <cx_ctl_codes.h>

    #define CX_DEVICE_PATH          "\\\\.\\cxadc"
    #define FILE_OPEN_FLAGS         (_O_BINARY)

    #define _get_error()            win32_get_err_msg(GetLastError())

    #define _file_get_error()       (errno > 0 ? strerror(errno) : _get_error())
    #define _file_open              win32_file_open_non_blocking
    #define _file_read              _read
    #define _file_close             _close

    #define _stack_alloc(size)      _Pragma("warning (push)") _Pragma("warning (disable: 6255)") _alloca(size) _Pragma("warning (pop)")

    #define _thread_is_init(thr)    ((thr)._Handle != NULL)

    typedef SSIZE_T ssize_t;

    static char* win32_get_err_msg(DWORD msg_id) {
        static _Thread_local char msg_buf[1024];

        if (msg_id == ERROR_SUCCESS) {
            _strerror_s(msg_buf, sizeof(msg_buf), NULL);
            return msg_buf;
        }

        const DWORD ret = FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK,
            NULL,
            msg_id,
            0,
            (LPSTR)&msg_buf,
            sizeof(msg_buf),
            NULL
        );

        if (ret > 0) {
            msg_buf[ret - 1] = '\0'; // remove trailing space
        }

        return msg_buf;
    }

    static int win32_file_open_non_blocking(char* file_name, int flags) {
        int fd = _open(file_name, (flags | _O_RDWR));

        if (!DeviceIoControl((HANDLE)_get_osfhandle(fd), CX_IOCTL_IO_NON_BLOCKING_SET, NULL, 0, NULL, 0, NULL, NULL)) {
            return -1;
        }

        return fd;
    }
#else
    #include <alloca.h>
    #include <unistd.h>

    #define CX_DEVICE_PATH          "/dev/cxadc"
    #define FILE_OPEN_FLAGS         (O_NONBLOCK)

    #define _get_error()            strerror(errno)

    #define _file_get_error()       strerror(errno)
    #define _file_open              open
    #define _file_read              read
    #define _file_close             close

    #define _stack_alloc            alloca

    #define _thread_is_init(thr)    ((thr) != 0)
#endif
