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


NTSTATUS cx_init(_Inout_ PDEVICE_CONTEXT dev_ctx);
NTSTATUS cx_disable(_Inout_ PMMIO mmio);
VOID cx_reset(_Inout_ PMMIO mmio);
VOID cx_init_cdt(_Inout_ PMMIO mmio);
VOID cx_init_risc(_Inout_ PRISC risc);
VOID cx_init_cmds(_Inout_ PMMIO mmio, _In_ PRISC risc);

EVT_WDF_INTERRUPT_ISR cx_evt_isr;
EVT_WDF_INTERRUPT_DPC cx_evt_dpc;
EVT_WDF_INTERRUPT_ENABLE cx_evt_intr_enable;
EVT_WDF_INTERRUPT_DISABLE cx_evt_intr_disable;

VOID cx_start_capture(_Inout_ PDEVICE_CONTEXT dev_ctx);
VOID cx_stop_capture(_Inout_ PDEVICE_CONTEXT dev_ctx);
VOID cx_set_vmux(_Inout_ PMMIO mmio, _In_ ULONG vmux);
VOID cx_set_level(_Inout_ PMMIO mmio, _In_ ULONG level, _In_ BOOLEAN enable_sixdb);
VOID cx_set_tenbit(_Inout_ PMMIO mmio, _In_ BOOLEAN enable_tenbit);
VOID cx_set_center_offset(_Inout_ PMMIO mmio, _In_ ULONG center_offset);
BOOLEAN cx_get_ouflow_state(_Inout_ PMMIO mmio);
VOID cx_reset_ouflow_state(_Inout_ PMMIO mmio);

__inline ULONG cx_read(_Inout_ PMMIO mmio, _In_ ULONG off);
__inline VOID cx_write(_Inout_ PMMIO mmio, _In_ ULONG off, _In_ ULONG val);
__inline VOID cx_write_buf(_Inout_ PMMIO mmio, _In_ ULONG off, _In_reads_(count) PUCHAR buf, _In_ ULONG count);

#define CX_IRQ_PERIOD_IN_PAGES                  (0x200000 >> PAGE_SHIFT)

#define CX_MEM_SRAM_BASE                        0x180000
#define CX_SRAM_CMDS_VBI_BASE                   (CX_MEM_SRAM_BASE + 0x0100)
#define CX_SRAM_RISC_QUEUE_BASE                 (CX_MEM_SRAM_BASE + 0x0800)
#define CX_SRAM_CDT_BASE                        (CX_MEM_SRAM_BASE + 0x1000)
#define CX_SRAM_CDT_BUF_BASE                    (CX_MEM_SRAM_BASE + 0x4000)

#define CX_REGISTER_BASE                        0x200000
#define CX_REGISTER_END                         (CX_REGISTER_BASE + 0x3CFFFF)

#define CX_DMAC_DEVICE_CONTROL_2_ADDR           0x200034
#define CX_MISC_PCI_INTERRUPT_MASK_ADDR         0x200040
#define CX_DMAC_VIDEO_INTERRUPT_MASK_ADDR       0x200050
#define CX_DMAC_VIDEO_INTERRUPT_STATUS_ADDR     0x200054
#define CX_DMAC_VIDEO_INTERRUPT_MSTATUS_ADDR    0x200058
#define CX_DMAC_VBI_PTR2_ADDR                   0x3000CC
#define CX_DMAC_VBI_CNT1_ADDR                   0x30010C
#define CX_DMAC_VBI_CNT2_ADDR                   0x30014C
#define CX_VIDEO_DEVICE_STATUS_ADDR             0x310100
#define CX_VIDEO_INPUT_FORMAT_ADDR              0x310104
#define CX_VIDEO_CONTRAST_BRIGHTNESS_ADDR       0x310110
#define CX_VIDEO_OUTPUT_CONTROL_ADDR            0x310164
#define CX_VIDEO_PLL_ADDR                       0x310168
#define CX_VIDEO_PLL_ADJUST_ADDR                0x31016C
#define CX_VIDEO_SAMPLE_RATE_CONVERSION_ADDR    0x310170
#define CX_VIDEO_CAPTURE_CONTROL_ADDR           0x310180
#define CX_VIDEO_COLOR_FORMAT_CONTROL_ADDR      0x310184
#define CX_VIDEO_VBI_PACKET_SIZE_DELAY_ADDR     0x310188
#define CX_VIDEO_AGC_CONTROL_ADDR               0x310200
#define CX_VIDEO_AGC_SYNC_SLICER_ADDR           0x310204
#define CX_VIDEO_AGC_SYNC_TIP_ADJUST_1_ADDR     0x310208
#define CX_VIDEO_AGC_SYNC_TIP_ADJUST_2_ADDR     0x31020C
#define CX_VIDEO_AGC_SYNC_TIP_ADJUST_3_ADDR     0x310210
#define CX_VIDEO_AGC_GAIN_ADJUST_1_ADDR         0x310214
#define CX_VIDEO_AGC_GAIN_ADJUST_2_ADDR         0x310218
#define CX_VIDEO_AGC_GAIN_ADJUST_3_ADDR         0x31021C
#define CX_VIDEO_AGC_GAIN_ADJUST_4_ADDR         0x310220
#define CX_VIDEO_VBI_GP_COUNTER_ADDR            0x31C02C
#define CX_VIDEO_IPB_DMA_CONTROL_ADDR           0x31C040
#define CX_MISC_AFECFG_ADDR                     0x35C04C
#define CX_I2C_DATA_CONTROL_ADDR                0x368000

