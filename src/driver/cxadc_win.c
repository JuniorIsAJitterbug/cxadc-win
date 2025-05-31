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
#include "cxadc_win.tmh"
#include "cxadc_win.h"

#include "cx2388x.h"
#include "registry.h"
#include "wmi.h"
#include "ioctl.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, cx_evt_device_add)
#pragma alloc_text (PAGE, cx_evt_device_cleanup)
#pragma alloc_text (PAGE, cx_evt_driver_ctx_cleanup)
#pragma alloc_text (PAGE, cx_evt_device_prepare_hardware)
#pragma alloc_text (PAGE, cx_evt_device_release_hardware)
#pragma alloc_text (PAGE, cx_evt_device_d0_entry)
#pragma alloc_text (PAGE, cx_evt_device_d0_exit)

#pragma alloc_text (PAGE, cx_init_mmio)
#pragma alloc_text (PAGE, cx_init_interrupt)
#pragma alloc_text (PAGE, cx_init_device)
#pragma alloc_text (PAGE, cx_init_dma)
#pragma alloc_text (PAGE, cx_init_queue)
#pragma alloc_text (PAGE, cx_check_dev_info)
#pragma alloc_text (PAGE, cx_read_device_prop)
#endif

_Use_decl_annotations_
NTSTATUS DriverEntry(
    PDRIVER_OBJECT driver_obj,
    PUNICODE_STRING reg_path
)
{
    WDF_OBJECT_ATTRIBUTES attrs;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attrs, DRIVER_CONTEXT);
    attrs.EvtCleanupCallback = cx_evt_driver_ctx_cleanup;

    WDF_DRIVER_CONFIG cfg;
    WDF_DRIVER_CONFIG_INIT(&cfg, cx_evt_device_add);

    WDFDRIVER driver = WDF_NO_HANDLE;
    RETURN_NTSTATUS_IF_FAILED(WdfDriverCreate(driver_obj, reg_path, &attrs, &cfg, &driver));

    WPP_INIT_TRACING(driver_obj, reg_path);
    TRACE_INFO("cxadc-win entry");

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS cx_evt_device_add(
    WDFDRIVER driver,
    PWDFDEVICE_INIT dev_init
)
{
    PAGED_CODE();

    // pnp
    WDF_PNPPOWER_EVENT_CALLBACKS pnp_callbacks;
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp_callbacks);
    pnp_callbacks.EvtDevicePrepareHardware = cx_evt_device_prepare_hardware;
    pnp_callbacks.EvtDeviceReleaseHardware = cx_evt_device_release_hardware;
    pnp_callbacks.EvtDeviceD0Entry = cx_evt_device_d0_entry;
    pnp_callbacks.EvtDeviceD0Exit = cx_evt_device_d0_exit;
    WdfDeviceInitSetPnpPowerEventCallbacks(dev_init, &pnp_callbacks);

    // file callbacks
    WDF_FILEOBJECT_CONFIG file_obj_cfg;
    WDF_FILEOBJECT_CONFIG_INIT(&file_obj_cfg, cx_evt_file_create, cx_evt_file_close, cx_evt_file_cleanup);

    WDF_OBJECT_ATTRIBUTES file_attrs;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&file_attrs, FILE_CONTEXT);
    WdfDeviceInitSetFileObjectConfig(dev_init, &file_obj_cfg, &file_attrs);

    WDF_OBJECT_ATTRIBUTES dev_attrs;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&dev_attrs, DEVICE_CONTEXT);
    dev_attrs.EvtCleanupCallback = cx_evt_device_cleanup;

    // assign NT name
    PDRIVER_CONTEXT driver_ctx = cx_driver_get_ctx(driver);
    LONG dev_idx = InterlockedIncrement(&driver_ctx->dev_count) - 1;

    DECLARE_UNICODE_STRING_SIZE(dev_path, MAX_PATH);
    RETURN_NTSTATUS_IF_FAILED(RtlUnicodeStringPrintf(&dev_path, L"%ws%d", NT_PATH, dev_idx));
    RETURN_NTSTATUS_IF_FAILED(WdfDeviceInitAssignName(dev_init, &dev_path));

    // set io types
    WdfDeviceInitSetIoType(dev_init, WdfDeviceIoDirect);
    WdfDeviceInitSetDeviceType(dev_init, FILE_DEVICE_NAMED_PIPE); // lets non-Win32 functions (e.g. fread) read the device

    // create device
    WDFDEVICE dev = WDF_NO_HANDLE;
    RETURN_NTSTATUS_IF_FAILED(WdfDeviceCreate(&dev_init, &dev_attrs, &dev));
    RETURN_NTSTATUS_IF_FAILED(WdfDeviceCreateDeviceInterface(dev, (LPGUID)&GUID_DEVINTERFACE_CXADCWIN, NULL));

    // check valid pci id
    RETURN_NTSTATUS_IF_FAILED(cx_check_dev_info(dev));

    // create symlink
    DECLARE_UNICODE_STRING_SIZE(symlink_path, MAX_PATH);
    RETURN_NTSTATUS_IF_FAILED(RtlUnicodeStringPrintf(&symlink_path, L"%ws%d", SYMLINK_PATH, dev_idx));
    RETURN_NTSTATUS_IF_FAILED(WdfDeviceCreateSymbolicLink(dev, &symlink_path));
    TRACE_INFO("created symlink %wZ", &symlink_path);

    // init device
    RETURN_NTSTATUS_IF_FAILED(cx_init_device(dev, dev_idx));

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
VOID cx_evt_device_cleanup(
    WDFOBJECT device_obj
)
{
    UNREFERENCED_PARAMETER(device_obj);
    PAGED_CODE();
    // nothing to do ??
}

