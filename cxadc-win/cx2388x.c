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

#include "pch.h"
#include "cx2388x.tmh"
#include "cx2388x.h"

_Use_decl_annotations_
NTSTATUS cx_init(
    PDEVICE_CONTEXT dev_ctx
)
{
    NTSTATUS status = STATUS_SUCCESS;

    // the following values & comments are from the Linux driver
    // I don't fully understand what each value is doing, nor if/why they are required

    cx_init_cdt(&dev_ctx->mmio);
    cx_init_risc(&dev_ctx->risc);
    cx_init_cmds(&dev_ctx->mmio, &dev_ctx->risc);

    // clear interrupt
    cx_write(&dev_ctx->mmio, CX_DMAC_VIDEO_INTERRUPT_STATUS_ADDR, cx_read(&dev_ctx->mmio, CX_DMAC_VIDEO_INTERRUPT_STATUS_ADDR));

    // allow full range
    cx_write(&dev_ctx->mmio, CX_VIDEO_OUTPUT_CONTROL_ADDR,
        (CX_VIDEO_OUTPUT_CONTROL){
            .hsfmt = 1,
            .hactext = 1,
            .range = 1
        }.dword);

    cx_write(&dev_ctx->mmio, CX_VIDEO_CONTRAST_BRIGHTNESS_ADDR,
        (CX_VIDEO_CONTRAST_BRIGHTNESS){
            .cntrst = 0xFF
        }.dword);

    // no of byte transferred from peripheral to fifo
    // if fifo buffer < this, it will still transfer this no of byte
    // must be multiple of 8, if not go haywire?
    cx_write(&dev_ctx->mmio, CX_VIDEO_VBI_PACKET_SIZE_DELAY_ADDR,
        (CX_VIDEO_VBI_PACKET_SIZE_DELAY){
            .vbi_v_del = 2,
            .frm_size = CX_CDT_BUF_LEN
        }.dword);

    // raw mode & byte swap << 8 (3 << 8 = swap)
    cx_write(&dev_ctx->mmio, CX_VIDEO_COLOR_FORMAT_CONTROL_ADDR,
        (CX_VIDEO_COLOR_FORMAT_CONTROL){
            .color_even = 0xE,
            .color_odd = 0xE,
        }.dword);

    // power down audio and chroma DAC+ADC
    cx_write(&dev_ctx->mmio, CX_MISC_AFECFG_ADDR,
        (CX_MISC_AFECFG){
            .bg_pwrdn = 1,
            .dac_pwrdn = 1
        }.dword);

    // set SRC to 8xfsc
    cx_write(&dev_ctx->mmio, CX_VIDEO_SAMPLE_RATE_CONVERSION_ADDR,
        (CX_VIDEO_SAMPLE_RATE_CONVERSION){
            .src_reg_val = 0x20000
        }.dword);

    // set PLL to 1:1
    cx_write(&dev_ctx->mmio, CX_VIDEO_PLL_ADDR,
        (CX_VIDEO_PLL){
            .pll_int = 0x10,
            .pll_dds = 1
        }.dword);

    // set vbi agc
    // set back porch sample delay & sync sample delay to max
    cx_write(&dev_ctx->mmio, CX_VIDEO_AGC_SYNC_SLICER_ADDR,
        (CX_VIDEO_AGC_SYNC_SLICER){
            .sync_sam_dly = 0xFF,
            .bp_sam_dly = 0xFF
        }.dword);

    cx_write(&dev_ctx->mmio, CX_VIDEO_AGC_CONTROL_ADDR,
        (CX_VIDEO_AGC_CONTROL){
            .intrvl_cnt_val = 0xFFF,
            .bp_ref = 0x100,
            .bp_ref_sel = 1,
            .agc_vbi_en = 0,
            .clamp_vbi_en = 0
        }.dword);

    cx_write(&dev_ctx->mmio, CX_VIDEO_AGC_SYNC_TIP_ADJUST_1_ADDR,
        (CX_VIDEO_AGC_SYNC_TIP_ADJUST_1){
            .trk_sat_val = 0x0F,
            .trk_mode_thr = 0x1C0
        }.dword);

    cx_write(&dev_ctx->mmio, CX_VIDEO_AGC_SYNC_TIP_ADJUST_2_ADDR,
        (CX_VIDEO_AGC_SYNC_TIP_ADJUST_2){
            .acq_sat_val = 0xF,
            .acq_mode_thr = 0x20
        }.dword);

    cx_write(&dev_ctx->mmio, CX_VIDEO_AGC_GAIN_ADJUST_1_ADDR,
        (CX_VIDEO_AGC_GAIN_ADJUST_1){
            .trk_agc_sat_val = 7,
            .trk_agc_core_th_val = 0xE,
            .trk_agc_mode_th = 0xE0
        }.dword);

    cx_write(&dev_ctx->mmio, CX_VIDEO_AGC_GAIN_ADJUST_2_ADDR,
        (CX_VIDEO_AGC_GAIN_ADJUST_2){
            .acq_agc_sat_val = 0xF,
            .acq_gain_val = 2,
            .acq_agc_mode_th = 0x20
        }.dword);

    // set gain of agc but not offset
    cx_write(&dev_ctx->mmio, CX_VIDEO_AGC_GAIN_ADJUST_3_ADDR,
        (CX_VIDEO_AGC_GAIN_ADJUST_3){
            .acc_inc_val = 0x50,
            .acc_max_val = 0x28,
            .acc_min_val = 0x28
        }.dword);

    // disable PLL adjust (stabilizes output when video is detected by chip)
    CX_VIDEO_PLL_ADJUST pll_adjust =
    {
        .dword = cx_read(&dev_ctx->mmio, CX_VIDEO_PLL_ADJUST_ADDR)
    };

    pll_adjust.pll_adj_en = 0;
    cx_write(&dev_ctx->mmio, CX_VIDEO_PLL_ADJUST_ADDR, pll_adjust.dword);

    // i2c sda/scl set to high and use software control
    cx_write(&dev_ctx->mmio, CX_I2C_DATA_CONTROL_ADDR,
        (CX_I2C_DATA_CONTROL){
            .sda = 1,
            .scl = 1
        }.dword);

    cx_set_vmux(&dev_ctx->mmio, dev_ctx->config.vmux);
    cx_set_tenbit(&dev_ctx->mmio, dev_ctx->config.tenbit);
    cx_set_level(&dev_ctx->mmio, dev_ctx->config.level, dev_ctx->config.sixdb);
    cx_set_center_offset(&dev_ctx->mmio, dev_ctx->config.center_offset);

    return status;
}