// bitfields for above addrs
// see datasheet for descriptions
// DMAC
typedef union
{
    struct
    {
        ULONG                       : 5;
        ULONG run_risc              : 1;
        ULONG                       : 26;
    };

    ULONG dword;
} CX_DMAC_DEVICE_CONTROL_2;

typedef union
{
    struct
    {
        ULONG vid_int               : 1;
        ULONG aud_int               : 1;
        ULONG ts_int                : 1;
        ULONG vip_int               : 1;
        ULONG hst_int               : 1;
        ULONG                       : 2;
        ULONG tm1_int               : 1;
        ULONG src_dma_int           : 1;
        ULONG dst_dma_int           : 1;
        ULONG risc_rd_berr_int      : 1;
        ULONG risc_wr_berr_int      : 1;
        ULONG brdg_berr_int         : 1;
        ULONG src_dma_berr_int      : 1;
        ULONG dst_dma_berr_int      : 1;
        ULONG ipb_dma_berr_int      : 1;
        ULONG i2c_int               : 1;
        ULONG i2c_rack              : 1;
        ULONG ir_smp_int            : 1;
        ULONG gpio_int0             : 1;
        ULONG gpio_int1             : 1;
        ULONG                       : 11;
    };

    ULONG dword;
} CX_MISC_PCI_INTERRUPT_MASK, *PCX_MISC_PCI_INTERRUPT_MASK;

typedef union
{
    struct
    {
        ULONG y_risci1              : 1;
        ULONG u_risci1              : 1;
        ULONG v_risci1              : 1;
        ULONG vbi_risci1            : 1;
        ULONG y_risci2              : 1;
        ULONG u_risci2              : 1;
        ULONG v_risci2              : 1;
        ULONG vbi_risci2            : 1;
        ULONG yf_of                 : 1;
        ULONG uf_of                 : 1;
        ULONG vf_of                 : 1;
        ULONG vbif_of               : 1;
        ULONG y_sync                : 1;
        ULONG u_sync                : 1;
        ULONG v_sync                : 1;
        ULONG vbi_sync              : 1;
        ULONG opc_err               : 1;
        ULONG par_err               : 1;
        ULONG rip_err               : 1;
        ULONG pci_abort             : 1;
        ULONG                       : 12;
    };

    ULONG dword;
} CX_DMAC_VIDEO_INTERRUPT, *PCX_DMAC_VIDEO_INTERRUPT;

typedef union
{
    struct
    {
        ULONG                       : 2;
        ULONG dma_ptr2              : 22;
        ULONG                       : 8; 
    };

    ULONG dword;
} CX_DMAC_DMA_PTR2;


typedef union
{
    struct
    {
        ULONG dma_cnt1              : 11;
        ULONG                       : 21;
    };

    ULONG dword;
} CX_DMAC_DMA_CNT1;


