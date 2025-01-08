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

#include "precomp.h"
#include "cxadc_win.h"
#include "cxadc_win.tmh"

#include "cx2388x.h"
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
#pragma alloc_text (PAGE, cx_init_device_ctx)
#pragma alloc_text (PAGE, cx_init_dma)
#pragma alloc_text (PAGE, cx_init_queue)
#pragma alloc_text (PAGE, cx_check_dev_info)
#pragma alloc_text (PAGE, cx_read_device_prop)
#endif

UCHAR dev_count = 0;

NTSTATUS DriverEntry(
    _In_    PDRIVER_OBJECT  driver_obj,
    _In_    PUNICODE_STRING reg_path
)
{
    NTSTATUS status = STATUS_SUCCESS;
    WDF_DRIVER_CONFIG cfg;
    WDF_OBJECT_ATTRIBUTES attrs;

    WPP_INIT_TRACING(driver_obj, reg_path);
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "cxadc-win entry");

    WDF_OBJECT_ATTRIBUTES_INIT(&attrs);
    attrs.EvtCleanupCallback = cx_evt_driver_ctx_cleanup;

    WDF_DRIVER_CONFIG_INIT(&cfg, cx_evt_device_add);

    status = WdfDriverCreate(driver_obj, reg_path, &attrs, &cfg, WDF_NO_HANDLE);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfDriverCreate failed with status %!STATUS!", status);
        WPP_CLEANUP(driver_obj);
        return status;
    }

    return status;
}

NTSTATUS cx_evt_device_add(
    _In_    WDFDRIVER driver,
    _Inout_ PWDFDEVICE_INIT dev_init
)
{
    NTSTATUS status;
    WDF_PNPPOWER_EVENT_CALLBACKS pnp_callbacks;
    WDF_OBJECT_ATTRIBUTES dev_attrs;
    WDFDEVICE dev;

    UNREFERENCED_PARAMETER(driver);
    PAGED_CODE();

    WdfDeviceInitSetIoType(dev_init, WdfDeviceIoDirect);

    // pnp
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp_callbacks);
    pnp_callbacks.EvtDevicePrepareHardware = cx_evt_device_prepare_hardware;
    pnp_callbacks.EvtDeviceReleaseHardware = cx_evt_device_release_hardware;
    pnp_callbacks.EvtDeviceD0Entry = cx_evt_device_d0_entry;
    pnp_callbacks.EvtDeviceD0Exit = cx_evt_device_d0_exit;
    WdfDeviceInitSetPnpPowerEventCallbacks(dev_init, &pnp_callbacks);

    // file callbacks
    WDF_FILEOBJECT_CONFIG file_obj_cfg;
    WDF_OBJECT_ATTRIBUTES file_attrs;
    WDF_FILEOBJECT_CONFIG_INIT(&file_obj_cfg, cx_evt_file_create, cx_evt_file_close, cx_evt_file_cleanup);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&file_attrs, FILE_CONTEXT);
    WdfDeviceInitSetFileObjectConfig(dev_init, &file_obj_cfg, &file_attrs);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&dev_attrs, DEVICE_CONTEXT);
    dev_attrs.EvtCleanupCallback = cx_evt_device_cleanup;

    status = WdfDeviceCreate(&dev_init, &dev_attrs, &dev);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfDeviceCreate failed with status %!STATUS!", status);
        return status;
    }

    status = WdfDeviceCreateDeviceInterface(dev, (LPGUID)&GUID_DEVINTERFACE_CXADCWIN, NULL);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfDeviceCreateDeviceInterface failed with status %!STATUS!", status);
        return status;
    }

    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(dev);
    dev_ctx->dev = dev;

    status = cx_check_dev_info(dev_ctx);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "cx_check_dev_info failed with status %!STATUS!", status);
        return status;
    }

    status = cx_read_device_prop(dev_ctx, DevicePropertyBusNumber, &dev_ctx->bus_number);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "cx_read_device_prop(DevicePropertyBusNumber) failed with status %!STATUS!", status);
        return status;
    }
    
    status = cx_read_device_prop(dev_ctx, DevicePropertyAddress, &dev_ctx->dev_addr);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "cx_read_device_prop(DevicePropertyAddress) failed with status %!STATUS!", status);
        return status;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "add device %02x:%02d.%01d PDO(0x%p) FDO(0x%p) ctx(0x%p)",
        dev_ctx->bus_number, (dev_ctx->dev_addr >> 16) & 0x0000FFFF, dev_ctx->dev_addr & 0x0000FFFF,
        WdfDeviceWdmGetPhysicalDevice(dev),
        WdfDeviceWdmGetDeviceObject(dev),
        dev_ctx);

    status = cx_init_device_ctx(dev_ctx);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "cx_init_device_ctx failed with status %!STATUS!", status);
        return status;
    }

    return status;
}

