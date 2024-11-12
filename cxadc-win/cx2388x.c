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

#include "precomp.h"
#include "cx2388x.tmh"

#include "cx2388x.h"

__inline ULONG
cx_read(
    _Inout_ PDEVICE_CONTEXT dev_ctx,
    _In_ ULONG off
)
{
    return READ_REGISTER_ULONG(&dev_ctx->mmio[off >> 2]);
}

__inline VOID
cx_write(
    _Inout_ PDEVICE_CONTEXT dev_ctx,
    _In_ ULONG off,
    _In_ ULONG val
)
{
    WRITE_REGISTER_ULONG(&dev_ctx->mmio[off >> 2], val);
}

NTSTATUS cx_init(
    _Inout_ PDEVICE_CONTEXT dev_ctx
)
{
    NTSTATUS status = STATUS_SUCCESS;

    cx_init_risc(dev_ctx);
    cx_init_cdt(dev_ctx);

    // clear interrupt
    cx_write(dev_ctx, CX_VID_INT_STAT, cx_read(dev_ctx, CX_VID_INT_STAT));

    // init sram
    cx_write(dev_ctx, CX_SRAM_CMDS_BASE, dev_ctx->dma_risc_instr.la.LowPart);
    cx_write(dev_ctx, CX_SRAM_CMDS_BASE + 4, CX_SRAM_CDT_BASE);
    cx_write(dev_ctx, CX_SRAM_CMDS_BASE + 8, CX_CDT_BUF_COUNT * 2);
    cx_write(dev_ctx, CX_SRAM_CMDS_BASE + 12, CX_SRAM_RISC_QUEUE_BASE);
    cx_write(dev_ctx, CX_SRAM_CMDS_BASE + 16, 0x40);

    dev_ctx->attrs.vmux &= 3;

    // the following values are from the Linux driver
    // I don't fully understand what each value is doing, nor if/why they are required
    cx_set_vmux(dev_ctx);
    cx_write(dev_ctx, CX_OUTPUT_FORMAT, 0xF);

    cx_write(dev_ctx, CX_CONT_BRIGHT_CNTRL, 0xFF00);
    cx_write(dev_ctx, CX_VBI_PACKET_CNTRL, (CX_CDT_BUF_LEN << 17) | (2 << 11));
    cx_write(dev_ctx, CX_COLOR_CNTRL, 0xE | (0xE << 4) | (0 << 8));

    cx_set_tenbit(dev_ctx);

    cx_write(dev_ctx, CX_AFE_CFG_IO, 0x12);
    cx_write(dev_ctx, CX_DEV_CNTRL2, 1 << 5);
    cx_write(dev_ctx, CX_VID_DMA_CNTRL, (1 << 7) | (1 << 3));

    cx_write(dev_ctx, CX_SR_CONV, 0x20000);
    cx_write(dev_ctx, CX_PLL, 0x11000000);
    cx_write(dev_ctx, CX_ADC_SYNC_SLICER, 0);
    cx_write(dev_ctx, CX_AGC_BACK_VBI, (0 << 27) | (0 << 26) | (1 << 25) | (0x100 << 16) | (0xFFF << 0));

    cx_set_level(dev_ctx);

    cx_write(dev_ctx, CX_AGC_SYNC_TIP1, (0x1C0 << 17) | (0 << 9) | (0 << 7) | (0xF << 0));
    cx_write(dev_ctx, CX_AGC_SYNC_TIP2, (0x20 << 17) | (0 << 9) | (0 << 7) | (0xF << 0));

    cx_set_center_offset(dev_ctx);

    cx_write(dev_ctx, CX_AGC_GAIN_ADJ1, (0xE0 << 17) | (0xE << 9) | (0 << 7) | (7 << 0));
    cx_write(dev_ctx, CX_AGC_GAIN_ADJ2, (0x20 << 17) | (2 << 7) | 0xF);
    cx_write(dev_ctx, CX_AGC_GAIN_ADJ3, (0x28 << 16) | (0x28 << 8) | (0x50 << 0));

    cx_write(dev_ctx, CX_I2C, 0x3);
    cx_write(dev_ctx, CX_VID_INT_MSK, CX_INTR_MASK);

    return status;
}