_Use_decl_annotations_
VOID cx_init_cdt(
    PMMIO mmio
)
{
    ULONG cdt_ptr = CX_SRAM_CDT_BASE;
    ULONG buf_ptr = CX_SRAM_CDT_BUF_BASE;

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "init cdt table (%d * %d)",
        CX_CDT_BUF_COUNT, CX_CDT_BUF_LEN);

    // set cluster buffer location
    for (ULONG i = 0; i < CX_CDT_BUF_COUNT; i++)
    {
        cx_write_buf(mmio, cdt_ptr,
            (CX_CDT_DESCRIPTOR){
                .buffer_ptr = buf_ptr
            }.data,
            sizeof(CX_CDT_DESCRIPTOR));

        cdt_ptr += sizeof(CX_CDT_DESCRIPTOR);
        buf_ptr += CX_CDT_BUF_LEN;
    }

    // size of one buffer - 1
    cx_write(mmio, CX_DMAC_VBI_CNT1_ADDR,
        (CX_DMAC_DMA_CNT1){
            .dma_cnt1 = CX_CDT_BUF_LEN / 8 - 1
        }.dword);

    // ptr to cdt
    cx_write(mmio, CX_DMAC_VBI_PTR2_ADDR,
        (CX_DMAC_DMA_PTR2){
            .dma_ptr2 = CX_SRAM_CDT_BASE >> 2
        }.dword);

    // size of cdt
    cx_write(mmio, CX_DMAC_VBI_CNT2_ADDR,
        (CX_DMAC_DMA_CNT2){
            .dma_cnt2 = CX_CDT_BUF_COUNT * 2
        }.dword);
}

