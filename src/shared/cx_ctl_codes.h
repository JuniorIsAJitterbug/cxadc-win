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

#define CTL_CODE_R(fn)  CTL_CODE(FILE_DEVICE_UNKNOWN, fn, METHOD_BUFFERED, FILE_READ_DATA)
#define CTL_CODE_W(fn)  CTL_CODE(FILE_DEVICE_UNKNOWN, fn, METHOD_BUFFERED, FILE_WRITE_DATA)

// state
#define CX_IOCTL_STATE_GET                      CTL_CODE_R(0x701)
#define CX_IOCTL_STATE_CAPTURE_STATE_GET        CTL_CODE_R(0x800)
#define CX_IOCTL_STATE_WIN32_PATH_GET           CTL_CODE_R(0x832)
#define CX_IOCTL_STATE_OUFLOW_GET               CTL_CODE_R(0x810)
#define CX_IOCTL_STATE_OUFLOW_RESET             CTL_CODE_W(0x910)
#define CX_CTRL_STATE_OUFLOW_RESET_WMI_ID       1

// hardware
#define CX_IOCTL_HW_BUS_NUMBER_GET              CTL_CODE_R(0x830)
#define CX_CTRL_HW_BUS_NUMBER_GET_WMI_ID        1

#define CX_IOCTL_HW_DEVICE_ADDRESS_GET          CTL_CODE_R(0x831)
#define CX_CTRL_HW_DEVICE_ADDRESS_GET_WMI_ID    2

// registers
#define CX_IOCTL_HW_REGISTER_GET                CTL_CODE_R(0x82F)
#define CX_CTRL_HW_REGISTER_GET_WMI_ID          1

#define CX_IOCTL_HW_REGISTER_SET                CTL_CODE_W(0x92F)
#define CX_CTRL_HW_REGISTER_SET_WMI_ID          2

// config
#define CX_IOCTL_CONFIG_GET                     CTL_CODE_R(0x700)

// vmux
#define CX_IOCTL_CONFIG_VMUX_GET                CTL_CODE_R(0x821)
#define CX_IOCTL_CONFIG_VMUX_SET                CTL_CODE_W(0x921)
#define CX_CTRL_CONFIG_VMUX_REG_KEY             L"vmux"
#define CX_CTRL_CONFIG_VMUX_WMI_ID              1
#define CX_CTRL_CONFIG_VMUX_DEFAULT             2
#define CX_CTRL_CONFIG_VMUX_MIN                 0
#define CX_CTRL_CONFIG_VMUX_MAX                 3

// level
#define CX_IOCTL_CONFIG_LEVEL_GET               CTL_CODE_R(0x822)
#define CX_IOCTL_CONFIG_LEVEL_SET               CTL_CODE_W(0x922)
#define CX_CTRL_CONFIG_LEVEL_REG_KEY            L"level"
#define CX_CTRL_CONFIG_LEVEL_WMI_ID             2
#define CX_CTRL_CONFIG_LEVEL_DEFAULT            16
#define CX_CTRL_CONFIG_LEVEL_MIN                0
#define CX_CTRL_CONFIG_LEVEL_MAX                31

// tenbit
#define CX_IOCTL_CONFIG_TENBIT_GET              CTL_CODE_R(0x823)
#define CX_IOCTL_CONFIG_TENBIT_SET              CTL_CODE_W(0x923)
#define CX_CTRL_CONFIG_TENBIT_REG_KEY           L"tenbit"
#define CX_CTRL_CONFIG_TENBIT_WMI_ID            3
#define CX_CTRL_CONFIG_TENBIT_DEFAULT           0
#define CX_CTRL_CONFIG_TENBIT_MIN               0
#define CX_CTRL_CONFIG_TENBIT_MAX               1

// sixdb
#define CX_IOCTL_CONFIG_SIXDB_GET               CTL_CODE_R(0x824)
#define CX_IOCTL_CONFIG_SIXDB_SET               CTL_CODE_W(0x924)
#define CX_CTRL_CONFIG_SIXDB_REG_KEY            L"sixdb"
#define CX_CTRL_CONFIG_SIXDB_WMI_ID             4
#define CX_CTRL_CONFIG_SIXDB_DEFAULT            0
#define CX_CTRL_CONFIG_SIXDB_MIN                0
#define CX_CTRL_CONFIG_SIXDB_MAX                1

// center offset
#define CX_IOCTL_CONFIG_CENTER_OFFSET_GET       CTL_CODE_R(0x825)
#define CX_IOCTL_CONFIG_CENTER_OFFSET_SET       CTL_CODE_W(0x925)
#define CX_CTRL_CONFIG_CENTER_OFFSET_REG_KEY    L"center_offset"
#define CX_CTRL_CONFIG_CENTER_OFFSET_WMI_ID     5
#define CX_CTRL_CONFIG_CENTER_OFFSET_DEFAULT    0
#define CX_CTRL_CONFIG_CENTER_OFFSET_MIN        0
#define CX_CTRL_CONFIG_CENTER_OFFSET_MAX        63

// memory
#define CX_IOCTL_HW_MMAP                        CTL_CODE_R(0xA00)
#define CX_IOCTL_HW_MUNMAP                      CTL_CODE_W(0xA01)