VOID cx_evt_device_cleanup(
    _In_ WDFOBJECT driver_obj
)
{
    UNREFERENCED_PARAMETER(driver_obj);
    PAGED_CODE();
    // nothing to do ??
}

VOID cx_evt_driver_ctx_cleanup(
    _In_ WDFOBJECT driver_obj
)
{
    PAGED_CODE();
    WPP_CLEANUP(driver_obj);
}

NTSTATUS cx_evt_device_prepare_hardware(
    WDFDEVICE dev,
    WDFCMRESLIST res,
    WDFCMRESLIST res_trans
)
{
    NTSTATUS status = STATUS_SUCCESS;
    PAGED_CODE();

    // ensure we stick to arbitrary device count
    if (dev_count >= MAXUCHAR)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "too many devices");
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
            status = cx_init_mmio(dev_ctx, desc);
            break;

        case CmResourceTypeInterrupt:
            status = cx_init_interrupt(dev_ctx, desc, WdfCmResourceListGetDescriptor(res, i));
            break;

        default:
            break;
        }

        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "cx_evt_device_prepare_hardware failed with status %!STATUS!", status);
            return status;
        }
    }

    return status;
}

NTSTATUS cx_init_mmio(
    _In_ PDEVICE_CONTEXT dev_ctx,
    _In_ PCM_PARTIAL_RESOURCE_DESCRIPTOR desc
)
{
    PAGED_CODE();

    dev_ctx->mmio = (PULONG)MmMapIoSpaceEx(
        desc->u.Memory.Start,
        desc->u.Memory.Length,
        PAGE_NOCACHE | PAGE_READWRITE);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "mmio created %I64X->%I64X (%08X)",
        desc->u.Memory.Start.QuadPart,
        desc->u.Memory.Start.QuadPart + desc->u.Memory.Length,
        desc->u.Memory.Length);

    if (!dev_ctx->mmio)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "MmMapIoSpaceEx failed to create %I64X->%I64X (%08X)",
            desc->u.Memory.Start.QuadPart,
            desc->u.Memory.Start.QuadPart + desc->u.Memory.Length,
            desc->u.Memory.Length);

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    dev_ctx->mmio_len = desc->u.Memory.Length;
    dev_ctx->user_mdl = IoAllocateMdl((PVOID)dev_ctx->mmio, dev_ctx->mmio_len, FALSE, FALSE, NULL);

    if (!dev_ctx->user_mdl)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "IoAllocateMdl failed");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    MmBuildMdlForNonPagedPool(dev_ctx->user_mdl);

    return STATUS_SUCCESS;
}

NTSTATUS cx_init_interrupt(
    _In_ PDEVICE_CONTEXT dev_ctx,
    _In_ PCM_PARTIAL_RESOURCE_DESCRIPTOR desc,
    _In_ PCM_PARTIAL_RESOURCE_DESCRIPTOR desc_raw
)
{
    PAGED_CODE();

    NTSTATUS status;
    WDF_INTERRUPT_CONFIG intr_cfg;

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "intrs lvl 0x%08X vec 0x%08X",
        desc->u.Interrupt.Level,
        desc->u.Interrupt.Vector);

    WDF_INTERRUPT_CONFIG_INIT(&intr_cfg, cx_evt_isr, cx_evt_dpc);
    intr_cfg.InterruptTranslated = desc;
    intr_cfg.InterruptRaw = desc_raw;
    intr_cfg.EvtInterruptEnable = cx_evt_intr_enable;
    intr_cfg.EvtInterruptDisable = cx_evt_intr_disable;
    status = WdfInterruptCreate(dev_ctx->dev, &intr_cfg, WDF_NO_OBJECT_ATTRIBUTES, &dev_ctx->intr);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfInterruptCreate failed with status %!STATUS!", status);
    }

    return status;
}

NTSTATUS cx_evt_device_release_hardware(
    _In_ WDFDEVICE dev,
    _In_ WDFCMRESLIST res_trans
)
{
    NTSTATUS status = STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(res_trans);
    PAGED_CODE();

    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(dev);

    if (dev_ctx->mmio)
    {
        MmUnmapIoSpace(dev_ctx->mmio, dev_ctx->mmio_len);
        dev_ctx->mmio = NULL;
    }

    return status;
}

NTSTATUS cx_evt_device_d0_entry(
    _In_ WDFDEVICE dev,
    _In_ WDF_POWER_DEVICE_STATE prev_state
)
{
    NTSTATUS status = STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(prev_state);
    PAGED_CODE();

    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(dev);

    cx_init(dev_ctx);

    return status;
}