_Use_decl_annotations_
VOID cx_init_risc(
    PRISC risc
)
{
    PCX_RISC_INSTRUCTIONS dma_instr_ptr = (PCX_RISC_INSTRUCTIONS)risc->instructions.va;

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "dma phys addr %08X", risc->instructions.la.LowPart);

    // the following comments are from the Linux driver, as they explain the logic sufficiently

    // The RISC program is just a long sequence of WRITEs that fill each DMA page in
    // sequence. It begins with a SYNC and ends with a JUMP back to the first WRITE.

    dma_instr_ptr->sync_instr = (CX_RISC_INSTR_SYNC)
    {
        .opcode = CX_RISC_INSTR_SYNC_OPCODE,
        .cnt_ctl = 3,
    };

    for (ULONG page_idx = 0; page_idx < CX_VBI_BUF_COUNT; page_idx++)
    {
        ULONG dma_page_addr = risc->page[page_idx].la.LowPart;

        // Each WRITE is CX_CDT_BUF_LEN bytes so each DMA page requires
        //  n = (PAGE_SIZE / CX_CDT_BUF_LEN) WRITEs to fill it.

        // Generate n WRITEs.
        for (ULONG write_idx = 0; write_idx < (PAGE_SIZE / CX_CDT_BUF_LEN); write_idx++)
        {
            PCX_RISC_INSTR_WRITE write_instr = &dma_instr_ptr->write_instr[(page_idx * 2) + write_idx];

            *write_instr = (CX_RISC_INSTR_WRITE)
            {
                .opcode = CX_RISC_INSTR_WRITE_OPCODE,
                .sol = 1,
                .eol = 1,
                .byte_count = CX_CDT_BUF_LEN,
                .pci_target_address = dma_page_addr
            };

            dma_page_addr += CX_CDT_BUF_LEN;

            if (write_idx == (PAGE_SIZE / CX_CDT_BUF_LEN) - 1)
            {
                // always increment final write 
                write_instr->cnt_ctl = 1;

                //  reset counter on last page
                if (page_idx == (CX_VBI_BUF_COUNT - 1))
                {
                    write_instr->cnt_ctl = 3;
                }

                // trigger IRQ1
                if (((page_idx + 1) % CX_IRQ_PERIOD_IN_PAGES) == 0)
                {
                    write_instr->irq1 = 1;
                }
            }
        }
    }

    // Jump back to first WRITE (+4 skips the SYNC command.)
    dma_instr_ptr->jump_instr = (CX_RISC_INSTR_JUMP)
    {
        .opcode = CX_RISC_INSTR_JUMP_OPCODE,
        .jump_address = risc->instructions.la.LowPart + 4
    };

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "filled risc instr dma, total size %lu kbyte",
        sizeof(CX_RISC_INSTRUCTIONS) / 1024);

}

_Use_decl_annotations_
VOID cx_init_cmds(
    PMMIO mmio,
    PRISC risc
)
{
    // init sram
    cx_write_buf(mmio, CX_SRAM_CMDS_VBI_BASE,
        (CX_CMDS){
            .initial_risc_addr = risc->instructions.la.LowPart,
            .cdt_base = CX_SRAM_CDT_BASE,
            .cdt_size = CX_CDT_BUF_COUNT * 2,
            .risc_base = CX_SRAM_RISC_QUEUE_BASE,
            .risc_size = 0x40
        }.data,
        sizeof(CX_CMDS));
}

