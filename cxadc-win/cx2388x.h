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

#include "common.h"

__inline ULONG cx_read(_Inout_ PDEVICE_CONTEXT dev_ctx, _In_ ULONG off);
__inline VOID cx_write(_Inout_ PDEVICE_CONTEXT dev_ctx, _In_ ULONG off, _In_ ULONG val);

NTSTATUS cx_init(_Inout_ PDEVICE_CONTEXT dev_ctx);
NTSTATUS cx_disable(_Inout_ PDEVICE_CONTEXT dev_ctx);
NTSTATUS cx_init_cdt(_Inout_ PDEVICE_CONTEXT dev_ctx);
NTSTATUS cx_init_risc(_Inout_ PDEVICE_CONTEXT dev_ctx);

VOID cx_start_capture(_Inout_ PDEVICE_CONTEXT dev_ctx);
VOID cx_stop_capture(_Inout_ PDEVICE_CONTEXT dev_ctx);
VOID cx_set_vmux(_Inout_ PDEVICE_CONTEXT dev_ctx);
VOID cx_set_level(_Inout_ PDEVICE_CONTEXT dev_ctx);
VOID cx_set_tenbit(_Inout_ PDEVICE_CONTEXT dev_ctx);
VOID cx_set_center_offset(_Inout_ PDEVICE_CONTEXT dev_ctx);

#define CX_IRQ_PERIOD_IN_PAGES  (0x200000 >> PAGE_SHIFT)
#define CX_INTR_MASK            0x018888
#define CX_PCI_INT_MSK          0x200040

#define CX_MEM_SRAM_BASE        0x180000
#define CX_SRAM_CMDS_BASE       CX_MEM_SRAM_BASE + 0x0100
#define CX_SRAM_RISC_QUEUE_BASE CX_MEM_SRAM_BASE + 0x0800
#define CX_SRAM_CDT_BASE        CX_MEM_SRAM_BASE + 0x1000
#define CX_SRAM_CDT_BUF_BASE    CX_MEM_SRAM_BASE + 0x4000

#define CX_DEV_CNTRL2           0x200034
#define CX_DMA24_PTR2           0x3000CC
#define CX_DMA24_CNT1           0x30010C
#define CX_DMA24_CNT2           0x30014C

#define CX_VID_INT_MSK          0x200050
#define CX_VID_INT_STAT         0x200054

#define CX_RISC_CNT_NONE        0x00000000
#define CX_RISC_INC             0x00010000
#define CX_RISC_CNT_RESET       0x00030000
#define CX_RISC_IRQ1            0x01000000
#define CX_RISC_EOL             0x04000000
#define CX_RISC_SOL             0x08000000
#define CX_RISC_WRITE           0x10000000
#define CX_RISC_JUMP            0x70000000
#define CX_RISC_SYNC            0x80000000

#define CX_AGC_BACK_VBI         0x310200
#define CX_ADC_SYNC_SLICER      0x310204
#define CX_AGC_SYNC_TIP1        0x310208
#define CX_AGC_SYNC_TIP2        0x31020c
#define CX_AGC_SYNC_TIP3        0x310210
#define CX_AGC_GAIN_ADJ1        0x310214
#define CX_AGC_GAIN_ADJ2        0x310218
#define CX_AGC_GAIN_ADJ3        0x31021c
#define CX_AGC_GAIN_ADJ4        0x310220

#define CX_OUTPUT_FORMAT        0x301064
#define CX_INPUT_FORMAT         0x310104
#define CX_CONT_BRIGHT_CNTRL    0x310110
#define CX_CAPTURE_CNTRL        0x310180
#define CX_COLOR_CNTRL          0x310184
#define CX_VBI_PACKET_CNTRL     0x310188
#define CX_VID_DMA_CNTRL        0x31C040

#define CX_PLL                  0x310168
#define CX_SR_CONV              0x310170
#define CX_VBI_GP_CNT           0x31C02C
#define CX_AFE_CFG_IO           0x35C04C
#define CX_I2C                  0x368000