_Use_decl_annotations_
VOID cx_evt_driver_ctx_cleanup(
    WDFOBJECT driver_obj
)
{
    PAGED_CODE();
    WPP_CLEANUP(WdfDriverWdmGetDriverObject((WDFDRIVER)driver_obj));
}

_Use_decl_annotations_
NTSTATUS cx_evt_device_prepare_hardware(
    WDFDEVICE dev,
    WDFCMRESLIST res,
    WDFCMRESLIST res_trans
)
{
    PAGED_CODE();

    // ensure we stick to arbitrary device count
    PDRIVER_CONTEXT driver_ctx = cx_driver_get_ctx(WdfGetDriver());

    if (driver_ctx->dev_count >= MAXUCHAR)
    {
        TRACE_ERROR("too many devices (%d)", driver_ctx->dev_count);
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(dev);

    for (ULONG i = 0; i < WdfCmResourceListGetCount(res_trans); i++)
    {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR desc = WdfCmResourceListGetDescriptor(res_trans, i);

        if (!desc)
        {
            return STATUS_DEVICE_CONFIGURATION_ERROR;
        }

        switch (desc->Type)
        {
            case CmResourceTypeMemory:
                RETURN_NTSTATUS_IF_FAILED(cx_init_mmio(dev_ctx, desc));
                break;

            case CmResourceTypeInterrupt:
                RETURN_NTSTATUS_IF_FAILED(cx_init_interrupt(dev_ctx, desc, WdfCmResourceListGetDescriptor(res, i)));
                break;

            default:
                break;
        }
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS cx_init_mmio(
    PDEVICE_CONTEXT dev_ctx,
    PCM_PARTIAL_RESOURCE_DESCRIPTOR desc
)
{
    PAGED_CODE();

    dev_ctx->mmio.base = MmMapIoSpaceEx(desc->u.Memory.Start, desc->u.Memory.Length, PAGE_NOCACHE | PAGE_READWRITE);

    if (!dev_ctx->mmio.base)
    {
        TRACE_ERROR("MmMapIoSpaceEx failed to create %I64X->%I64X (%08X)",
            desc->u.Memory.Start.QuadPart,
            desc->u.Memory.Start.QuadPart + desc->u.Memory.Length,
            desc->u.Memory.Length);

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    TRACE_INFO("mmio created %I64X->%I64X (%08X)",
        desc->u.Memory.Start.QuadPart,
        desc->u.Memory.Start.QuadPart + desc->u.Memory.Length,
        desc->u.Memory.Length);

    dev_ctx->mmio.len = desc->u.Memory.Length;

    // create mdl for user mmap
    dev_ctx->user_mdl = IoAllocateMdl((PVOID)dev_ctx->mmio.base, dev_ctx->mmio.len, FALSE, FALSE, NULL);

    if (!dev_ctx->user_mdl)
    {
        TRACE_ERROR("IoAllocateMdl failed");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    MmBuildMdlForNonPagedPool(dev_ctx->user_mdl);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS cx_init_interrupt(
    PDEVICE_CONTEXT dev_ctx,
    PCM_PARTIAL_RESOURCE_DESCRIPTOR desc,
    PCM_PARTIAL_RESOURCE_DESCRIPTOR desc_raw
)
{
    PAGED_CODE();

    TRACE_INFO("intrs lvl 0x%08X vec 0x%08X", desc->u.Interrupt.Level, desc->u.Interrupt.Vector);

    WDF_INTERRUPT_CONFIG intr_cfg;
    WDF_INTERRUPT_CONFIG_INIT(&intr_cfg, cx_evt_isr, cx_evt_dpc);
    intr_cfg.InterruptTranslated = desc;
    intr_cfg.InterruptRaw = desc_raw;
    intr_cfg.EvtInterruptEnable = cx_evt_intr_enable;
    intr_cfg.EvtInterruptDisable = cx_evt_intr_disable;

    RETURN_NTSTATUS_IF_FAILED(WdfInterruptCreate(dev_ctx->dev, &intr_cfg, WDF_NO_OBJECT_ATTRIBUTES, &dev_ctx->intr));

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS cx_evt_device_release_hardware(
    WDFDEVICE dev,
    WDFCMRESLIST res_trans
)
{
    UNREFERENCED_PARAMETER(res_trans);
    PAGED_CODE();

    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(dev);

    if (dev_ctx->mmio.base != NULL)
    {
        MmUnmapIoSpace((PVOID)dev_ctx->mmio.base, dev_ctx->mmio.len);
        dev_ctx->mmio.base = NULL;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS cx_evt_device_d0_entry(
    WDFDEVICE dev,
    WDF_POWER_DEVICE_STATE prev_state
)
{
    UNREFERENCED_PARAMETER(prev_state);
    PAGED_CODE();

    cx_init(cx_device_get_ctx(dev));

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS cx_evt_device_d0_exit(
    WDFDEVICE dev,
    WDF_POWER_DEVICE_STATE target_state
)
{
    PAGED_CODE();
    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(dev);

    // should already be stopped
    cx_stop_capture(dev_ctx);
    cx_disable(&dev_ctx->mmio);

    switch (target_state)
    {
        case WdfPowerDeviceD3Final:
            cx_reset(&dev_ctx->mmio);
            break;

        case WdfPowerDeviceD0:
        case WdfPowerDeviceD1:
        case WdfPowerDeviceD2:
        case WdfPowerDeviceD3:
        case WdfPowerDevicePrepareForHibernation:
        case WdfPowerDeviceInvalid:
        case WdfPowerDeviceMaximum:
        default:
            break;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS cx_init_device(
    WDFDEVICE dev,
    LONG dev_idx
)
{
    PAGED_CODE();

    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(dev);
    dev_ctx->dev = dev;
    dev_ctx->dev_idx = dev_idx;

    RETURN_NTSTATUS_IF_FAILED(cx_read_device_prop(dev_ctx, DevicePropertyBusNumber, &dev_ctx->bus_number));
    RETURN_NTSTATUS_IF_FAILED(cx_read_device_prop(dev_ctx, DevicePropertyAddress, &dev_ctx->dev_addr));

    TRACE_INFO("device idx: %d, bus location: %02d:%02d.%01d",
        dev_idx, dev_ctx->bus_number, (dev_ctx->dev_addr >> 16) & 0x0000FFFF, dev_ctx->dev_addr & 0x0000FFFF);

    // init ctx data
    cx_init_config(dev_ctx);
    dev_ctx->state = (DEVICE_STATE){ 0 };

    // init interrupt event
    KeInitializeEvent(&dev_ctx->isr_event, NotificationEvent, FALSE);

    RETURN_NTSTATUS_IF_FAILED(cx_init_dma(dev_ctx));
    RETURN_NTSTATUS_IF_FAILED(cx_init_queue(dev_ctx));
    RETURN_NTSTATUS_IF_FAILED(cx_wmi_register(dev_ctx));

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS cx_init_dma(
    PDEVICE_CONTEXT dev_ctx
)
{
    PAGED_CODE();

    // device is 32-bit aligned
    WdfDeviceSetAlignmentRequirement(dev_ctx->dev, FILE_LONG_ALIGNMENT);

    WDF_DMA_ENABLER_CONFIG dma_cfg;
    WDF_DMA_ENABLER_CONFIG_INIT(&dma_cfg, WdfDmaProfilePacket, CX_VBI_BUF_SIZE);
    dma_cfg.WdmDmaVersionOverride = 3;

    RETURN_NTSTATUS_IF_FAILED(WdfDmaEnablerCreate(dev_ctx->dev, &dma_cfg, WDF_NO_OBJECT_ATTRIBUTES, &dev_ctx->dma_enabler));

    // risc instructions
    WDFCOMMONBUFFER instr_buf = WDF_NO_HANDLE;
    RETURN_NTSTATUS_IF_FAILED(WdfCommonBufferCreate(dev_ctx->dma_enabler, CX_RISC_INSTR_BUF_SIZE, WDF_NO_OBJECT_ATTRIBUTES, &instr_buf));

    dev_ctx->risc.instructions = (DMA_DATA){
        .buf = instr_buf,
        .len = CX_RISC_INSTR_BUF_SIZE,
        .va = (PUCHAR)WdfCommonBufferGetAlignedVirtualAddress(instr_buf),
        .la = WdfCommonBufferGetAlignedLogicalAddress(instr_buf)
    };

    RtlZeroMemory(dev_ctx->risc.instructions.va, dev_ctx->risc.instructions.len);

    TRACE_INFO("created risc instr dma 0x%p (%I64X) (%u kbytes)",
        dev_ctx->risc.instructions.va,
        dev_ctx->risc.instructions.la.QuadPart,
        (ULONG)(WdfCommonBufferGetLength(dev_ctx->risc.instructions.buf) / 1024));

    // data pages
    for (ULONG i = 0; i < CX_VBI_BUF_COUNT; i++)
    {
        WDFCOMMONBUFFER page_buf = WDF_NO_HANDLE;
        RETURN_NTSTATUS_IF_FAILED(WdfCommonBufferCreate(dev_ctx->dma_enabler, PAGE_SIZE, WDF_NO_OBJECT_ATTRIBUTES, &page_buf));

        dev_ctx->risc.page[i] = (DMA_DATA){
            .buf = page_buf,
            .len = PAGE_SIZE,
            .va = WdfCommonBufferGetAlignedVirtualAddress(page_buf),
            .la = WdfCommonBufferGetAlignedLogicalAddress(page_buf)
        };

        RtlZeroMemory(dev_ctx->risc.page[i].va, dev_ctx->risc.page[i].len);
    }

    TRACE_INFO("created %d data pages", CX_VBI_BUF_COUNT);
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS cx_init_queue(
    PDEVICE_CONTEXT dev_ctx
)
{
    PAGED_CODE();

    WDF_IO_QUEUE_CONFIG queue_cfg;

    // control queue
    WDF_IO_QUEUE_CONFIG_INIT(&queue_cfg, WdfIoQueueDispatchSequential);
    queue_cfg.EvtIoDeviceControl = cx_evt_io_ctrl;
    RETURN_NTSTATUS_IF_FAILED(WdfIoQueueCreate(dev_ctx->dev, &queue_cfg, WDF_NO_OBJECT_ATTRIBUTES, &dev_ctx->control_queue));
    RETURN_NTSTATUS_IF_FAILED(WdfDeviceConfigureRequestDispatching(dev_ctx->dev, dev_ctx->control_queue, WdfRequestTypeDeviceControl));

    // read queue
    WDF_IO_QUEUE_CONFIG_INIT(&queue_cfg, WdfIoQueueDispatchSequential);
    queue_cfg.EvtIoRead = cx_evt_io_read;
    RETURN_NTSTATUS_IF_FAILED(WdfIoQueueCreate(dev_ctx->dev, &queue_cfg, WDF_NO_OBJECT_ATTRIBUTES, &dev_ctx->read_queue));
    RETURN_NTSTATUS_IF_FAILED(WdfDeviceConfigureRequestDispatching(dev_ctx->dev, dev_ctx->read_queue, WdfRequestTypeRead));

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
VOID cx_init_config(
    PDEVICE_CONTEXT dev_ctx
)
{
    ULONG value = 0;

    // init with reg or default values
    dev_ctx->config = (DEVICE_CONFIG){
        .vmux = NT_SUCCESS(cx_reg_get_value(dev_ctx->dev, CX_CTRL_CONFIG_VMUX_REG_KEY, &value)) ?
            value : CX_CTRL_CONFIG_VMUX_DEFAULT,
        .level = NT_SUCCESS(cx_reg_get_value(dev_ctx->dev, CX_CTRL_CONFIG_LEVEL_REG_KEY, &value)) ?
            value : CX_CTRL_CONFIG_LEVEL_DEFAULT,
        .tenbit = (NT_SUCCESS(cx_reg_get_value(dev_ctx->dev, CX_CTRL_CONFIG_TENBIT_REG_KEY, &value)) ?
            value : CX_CTRL_CONFIG_TENBIT_DEFAULT) ? TRUE : FALSE,
        .sixdb = (NT_SUCCESS(cx_reg_get_value(dev_ctx->dev, CX_CTRL_CONFIG_SIXDB_REG_KEY, &value)) ?
            value : CX_CTRL_CONFIG_SIXDB_DEFAULT) ? TRUE : FALSE,
        .center_offset = NT_SUCCESS(cx_reg_get_value(dev_ctx->dev, CX_CTRL_CONFIG_CENTER_OFFSET_REG_KEY, &value)) ?
            value : CX_CTRL_CONFIG_CENTER_OFFSET_DEFAULT
    };
}

_Use_decl_annotations_
NTSTATUS cx_check_dev_info(
    WDFDEVICE dev
)
{
    PAGED_CODE();

    BUS_INTERFACE_STANDARD bus = { 0 };

    // get pci bus interface
    RETURN_NTSTATUS_IF_FAILED(WdfFdoQueryForInterface(
        dev,
        &GUID_BUS_INTERFACE_STANDARD,
        (PINTERFACE)&bus,
        sizeof(BUS_INTERFACE_STANDARD),
        1,
        NULL));

    PCI_COMMON_CONFIG pci_config = { 0 };

    // read first 64 bytes from pci config
    if (bus.GetBusData(bus.Context, PCI_WHICHSPACE_CONFIG, &pci_config, 0, PCI_COMMON_HDR_LENGTH) != PCI_COMMON_HDR_LENGTH)
    {
        TRACE_ERROR("GetBusData failed");
        return STATUS_UNSUCCESSFUL;
    }

    // ensure device/vendor id is correct
    if (pci_config.VendorID != VENDOR_ID || pci_config.DeviceID != DEVICE_ID)
    {
        TRACE_ERROR("unknown vendor/device id %04X:%04X", pci_config.VendorID, pci_config.DeviceID);
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS cx_read_device_prop(
    PDEVICE_CONTEXT dev_ctx,
    DEVICE_REGISTRY_PROPERTY prop,
    PULONG value
)
{
    PAGED_CODE();

    ULONG len;
    return WdfDeviceQueryProperty(dev_ctx->dev, prop, sizeof(ULONG), value, &len);
}