typedef union
{
    struct
    {
        ULONG dma_cnt2              : 11;
        ULONG                       : 21;
    };

    ULONG dword;
} CX_DMAC_DMA_CNT2;

// video
typedef union
{
    struct
    {
        ULONG cof                   : 1;
        ULONG lof                   : 1;
        ULONG pll                   : 1;
        ULONG numl                  : 1;
        ULONG field                 : 1;
        ULONG hlock                 : 1;
        ULONG vpres                 : 1;
        ULONG nsplay                : 1;
        ULONG                       : 8;
        ULONG shcerr                : 15;
        ULONG                       : 1;
    };

    ULONG dword;
} CX_VIDEO_DEVICE_STATUS, *PCX_VIDEO_DEVICE_STATUS;

typedef union
{
    struct
    {
        ULONG fmt                   : 4;
        ULONG svid                  : 1;
        ULONG                       : 2;
        ULONG verten                : 1;
        ULONG scspd                 : 1;
        ULONG ckillen               : 1;
        ULONG cagcen                : 1;
        ULONG wcen                  : 1;
        ULONG ncagc                 : 1;
        ULONG agcen                 : 1;
        ULONG yadc_sel              : 2;
        ULONG svid_c_sel            : 1;
        ULONG pesrc_sel             : 1;
        ULONG                       : 14;
    };

    ULONG dword;
} CX_VIDEO_INPUT_FORMAT;

typedef union
{
    struct
    {
        ULONG brite                 : 8;
        ULONG cntrst                : 8;
        ULONG                       : 16;
    };

    ULONG dword;
} CX_VIDEO_CONTRAST_BRIGHTNESS;

typedef union
{
    struct
    {
        ULONG                       : 1;
        ULONG hsfmt                 : 1;
        ULONG hactext               : 1;
        ULONG range                 : 1;
        ULONG ccore                 : 2;
        ULONG ycore                 : 2;
        ULONG nremoden              : 1;
        ULONG nchromaen             : 1;
        ULONG forceremd             : 1;
        ULONG force2h               : 1;
        ULONG narrowadapt           : 1;
        ULONG disadapt              : 1;
        ULONG invcbf                : 1;
        ULONG disifx                : 1;
        ULONG comb_range            : 10;
        ULONG pal_inv_phase         : 1;
        ULONG combalt               : 1;
        ULONG prevremod             : 1;
        ULONG                       : 3;
    };

    ULONG dword;
} CX_VIDEO_OUTPUT_CONTROL;

typedef union
{
    struct
    {
        ULONG pll_frac              : 20;
        ULONG pll_int               : 6;
        ULONG pll_pre               : 2;
        ULONG pll_dds               : 1;
        ULONG                       : 3;
    };

    ULONG dword;
} CX_VIDEO_PLL;


typedef union
{
    struct
    {
        ULONG pll_th1               : 7;
        ULONG pll_th2               : 7;
        ULONG pll_drift_th          : 5;
        ULONG pll_max_offset        : 6;
        ULONG pll_adj_en            : 1;
        ULONG                       : 6;
    };

    ULONG dword;
} CX_VIDEO_PLL_ADJUST;

typedef union
{
    struct
    {
        ULONG src_reg_val           : 19;
        ULONG                       : 13;
    };

    ULONG dword;
} CX_VIDEO_SAMPLE_RATE_CONVERSION;

typedef union
{
    struct
    {
        ULONG frm_dith              : 1;
        ULONG capture_even          : 1;
        ULONG capture_odd           : 1;
        ULONG capture_vbi_even      : 1;
        ULONG capture_vbi_odd       : 1;
        ULONG raw16                 : 1;
        ULONG cap_raw_all           : 1;
        ULONG                       : 25;
    };

    ULONG dword;
} CX_VIDEO_CAPTURE_CONTROL;

typedef union
{
    struct
    {
        ULONG color_even            : 4;
        ULONG color_odd             : 4;
        ULONG bswap_even            : 1;
        ULONG bswap_odd             : 1;
        ULONG wswap_even            : 1;
        ULONG wswap_odd             : 1;
        ULONG gamma_dis             : 1;
        ULONG rgb_ded               : 1;
        ULONG color_en              : 1;
        ULONG                       : 17;
    };

    ULONG dword;
} CX_VIDEO_COLOR_FORMAT_CONTROL;