NTSTATUS cx_init_cdt(
    _Inout_ PDEVICE_CONTEXT dev_ctx
)
{
    NTSTATUS status = STATUS_SUCCESS;
    ULONG cdt_ptr = CX_SRAM_CDT_BASE;
    ULONG buf_ptr = CX_SRAM_CDT_BUF_BASE;
    ULONG i;

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "init cdt table (%d * %d)",
        CX_CDT_BUF_COUNT, CX_CDT_BUF_LEN);

    for (i = 0; i < CX_CDT_BUF_COUNT; i++)
    {
        cx_write(dev_ctx, cdt_ptr, buf_ptr);
        cdt_ptr += 4;
        cx_write(dev_ctx, cdt_ptr, 0);
        cdt_ptr += 4;
        cx_write(dev_ctx, cdt_ptr, 0);
        cdt_ptr += 4;
        cx_write(dev_ctx, cdt_ptr, 0);
        cdt_ptr += 4;

        buf_ptr += CX_CDT_BUF_LEN;
    }

    cx_write(dev_ctx, CX_DMA24_CNT1, CX_CDT_BUF_LEN / 8 - 1);
    cx_write(dev_ctx, CX_DMA24_PTR2, CX_SRAM_CDT_BASE);
    cx_write(dev_ctx, CX_DMA24_CNT2, CX_CDT_BUF_COUNT * 2);

    return status;
}

NTSTATUS cx_init_risc(
    _Inout_ PDEVICE_CONTEXT dev_ctx
)
{
    NTSTATUS status = STATUS_SUCCESS;
    PULONG dma_instr_ptr = (PULONG)dev_ctx->dma_risc_instr.va;
    ULONG dma_page_addr;
    ULONG page_idx, write_idx;

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "dma phys addr %08X", dev_ctx->dma_risc_instr.la.LowPart);

    // the following comments are from the Linux driver, as they explain the logic sufficiently

    // The RISC program is just a long sequence of WRITEs that fill each DMA page in
    // sequence. It begins with a SYNC and ends with a JUMP back to the first WRITE.

    *dma_instr_ptr++ = CX_RISC_SYNC | CX_RISC_CNT_RESET;

    for (page_idx = 0; page_idx < CX_VBI_BUF_COUNT; page_idx++)
    {
        dma_page_addr = dev_ctx->dma_risc_page[page_idx].la.LowPart;

        // Each WRITE is CX_CDT_BUF_LEN bytes so each DMA page requires
        //  n = (PAGE_SIZE / CX_CDT_BUF_LEN) WRITEs to fill it.

        // Generate n - 1 WRITEs.
        for (write_idx = 0; write_idx < (PAGE_SIZE / CX_CDT_BUF_LEN) - 1; write_idx++)
        {
            *dma_instr_ptr++ = CX_RISC_WRITE | CX_CDT_BUF_LEN | CX_RISC_SOL | CX_RISC_EOL | CX_RISC_CNT_NONE;
            *dma_instr_ptr++ = dma_page_addr;
            dma_page_addr += CX_CDT_BUF_LEN;
        }

        // Generate the final write which may trigger side effects.
        *dma_instr_ptr++ = CX_RISC_WRITE | CX_CDT_BUF_LEN | CX_RISC_SOL | CX_RISC_EOL
            // If this is the last DMA page, reset counter, otherwise increment it.
            | (page_idx == (CX_VBI_BUF_COUNT - 1) ? CX_RISC_CNT_RESET : CX_RISC_INC)
            // If we've filled enough pages, trigger IRQ1.
            | ((((page_idx + 1) % CX_IRQ_PERIOD_IN_PAGES) == 0) ? CX_RISC_IRQ1 : 0);

        *dma_instr_ptr++ = dma_page_addr;
    }

    // Jump back to first WRITE (+4 skips the SYNC command.)
    *dma_instr_ptr++ = CX_RISC_JUMP;
    *dma_instr_ptr++ = dev_ctx->dma_risc_instr.la.LowPart + 4;

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "filled risc instr dma, total size %lu kbyte",
        (ULONG)((PUCHAR)dma_instr_ptr - dev_ctx->dma_risc_instr.va) / 1024);

    return status;
}