_Use_decl_annotations_
NTSTATUS cx_disable(
    PMMIO mmio
)
{
    NTSTATUS status = STATUS_SUCCESS;

    // setting all bits to 0/1 so just write entire dword

    // turn off interrupt
    cx_write(mmio, CX_DMAC_VIDEO_INTERRUPT_MASK_ADDR, 0);
    cx_write(mmio, CX_DMAC_VIDEO_INTERRUPT_STATUS_ADDR, 0xFFFFFFFF);

    // disable fifo and risc
    cx_write(mmio, CX_VIDEO_IPB_DMA_CONTROL_ADDR, 0);

    // disable risc
    cx_write(mmio, CX_DMAC_DEVICE_CONTROL_2_ADDR, 0);

    return status;
}

_Use_decl_annotations_
VOID cx_reset(
    PMMIO mmio
)
{
    // set agc registers back to default values
    cx_write(mmio, CX_VIDEO_AGC_CONTROL_ADDR,
        (CX_VIDEO_AGC_CONTROL){
            .intrvl_cnt_val = 0x555,
            .bp_ref = 0xE0
        }.dword);

    cx_write(mmio, CX_VIDEO_AGC_SYNC_SLICER_ADDR,
        (CX_VIDEO_AGC_SYNC_SLICER){
            .sync_sam_dly = 0x1C,
            .bp_sam_dly = 0x60,
            .mm_multi = 4,
            .std_slice_en = 1,
            .sam_slice_en = 1,
            .dly_upd_en = 1
        }.dword);

    cx_write(mmio, CX_VIDEO_AGC_SYNC_TIP_ADJUST_1_ADDR,
        (CX_VIDEO_AGC_SYNC_TIP_ADJUST_1){
            .trk_sat_val = 0xF,
            .trk_mode_thr = 0x1C0
        }.dword);

    cx_write(mmio, CX_VIDEO_AGC_SYNC_TIP_ADJUST_2_ADDR,
        (CX_VIDEO_AGC_SYNC_TIP_ADJUST_2){
            .acq_sat_val = 0x3F,
            .acq_g_val = 1,
            .acq_mode_thr = 0x20
        }.dword);

    cx_write(mmio, CX_VIDEO_AGC_SYNC_TIP_ADJUST_3_ADDR,
        (CX_VIDEO_AGC_SYNC_TIP_ADJUST_3){
            .acc_max = 0x40,
            .acc_min = 0xE0,
            .low_stip_th = 0x1E48
        }.dword);

    cx_write(mmio, CX_VIDEO_AGC_GAIN_ADJUST_1_ADDR,
        (CX_VIDEO_AGC_GAIN_ADJUST_1){
            .trk_agc_sat_val = 7,
            .trk_agc_core_th_val = 0xE,
            .trk_agc_mode_th = 0xE0
        }.dword);

    cx_write(mmio, CX_VIDEO_AGC_GAIN_ADJUST_2_ADDR,
        (CX_VIDEO_AGC_GAIN_ADJUST_2){
            .acq_agc_sat_val = 0xF,
            .acq_gain_val = 2,
            .acq_agc_mode_th = 0x20
        }.dword);

    cx_write(mmio, CX_VIDEO_AGC_GAIN_ADJUST_3_ADDR,
        (CX_VIDEO_AGC_GAIN_ADJUST_3){
            .acc_inc_val = 0xC0,
            .acc_max_val = 0x38,
            .acc_min_val = 0x28
        }.dword);

    cx_write(mmio, CX_VIDEO_AGC_GAIN_ADJUST_4_ADDR,
        (CX_VIDEO_AGC_GAIN_ADJUST_4){
            .high_acc_val = 0x34,
            .low_acc_val = 0x2C,
            .init_vga_val = 0xA,
            .vga_en = 1,
            .slice_ref_en = 1
        }.dword);
}