typedef union
{
    struct
    {
        ULONG vbi_pkt_size          : 10;
        ULONG _extern               : 1;
        ULONG vbi_v_del             : 6;
        ULONG frm_size              : 12;
        ULONG                       : 3;
    };

    ULONG dword;
} CX_VIDEO_VBI_PACKET_SIZE_DELAY;

typedef union
{
    struct
    {
        ULONG intrvl_cnt_val        : 12;
        ULONG                       : 4;
        ULONG bp_ref                : 9;
        ULONG bp_ref_sel            : 1;
        ULONG agc_vbi_en            : 1;
        ULONG clamp_vbi_en          : 1;
        ULONG                       : 4;
    };

    ULONG dword;
} CX_VIDEO_AGC_CONTROL;

typedef union
{
    struct
    {
        ULONG sync_sam_dly          : 8;
        ULONG bp_sam_dly            : 8;
        ULONG mm_multi              : 3;
        ULONG std_slice_en          : 1;
        ULONG sam_slice_en          : 1;
        ULONG dly_upd_en            : 1;
        ULONG                       : 10;
    };

    ULONG dword;
} CX_VIDEO_AGC_SYNC_SLICER;

typedef union
{
    struct
    {
        ULONG trk_sat_val           : 7;
        ULONG trk_g_val             : 2;
        ULONG trk_core_thr          : 8;
        ULONG trk_mode_thr          : 12;
        ULONG                       : 3;
    };

    ULONG dword;
} CX_VIDEO_AGC_SYNC_TIP_ADJUST_1;

typedef union
{
    struct
    {
        ULONG acq_sat_val           : 7;
        ULONG acq_g_val             : 2;
        ULONG acq_core_thr          : 8;
        ULONG acq_mode_thr          : 12;
        ULONG                       : 3;
    };

    ULONG dword;
} CX_VIDEO_AGC_SYNC_TIP_ADJUST_2;

typedef union
{
    struct
    {
        ULONG acc_max               : 8;
        ULONG acc_min               : 8;
        ULONG low_stip_th           : 13;
        ULONG                       : 3;
    };

    ULONG dword;
} CX_VIDEO_AGC_SYNC_TIP_ADJUST_3;

typedef union
{
    struct
    {
        ULONG trk_agc_sat_val       : 7;
        ULONG trk_gain_val          : 2;
        ULONG trk_agc_core_th_val   : 8;
        ULONG trk_agc_mode_th       : 12;
        ULONG                       : 3;
    };

    ULONG dword;
} CX_VIDEO_AGC_GAIN_ADJUST_1;

typedef union
{
    struct
    {
        ULONG acq_agc_sat_val       : 7;
        ULONG acq_gain_val          : 2;
        ULONG acq_agc_core_th_val   : 8;
        ULONG acq_agc_mode_th       : 12;
        ULONG                       : 3;
    };

    ULONG dword;
} CX_VIDEO_AGC_GAIN_ADJUST_2;

typedef union
{
    struct
    {
        ULONG acc_inc_val           : 8;
        ULONG acc_max_val           : 8;
        ULONG acc_min_val           : 8;
        ULONG                       : 8;
    };

    ULONG dword;
} CX_VIDEO_AGC_GAIN_ADJUST_3;

typedef union
{
    struct
    {
        ULONG high_acc_val          : 8;
        ULONG low_acc_val           : 8;
        ULONG init_vga_val          : 5;
        ULONG vga_en                : 1;
        ULONG slice_ref_en          : 1;
        ULONG init_6db_val          : 1;
        ULONG                       : 8;
    };

    ULONG dword;
} CX_VIDEO_AGC_GAIN_ADJUST_4;

typedef union
{
    struct
    {
        ULONG gp_cnt                : 16;
        ULONG                       : 16;
    };

    ULONG dword;
} CX_VIDEO_GP_COUNTER, *PCX_VIDEO_GP_COUNTER;