NTSTATUS cx_evt_device_d0_exit(
    _In_ WDFDEVICE dev,
    _In_ WDF_POWER_DEVICE_STATE target_state
)
{
    NTSTATUS status = STATUS_SUCCESS;

    PAGED_CODE();
    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(dev);

    // should already be stopped
    cx_stop_capture(dev_ctx);
    cx_disable(dev_ctx);

    switch (target_state)
    {
    case WdfPowerDeviceD1:
    case WdfPowerDeviceD2:
    case WdfPowerDeviceD3:
        break;

    case WdfPowerDevicePrepareForHibernation:
        break;

    case WdfPowerDeviceD3Final:
        cx_reset(dev_ctx);
        break;
    }

    return status;
}

NTSTATUS cx_init_device_ctx(
    _Inout_ PDEVICE_CONTEXT dev_ctx
)
{
    NTSTATUS status;

    PAGED_CODE();
    
    // device is 32-bit aligned
    WdfDeviceSetAlignmentRequirement(dev_ctx->dev, FILE_LONG_ALIGNMENT);

    dev_ctx->dev_idx = dev_count++;
    dev_ctx->state.ouflow_count = 0;

    // create symlink
    DECLARE_UNICODE_STRING_SIZE(symlink_path, sizeof(SYMLINK_PATH) + 3);
    RtlUnicodeStringPrintf(&symlink_path, L"%ws%d", SYMLINK_PATH, dev_ctx->dev_idx);
    dev_ctx->symlink_path = symlink_path;

    status = WdfDeviceCreateSymbolicLink(dev_ctx->dev, &dev_ctx->symlink_path);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL,
            "WdfDeviceCreateSymbolicLink failed with status %!STATUS!", status);
        return status;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "created symlink %wZ", &dev_ctx->symlink_path);

    // init dma
    status = cx_init_dma(dev_ctx);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "cx_init_dma failed with status %!STATUS!", status);
        return status;
    }

    // init queue
    status = cx_init_queue(dev_ctx);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "cx_init_queue failed with status %!STATUS!", status);
        return status;
    }

    // init attrs
    cx_init_attrs(dev_ctx);

    // init state
    cx_init_state(dev_ctx);

    // init interrupt event
    KeInitializeEvent(&dev_ctx->isr_event, NotificationEvent, FALSE);

    return status;
}

NTSTATUS cx_init_dma(
    _In_ PDEVICE_CONTEXT dev_ctx
)
{
    NTSTATUS status = STATUS_SUCCESS;
    PAGED_CODE();

    WDF_DMA_ENABLER_CONFIG dma_cfg;

    WDF_DMA_ENABLER_CONFIG_INIT(&dma_cfg, WdfDmaProfilePacket, CX_VBI_BUF_SIZE);
    dma_cfg.WdmDmaVersionOverride = 3;

    status = WdfDmaEnablerCreate(dev_ctx->dev, &dma_cfg, WDF_NO_OBJECT_ATTRIBUTES, &dev_ctx->dma_enabler);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfDmaEnablerCreate failed with status %!STATUS!", status);
        return status;
    }

    // risc instructions
    dev_ctx->dma_risc_instr.len = CX_RISC_INSTR_BUF_SIZE;
    status = WdfCommonBufferCreate(dev_ctx->dma_enabler, dev_ctx->dma_risc_instr.len, WDF_NO_OBJECT_ATTRIBUTES, &dev_ctx->dma_risc_instr.buf);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfCommonBufferCreate failed with status %!STATUS!", status);
        return status;
    }

    dev_ctx->dma_risc_instr.va = WdfCommonBufferGetAlignedVirtualAddress(dev_ctx->dma_risc_instr.buf);
    dev_ctx->dma_risc_instr.la = WdfCommonBufferGetAlignedLogicalAddress(dev_ctx->dma_risc_instr.buf);

    RtlZeroMemory(dev_ctx->dma_risc_instr.va, dev_ctx->dma_risc_instr.len);

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "created risc instr dma 0x%p, (%I64X) (%u kbytes)",
        dev_ctx->dma_risc_instr.va,
        dev_ctx->dma_risc_instr.la.QuadPart,
        (ULONG)(WdfCommonBufferGetLength(dev_ctx->dma_risc_instr.buf) / 1024));

    // data pages
    for (ULONG i = 0; i < CX_VBI_BUF_COUNT; i++)
    {
        DMA_DATA dma_data;
        dma_data.len = PAGE_SIZE;
        status = WdfCommonBufferCreate(dev_ctx->dma_enabler, dma_data.len, WDF_NO_OBJECT_ATTRIBUTES, &dma_data.buf);

        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfCommonBufferCreate failed with status %!STATUS!", status);
            return status;
        }

        dma_data.va = WdfCommonBufferGetAlignedVirtualAddress(dma_data.buf);
        dma_data.la = WdfCommonBufferGetAlignedLogicalAddress(dma_data.buf);

        RtlZeroMemory(dma_data.va, dma_data.len);
        dev_ctx->dma_risc_page[i] = dma_data;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "created %d data pages", CX_VBI_BUF_COUNT);
    return status;
}