_Use_decl_annotations_
BOOLEAN cx_evt_isr(
    WDFINTERRUPT intr,
    ULONG msg_id
)
{
    UNREFERENCED_PARAMETER(msg_id);

    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(WdfInterruptGetDevice(intr));
    BOOLEAN is_recognized = FALSE;

    CX_DMAC_VIDEO_INTERRUPT mstat =
    {
        .dword = cx_read(&dev_ctx->mmio, CX_DMAC_VIDEO_INTERRUPT_MSTATUS_ADDR)
    };

    if (!mstat.vbi_risci1 && mstat.dword)
    {
        // unexpected interrupts?
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "intr stat 0x%0X masked 0x%0X",
            cx_read(&dev_ctx->mmio, CX_DMAC_VIDEO_INTERRUPT_STATUS_ADDR),
            mstat.dword);
    }

    if (mstat.vbi_risci1)
    {
        is_recognized = TRUE;
    }

    // clear interrupts
    cx_write(&dev_ctx->mmio, CX_DMAC_VIDEO_INTERRUPT_STATUS_ADDR, mstat.dword);

    if (is_recognized)
    {
        WdfInterruptQueueDpcForIsr(intr);
    }

    return is_recognized;
}

_Use_decl_annotations_
VOID cx_evt_dpc(
    WDFINTERRUPT intr,
    WDFOBJECT dev
)
{
    UNREFERENCED_PARAMETER(dev);

    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(WdfInterruptGetDevice(intr));

    // comment from the Linux driver
    // NB: CX_VBI_GP_CNT is not guaranteed to be in-sync with resident pages.
    // i.e. we can get gp_cnt == 1 but the first page may not yet have been transferred
    // to main memory. on the other hand, if an interrupt has occurred, we are guaranteed to have the page
    // in main memory. so we only retrieve CX_VBI_GP_CNT after an interrupt has occurred and then round
    // it down to the last page that we know should have triggered an interrupt.
    InterlockedExchange((PLONG)&dev_ctx->state.last_gp_cnt,
        cx_read(&dev_ctx->mmio, CX_VIDEO_VBI_GP_COUNTER_ADDR) & ~(CX_IRQ_PERIOD_IN_PAGES - 1));

    KeSetEvent(&dev_ctx->isr_event, IO_NO_INCREMENT, FALSE);
}

_Use_decl_annotations_
NTSTATUS cx_evt_intr_enable(
    WDFINTERRUPT intr,
    WDFDEVICE    dev
)
{
    UNREFERENCED_PARAMETER(dev);

    NTSTATUS status = STATUS_SUCCESS;
    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(WdfInterruptGetDevice(intr));

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "enabling interrupts");

    cx_write(&dev_ctx->mmio, CX_MISC_PCI_INTERRUPT_MASK_ADDR,
        (CX_MISC_PCI_INTERRUPT_MASK){
            .vid_int = 1
        }.dword);

    return status;
}

_Use_decl_annotations_
NTSTATUS cx_evt_intr_disable(
    WDFINTERRUPT intr,
    WDFDEVICE    dev
)
{
    UNREFERENCED_PARAMETER(dev);

    NTSTATUS status = STATUS_SUCCESS;
    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(WdfInterruptGetDevice(intr));

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "disabling interrupts");
    cx_write(&dev_ctx->mmio, CX_MISC_PCI_INTERRUPT_MASK_ADDR, 0);

    return status;
}

_Use_decl_annotations_
VOID cx_start_capture(
    PDEVICE_CONTEXT dev_ctx
)
{
    if (dev_ctx->state.is_capturing)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "already capturing");
        return;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "starting capture");

    // enable fifo and risc
    cx_write(&dev_ctx->mmio, CX_DMAC_DEVICE_CONTROL_2_ADDR,
        (CX_DMAC_DEVICE_CONTROL_2){
            .run_risc = 1
        }.dword);

    cx_write(&dev_ctx->mmio, CX_VIDEO_IPB_DMA_CONTROL_ADDR,
        (CX_VIDEO_IPB_DMA_CONTROL){
            .vbi_fifo_en = 1,
            .vbi_risc_en = 1
        }.dword);

    // turn on interrupt
    cx_write(&dev_ctx->mmio, CX_DMAC_VIDEO_INTERRUPT_MASK_ADDR,
        (CX_DMAC_VIDEO_INTERRUPT){
            .vbi_risci1 = 1,
            .vbi_risci2 = 1,
            .vbif_of = 1,
            .vbi_sync = 1,
            .opc_err = 1
        }.dword);

    InterlockedExchange((PLONG)&dev_ctx->state.is_capturing, TRUE);
}

