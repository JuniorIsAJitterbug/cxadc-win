// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * leveladj - Windows port of the leveladj tool
 *
 * Copyright (C) 2024-2025 Jitterbug <jitterbug@posteo.co.uk>
 *
 * Based on the Linux version created by
 * Copyright (C) 2005-2007 Hew How Chee <how_chee@yahoo.com>
 * Copyright (C) 2013-2015 Chad Page <Chad.Page@gmail.com>
 * Copyright (C) 2019-2023 Adam Sampson <ats@offog.org>
 * Copyright (C) 2020-2022 Tony Anderson <tandersn@cs.washington.edu>
 */

#include <windows.h>
#include <stdio.h>
#include <cx_ctl_codes.h>
#include <cx_hw.h>

// this needs to be one over the ring buffer size to work
#define BUF_SIZE (CX_VBI_BUF_SIZE + (1024 * 1024 * 1))

unsigned char buf[BUF_SIZE];
const int readlen = 2048 * 1024;

LPCSTR win32_get_err_msg(DWORD msg_id);

int main(int argc, char* argv[])
{
    int ret = EXIT_FAILURE;
    HANDLE dev_handle = INVALID_HANDLE_VALUE;
    LPSTR device_path = NULL;

    ULONG level = 20;
    BOOLEAN tenbit = FALSE;

    int go_on = 1; // 2 after going over

    if (argc < 2) {
        fprintf(stderr, "Usage: %s device_path [starting_level]", argv[0]);
        return ret;
    }

    if (argc >= 2) {
        device_path = argv[1];
    }

    if (argc >= 3) {
        level = (ULONG)atoi(argv[2]);
    }

    if ((dev_handle = CreateFileA(
        device_path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL)) == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Error opening device %s: %s\n", device_path, win32_get_err_msg(GetLastError()));
        goto exit;
    }

    // get tenbit state
    if (!DeviceIoControl(dev_handle, CX_IOCTL_CONFIG_TENBIT_GET, NULL, 0, &tenbit, sizeof(tenbit), NULL, NULL)) {
        fprintf(stderr, "Error sending IOCTL (get tenbit): %s\n", win32_get_err_msg(GetLastError()));
        goto exit;
    }

    while (go_on) {
        int over = 0;
        unsigned int low = tenbit ? 65535 : 255, high = 0;

        if (!DeviceIoControl(dev_handle, CX_IOCTL_CONFIG_LEVEL_SET, (PULONG)&level, sizeof(level), NULL, 0, NULL, NULL)) {
            fprintf(stderr, "Error sending IOCTL (set level): %s\n", win32_get_err_msg(GetLastError()));
            goto exit;
        }

        printf("testing level %d\n", level);

        // dump cache
        if (!ReadFile(dev_handle, buf, BUF_SIZE, NULL, NULL)) {
            fprintf(stderr, "Error reading device: %s\n", win32_get_err_msg(GetLastError()));
            goto exit;
        }

        // read a bit
        if (!ReadFile(dev_handle, buf, readlen, NULL, NULL)) {
            fprintf(stderr, "Error reading device: %s\n", win32_get_err_msg(GetLastError()));
            goto exit;
        }

        if (tenbit) {
            unsigned short* wbuf = (void*)buf;

            for (int i = 0; i < (readlen / 2) && (over < (readlen / 200000)); i++) {
                if (wbuf[i] < low)
                    low = wbuf[i];
                if (wbuf[i] > high)
                    high = wbuf[i];

                if ((wbuf[i] < 0x0800) || (wbuf[i] > 0xf800))
                    over++;

                // auto fail on 0 and 65535
                if ((wbuf[i] == 0) || (wbuf[i] == 0xffff))
                    over += (readlen / 50000);
            }
        }
        else {
            for (int i = 0; i < readlen && (over < (readlen / 100000)); i++) {
                if (buf[i] < low)
                    low = buf[i];
                if (buf[i] > high)
                    high = buf[i];

                if ((buf[i] < 0x08) || (buf[i] > 0xf8))
                    over++;

                // auto fail on 0 and 255
                if ((buf[i] == 0) || (buf[i] == 0xff))
                    over += (readlen / 50000);
            }
        }

        printf("low %d high %d clipped %d nsamp %d\n", (int)low, (int)high, over, readlen);

        if (over >= 20) {
            go_on = 2;
        }
        else {
            if (go_on == 2)
                go_on = 0;
        }

        if (go_on == 1)
            level++;
        else if (go_on == 2)
            level--;

        if ((level < 0) || (level > 31))
            go_on = 0;
    }

    ret = EXIT_SUCCESS;

exit:
    if (dev_handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(dev_handle);
    }

    return ret;
}

LPCSTR win32_get_err_msg(DWORD msg_id) {
    static char msg_buf[1024];

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