typedef union
{
    struct
    {
        ULONG vidy_fifo_en          : 1;
        ULONG vidu_fifo_en          : 1;
        ULONG vidv_fifo_en          : 1;
        ULONG vbi_fifo_en           : 1;
        ULONG vidy_risc_en          : 1;
        ULONG vidu_risc_en          : 1;
        ULONG vidv_risc_en          : 1;
        ULONG vbi_risc_en           : 1;
        ULONG                       : 24;
    };

    ULONG dword;
} CX_VIDEO_IPB_DMA_CONTROL;

// Misc
typedef union
{
    struct
    {
        ULONG v_a_mode              : 1;
        ULONG bg_pwrdn              : 1;
        ULONG c_pwrdn               : 1;
        ULONG y_pwrdn               : 1;
        ULONG dac_pwrdn             : 1;
        ULONG                       : 27;
    };

    ULONG dword;
} CX_MISC_AFECFG;

// I2C
typedef union
{
    struct
    {
        ULONG sda                   : 1;
        ULONG scl                   : 1;
        ULONG w3bra                 : 1;
        ULONG sync                  : 1;
        ULONG nos1b                 : 1;
        ULONG nostop                : 1;
        ULONG rate                  : 1;
        ULONG mode                  : 1;
        ULONG db2                   : 8;
        ULONG db1                   : 8;
        ULONG db0                   : 8; 
    };

    ULONG dword;
} CX_I2C_DATA_CONTROL;

// RISC instructions
#define CX_RISC_INSTR_WRITE_OPCODE      1
#define CX_RISC_INSTR_JUMP_OPCODE       7
#define CX_RISC_INSTR_SYNC_OPCODE       8

typedef struct _CX_RISC_INSTR_WRITE
{
    ULONG byte_count                : 12;
    ULONG                           : 4;
    ULONG cnt_ctl                   : 2;
    ULONG                           : 6;
    ULONG irq1                      : 1;
    ULONG irq2                      : 1;
    ULONG eol                       : 1;
    ULONG sol                       : 1;
    ULONG opcode                    : 4;

    ULONG pci_target_address;
} CX_RISC_INSTR_WRITE, *PCX_RISC_INSTR_WRITE;

typedef struct _CX_RISC_INSTR_SYNC
{
    ULONG line_count                : 10;
    ULONG                           : 5;
    ULONG resync                    : 1;
    ULONG cnt_ctl                   : 2;
    ULONG                           : 6;
    ULONG irq1                      : 1;
    ULONG irq2                      : 1;
    ULONG                           : 2;
    ULONG opcode                    : 4;
} CX_RISC_INSTR_SYNC, *PCX_RISC_INSTR_SYNC;

typedef struct _CX_RISC_INSTR_JUMP
{
    ULONG srp                       : 1;
    ULONG                           : 15;
    ULONG cnt_ctl                   : 2;
    ULONG                           : 6;
    ULONG irq1                      : 1;
    ULONG irq2                      : 1;
    ULONG                           : 2;
    ULONG opcode                    : 4;

    ULONG jump_address;
} CX_RISC_INSTR_JUMP, *PCX_RISC_INSTR_JUMP;

typedef struct _CX_RISC_INSTRUCTIONS
{
    CX_RISC_INSTR_SYNC sync_instr;
    CX_RISC_INSTR_WRITE write_instr[CX_VBI_BUF_COUNT * (PAGE_SIZE / CX_CDT_BUF_LEN)];
    CX_RISC_INSTR_JUMP jump_instr;
} CX_RISC_INSTRUCTIONS, *PCX_RISC_INSTRUCTIONS;

// CDT
typedef union _CX_CDT_DESCRIPTOR
{
    struct {
        ULONG buffer_ptr;
        ULONG reserved[3];
    };

    UCHAR data[0x10];
} CX_CDT_DESCRIPTOR, *PCX_CDT_DESCRIPTOR;

// Channel Management Data Structure (CMDS)
typedef union _CX_CMDS
{
    struct {
        ULONG initial_risc_addr;
        ULONG cdt_base              : 24;
        ULONG                       : 8;
        ULONG cdt_size              : 11;
        ULONG                       : 21;
        ULONG risc_base             : 24;
        ULONG                       : 8;
        ULONG risc_size             : 8;
        ULONG                       : 23;
        ULONG isrp                  : 1;
    };

    UCHAR data[0x14];
} CX_CMDS, *PCX_CMDS;