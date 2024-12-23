// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * cxadc-win - CX2388x ADC DMA driver for Windows
 *
 * Copyright (C) 2024 Jitterbug
 *
 * Based on the Linux version created by
 * Copyright (C) 2005-2007 Hew How Chee <how_chee@yahoo.com>
 * Copyright (C) 2013-2015 Chad Page <Chad.Page@gmail.com>
 * Copyright (C) 2019-2023 Adam Sampson <ats@offog.org>
 * Copyright (C) 2020-2022 Tony Anderson <tandersn@cs.washington.edu>
 */

#pragma once

DEFINE_GUID(GUID_DEVINTERFACE_CXADCWIN,
    0x13EF40B0, 0x05FF, 0x4173, 0xB6, 0x13, 0x31, 0x41, 0xAD, 0x2E, 0x37, 0x62);

#define CX_IOCTL_GET_CAPTURE_STATE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_READ_DATA)

#define CX_IOCTL_GET_OUFLOW_COUNT \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x810, METHOD_BUFFERED, FILE_READ_DATA)

#define CX_IOCTL_GET_CRYSTAL \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x820, METHOD_BUFFERED, FILE_READ_DATA)

#define CX_IOCTL_GET_VMUX \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x821, METHOD_BUFFERED, FILE_READ_DATA)

#define CX_IOCTL_GET_LEVEL \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x822, METHOD_BUFFERED, FILE_READ_DATA)

#define CX_IOCTL_GET_TENBIT \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x823, METHOD_BUFFERED, FILE_READ_DATA)

#define CX_IOCTL_GET_SIXDB \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x824, METHOD_BUFFERED, FILE_READ_DATA)

#define CX_IOCTL_GET_CENTER_OFFSET \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x825, METHOD_BUFFERED, FILE_READ_DATA)

#define CX_IOCTL_GET_BUS_NUMBER \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x830, METHOD_BUFFERED, FILE_READ_DATA)

#define CX_IOCTL_GET_DEVICE_ADDRESS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x831, METHOD_BUFFERED, FILE_READ_DATA)

#define CX_IOCTL_GET_REGISTER \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x82F, METHOD_BUFFERED, FILE_READ_DATA)

#define CX_IOCTL_RESET_OUFLOW_COUNT \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x910, METHOD_BUFFERED, FILE_WRITE_DATA)

#define CX_IOCTL_SET_CRYSTAL \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x920, METHOD_BUFFERED, FILE_WRITE_DATA)

#define CX_IOCTL_SET_VMUX \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x921, METHOD_BUFFERED, FILE_WRITE_DATA)

#define CX_IOCTL_SET_LEVEL \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x922, METHOD_BUFFERED, FILE_WRITE_DATA)

#define CX_IOCTL_SET_TENBIT \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x923, METHOD_BUFFERED, FILE_WRITE_DATA)

#define CX_IOCTL_SET_SIXDB \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x924, METHOD_BUFFERED, FILE_WRITE_DATA)

#define CX_IOCTL_SET_CENTER_OFFSET \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x925, METHOD_BUFFERED, FILE_WRITE_DATA)

#define CX_IOCTL_SET_REGISTER \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x92F, METHOD_BUFFERED, FILE_WRITE_DATA)

#define CX_IOCTL_MMAP \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0xA00, METHOD_BUFFERED, FILE_READ_DATA)

#define CX_IOCTL_MUNMAP \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0xA01, METHOD_BUFFERED, FILE_WRITE_DATA)

// vmux 0-3
#define CX_IOCTL_VMUX_DEFAULT           2
#define CX_IOCTL_VMUX_MIN               0
#define CX_IOCTL_VMUX_MAX               3

// level 0-31
#define CX_IOCTL_LEVEL_DEFAULT          16
#define CX_IOCTL_LEVEL_MIN              0
#define CX_IOCTL_LEVEL_MAX              31

// tenbit 0-1
#define CX_IOCTL_TENBIT_DEFAULT         0
#define CX_IOCTL_TENBIT_MIN             0
#define CX_IOCTL_TENBIT_MAX             1

// sixdb 0-1
#define CX_IOCTL_SIXDB_DEFAULT          0
#define CX_IOCTL_SIXDB_MIN              0
#define CX_IOCTL_SIXDB_MAX              1

// center_offset 0-63
#define CX_IOCTL_CENTER_OFFSET_DEFAULT  0
#define CX_IOCTL_CENTER_OFFSET_MIN      0
#define CX_IOCTL_CENTER_OFFSET_MAX      63