NTSTATUS cx_disable(
    _Inout_ PDEVICE_CONTEXT dev_ctx
)
{
    NTSTATUS status = STATUS_SUCCESS;

    cx_write(dev_ctx, CX_PCI_INT_MSK, 0);
    cx_write(dev_ctx, CX_VID_INT_MSK, 0);
    cx_write(dev_ctx, CX_VID_INT_STAT, ~(ULONG)0);
    cx_write(dev_ctx, CX_VID_DMA_CNTRL, 0);
    cx_write(dev_ctx, CX_DEV_CNTRL2, 0);

    return status;
}

BOOLEAN cx_evt_isr(
    _In_ WDFINTERRUPT intr,
    _In_ ULONG msg_id
)
{
    UNREFERENCED_PARAMETER(intr);
    UNREFERENCED_PARAMETER(msg_id);

    ULONG allstat, stat, astat;
    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(WdfInterruptGetDevice(intr));

    allstat = cx_read(dev_ctx, CX_VID_INT_STAT);
    stat = cx_read(dev_ctx, CX_VID_INT_MSK);
    astat = stat & allstat;

    if (astat != 8 && allstat != 0 && astat != 0)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "intr stat 0x%0X masked 0x%0X", allstat, astat);
    }

    if (!astat) {
        return FALSE;
    }

    if (astat & 0x08)
    {
        // comment from the Linux driver
        // NB: CX_VBI_GP_CNT is not guaranteed to be in-sync with resident pages.
        // i.e. we can get gp_cnt == 1 but the first page may not yet have been transferred
        // to main memory. on the other hand, if an interrupt has occurred, we are guaranteed to have the page
        // in main memory. so we only retrieve CX_VBI_GP_CNT after an interrupt has occurred and then round
        // it down to the last page that we know should have triggered an interrupt.
        ULONG gp_cnt = cx_read(dev_ctx, CX_VBI_GP_CNT);
        gp_cnt &= ~(CX_IRQ_PERIOD_IN_PAGES - 1);
        dev_ctx->gp_cnt = gp_cnt;
        KeSetEvent(&dev_ctx->isr_event, IO_NO_INCREMENT, FALSE);
    }

    cx_write(dev_ctx, CX_VID_INT_STAT, astat);

    return TRUE;
}

VOID cx_start_capture(
    _Inout_ PDEVICE_CONTEXT dev_ctx
)
{
    cx_write(dev_ctx, CX_PCI_INT_MSK, 1);
}

VOID cx_stop_capture(
    _Inout_ PDEVICE_CONTEXT dev_ctx
)
{
    cx_write(dev_ctx, CX_PCI_INT_MSK, 0);
}

VOID cx_set_vmux(
    _Inout_ PDEVICE_CONTEXT dev_ctx
)
{
    cx_write(dev_ctx, CX_INPUT_FORMAT, (dev_ctx->attrs.vmux << 14) | (1 << 13) | 1 | 0x10 | 0x10000);
}

VOID cx_set_level(
    _Inout_ PDEVICE_CONTEXT dev_ctx
)
{
    cx_write(dev_ctx, CX_AGC_GAIN_ADJ4, 
       (dev_ctx->attrs.sixdb << 23) | (0 << 22) | (0 << 21) | (dev_ctx->attrs.level << 16) | (0xFF << 8) | (0 << 0));
}

VOID cx_set_tenbit(
    _Inout_ PDEVICE_CONTEXT dev_ctx
)
{
    if (dev_ctx->attrs.tenbit)
    {
        cx_write(dev_ctx, CX_CAPTURE_CNTRL, (1 << 6) | (3 << 1) | (1 << 5));
    }
    else
    {
        cx_write(dev_ctx, CX_CAPTURE_CNTRL, (1 << 6) | (3 << 1) | (0 << 5));
    }
}

VOID cx_set_center_offset(
    _Inout_ PDEVICE_CONTEXT dev_ctx
)
{
    cx_write(dev_ctx, CX_AGC_SYNC_TIP3, (0x1E48 << 16) | (0xFF << 8) | (dev_ctx->attrs.center_offset));
}
