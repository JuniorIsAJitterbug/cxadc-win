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

#define VENDOR_ID    0x14F1
#define DEVICE_ID    0x8800

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD cx_evt_device_add;
EVT_WDF_OBJECT_CONTEXT_CLEANUP cx_evt_device_cleanup;
EVT_WDF_OBJECT_CONTEXT_CLEANUP cx_evt_driver_ctx_cleanup;
EVT_WDF_DEVICE_PREPARE_HARDWARE cx_evt_device_prepare_hardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE cx_evt_device_release_hardware;
EVT_WDF_DEVICE_D0_ENTRY cx_evt_device_d0_entry;
EVT_WDF_DEVICE_D0_EXIT cx_evt_device_d0_exit;

NTSTATUS cx_init_mmio(_In_ PDEVICE_CONTEXT dev_ctx, _In_ PCM_PARTIAL_RESOURCE_DESCRIPTOR desc);
NTSTATUS cx_init_interrupt(
    _In_ PDEVICE_CONTEXT dev_ctx,
    _In_ PCM_PARTIAL_RESOURCE_DESCRIPTOR desc,
    _In_ PCM_PARTIAL_RESOURCE_DESCRIPTOR desc_raw
);

NTSTATUS cx_create_device(_Inout_ PWDFDEVICE_INIT);
NTSTATUS cx_init_device_ctx(_Inout_ PDEVICE_CONTEXT dev_ctx);
NTSTATUS cx_init_dma(_In_ PDEVICE_CONTEXT dev_ctx);
NTSTATUS cx_init_queue(_In_ PDEVICE_CONTEXT dev_ctx);
VOID cx_init_config(_Inout_ PDEVICE_CONTEXT dev_ctx);
VOID cx_init_state(_Inout_ PDEVICE_CONTEXT dev_ctx);

NTSTATUS cx_check_dev_info(_In_ PDEVICE_CONTEXT dev_ctx);
NTSTATUS cx_read_device_prop(
    _In_ PDEVICE_CONTEXT dev_ctx,
    _In_ DEVICE_REGISTRY_PROPERTY prop,
    _Inout_ PULONG value
);