NTSTATUS cx_init_queue(
    _In_ PDEVICE_CONTEXT dev_ctx
)
{
    NTSTATUS status = STATUS_SUCCESS;
    PAGED_CODE();

    WDF_IO_QUEUE_CONFIG queue_cfg;

    // control queue
    WDF_IO_QUEUE_CONFIG_INIT(&queue_cfg, WdfIoQueueDispatchSequential);
    queue_cfg.EvtIoDeviceControl = cx_evt_io_ctrl;
    status = WdfIoQueueCreate(dev_ctx->dev, &queue_cfg, WDF_NO_OBJECT_ATTRIBUTES, &dev_ctx->control_queue);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfIoQueueCreate (control) failed with status %!STATUS!", status);
        return status;
    }

    status = WdfDeviceConfigureRequestDispatching(dev_ctx->dev, dev_ctx->control_queue, WdfRequestTypeDeviceControl);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL,
            "WdfDeviceConfigureRequestDispatching (WdfRequestTypeDeviceControl) failed with status %!STATUS!", status);
        return status;
    }

    // read queue
    WDF_IO_QUEUE_CONFIG_INIT(&queue_cfg, WdfIoQueueDispatchSequential);
    queue_cfg.EvtIoRead = cx_evt_io_read;
    status = WdfIoQueueCreate(dev_ctx->dev, &queue_cfg, WDF_NO_OBJECT_ATTRIBUTES, &dev_ctx->read_queue);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfIoQueueCreate (read) failed with status %!STATUS!", status);
        return status;
    }

    status = WdfDeviceConfigureRequestDispatching(dev_ctx->dev, dev_ctx->read_queue, WdfRequestTypeRead);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL,
            "WdfDeviceConfigureRequestDispatching (WdfRequestTypeRead) failed with status %!STATUS!", status);
        return status;
    }

    return status;
}

_inline
VOID cx_init_attrs(
    _Inout_ PDEVICE_CONTEXT dev_ctx
)
{
    dev_ctx->attrs = (DEVICE_ATTRS) {
        .vmux = CX_IOCTL_VMUX_DEFAULT,
        .level = CX_IOCTL_LEVEL_DEFAULT,
        .tenbit = CX_IOCTL_TENBIT_DEFAULT,
        .sixdb = CX_IOCTL_SIXDB_DEFAULT,
        .center_offset = CX_IOCTL_CENTER_OFFSET_DEFAULT
    };
}

_inline
VOID cx_init_state(
    _Inout_ PDEVICE_CONTEXT dev_ctx
)
{
    dev_ctx->state = (DEVICE_STATE) { 0 };
}

NTSTATUS cx_check_dev_info(
    _In_ PDEVICE_CONTEXT dev_ctx
)
{
    NTSTATUS status = STATUS_SUCCESS;
    PAGED_CODE();

    BUS_INTERFACE_STANDARD bus;

    status = WdfFdoQueryForInterface(
        dev_ctx->dev,
        &GUID_BUS_INTERFACE_STANDARD,
        (PINTERFACE)&bus,
        sizeof(BUS_INTERFACE_STANDARD),
        1,
        NULL);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfFdoQueryForInterface failed with status %!STATUS!", status);
        return status;
    }

    UCHAR buf[256];
    RtlZeroMemory(buf, sizeof(buf));
    bus.GetBusData(bus.Context, PCI_WHICHSPACE_CONFIG, buf, FIELD_OFFSET(PCI_COMMON_CONFIG, VendorID), 256);

    // ensure device id is correct
    PPCI_COMMON_CONFIG pci_config = (PPCI_COMMON_CONFIG)buf;

    if (pci_config->VendorID != VENDOR_ID || pci_config->DeviceID != DEVICE_ID)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "unknown vendor/device id %04X:%04X",
            pci_config->VendorID,
            pci_config->DeviceID);

        return STATUS_UNSUCCESSFUL;
    }

    return status;
}

NTSTATUS cx_read_device_prop(
    _In_ PDEVICE_CONTEXT dev_ctx,
    _In_ DEVICE_REGISTRY_PROPERTY prop,
    _Inout_ PULONG value
)
{
    NTSTATUS status = STATUS_SUCCESS;
    PAGED_CODE();

    ULONG len;

    status = WdfDeviceQueryProperty(
        dev_ctx->dev,
        prop,
        sizeof(ULONG),
        value,
        &len
    );

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL,
            "WdfDeviceQueryProperty failed with status %!STATUS!", status);
        return status;
    }

    return status;
}