_Use_decl_annotations_
VOID cx_stop_capture(
    PDEVICE_CONTEXT dev_ctx
)
{
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "stopping capture");

    InterlockedExchange((PLONG)&dev_ctx->state.is_capturing, FALSE);

    // turn off interrupt
    cx_write(&dev_ctx->mmio, CX_DMAC_VIDEO_INTERRUPT_MASK_ADDR, 0);
    cx_write(&dev_ctx->mmio, CX_DMAC_VIDEO_INTERRUPT_STATUS_ADDR, 0xFFFFFFFF);

    // disable fifo and risc
    cx_write(&dev_ctx->mmio, CX_VIDEO_IPB_DMA_CONTROL_ADDR, 0);

    // disable risc
    cx_write(&dev_ctx->mmio, CX_DMAC_DEVICE_CONTROL_2_ADDR, 0);
}

_Use_decl_annotations_
VOID cx_set_vmux(
    PMMIO mmio,
    ULONG vmux
)
{
    cx_write(mmio, CX_VIDEO_INPUT_FORMAT_ADDR,
        (CX_VIDEO_INPUT_FORMAT){
            .fmt = 1,
            .svid = 1,
            .agcen = 1,
            .yadc_sel = vmux,
            .svid_c_sel = 1,
        }.dword);
}

_Use_decl_annotations_
VOID cx_set_level(
    PMMIO mmio,
    ULONG level,
    BOOLEAN enable_sixdb
)
{
    cx_write(mmio, CX_VIDEO_AGC_GAIN_ADJUST_4_ADDR,
        (CX_VIDEO_AGC_GAIN_ADJUST_4){
            .high_acc_val = 0x00,
            .low_acc_val = 0xFF,
            .init_vga_val = level,
            .vga_en = 0x00,
            .slice_ref_en = 0x00,
            .init_6db_val = enable_sixdb
        }.dword);
}

_Use_decl_annotations_
VOID cx_set_tenbit(
    PMMIO mmio,
    BOOLEAN enable_tenbit
)
{
    cx_write(mmio, CX_VIDEO_CAPTURE_CONTROL_ADDR,
        (CX_VIDEO_CAPTURE_CONTROL){
            .capture_even = 1,
            .capture_odd = 1,
            .raw16 = enable_tenbit,
            .cap_raw_all = 1
        }.dword);
}

_Use_decl_annotations_
VOID cx_set_center_offset(
    PMMIO mmio,
    ULONG center_offset
)
{
    cx_write(mmio, CX_VIDEO_AGC_SYNC_TIP_ADJUST_3_ADDR,
        (CX_VIDEO_AGC_SYNC_TIP_ADJUST_3){
            .acc_max = center_offset,
            .acc_min = 0xFF,
            .low_stip_th = 0x1E48
        }.dword);
}

_Use_decl_annotations_
BOOLEAN cx_get_ouflow_state(
    PMMIO mmio
)
{
    CX_VIDEO_DEVICE_STATUS status =
    {
        .dword = cx_read(mmio, CX_VIDEO_DEVICE_STATUS_ADDR)
    };

    return status.lof ? TRUE : FALSE;
}

_Use_decl_annotations_
VOID cx_reset_ouflow_state(
   PMMIO mmio
)
{
    CX_VIDEO_DEVICE_STATUS status =
    {
        .dword = cx_read(mmio, CX_VIDEO_DEVICE_STATUS_ADDR)
    };

    status.lof = 0;

    cx_write(mmio, CX_VIDEO_DEVICE_STATUS_ADDR, status.dword);
}
