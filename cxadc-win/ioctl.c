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
#include "ioctl.tmh"
#include "ioctl.h"

#include "control.h"
#include "cx2388x.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, cx_evt_file_create)
#pragma alloc_text (PAGE, cx_evt_file_close)
#pragma alloc_text (PAGE, cx_evt_file_cleanup)
#endif

_Use_decl_annotations_
VOID cx_evt_file_create(
    WDFDEVICE dev,
    WDFREQUEST req,
    WDFFILEOBJECT file_obj)
{
    UNREFERENCED_PARAMETER(dev);

    NTSTATUS status = STATUS_SUCCESS;
    PAGED_CODE();

    PFILE_CONTEXT file_ctx = cx_file_get_ctx(file_obj);

    *file_ctx = (FILE_CONTEXT){
        .read_offset = 0,
        .ptr = NULL
    };

    WdfRequestComplete(req, status);
}

_Use_decl_annotations_
VOID cx_evt_file_close(
    WDFFILEOBJECT file_obj
)
{
    PAGED_CODE();

    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(WdfFileObjectGetDevice(file_obj));
    PFILE_CONTEXT file_ctx = cx_file_get_ctx(file_obj);

    if (file_ctx->read_offset)
    {
        InterlockedDecrementSizeT(&dev_ctx->state.reader_count);

        // stop capture if no other readers
        if (!dev_ctx->state.reader_count)
        {
            cx_stop_capture(dev_ctx);
        }
    }
}

_Use_decl_annotations_
VOID cx_evt_file_cleanup(
    WDFFILEOBJECT file_obj
)
{
    PAGED_CODE();

    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(WdfFileObjectGetDevice(file_obj));
    PFILE_CONTEXT file_ctx = cx_file_get_ctx(file_obj);

    if (file_ctx->ptr != NULL)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "munmap addr %p", file_ctx->ptr);

        MmUnmapLockedPages(file_ctx->ptr, dev_ctx->user_mdl);
        file_ctx->ptr = NULL;
    }
}

_Use_decl_annotations_
NTSTATUS cx_evt_set_output(WDFREQUEST req, PVOID buf, size_t buf_len)
{
    WDFMEMORY mem = WDF_NO_HANDLE;
    NTSTATUS status = WdfRequestRetrieveOutputMemory(req, &mem);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfRequestRetrieveOutputMemory failed with status %!STATUS!", status);
        return status;
    }

    status = WdfMemoryCopyFromBuffer(mem, 0, buf, buf_len);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfMemoryCopyFromBuffer failed with status %!STATUS!", status);
        return status;
    }

    WdfRequestSetInformation(req, buf_len);
    return status;
}

_Use_decl_annotations_
NTSTATUS cx_evt_get_input(WDFREQUEST req, PVOID buf, size_t buf_len)
{
    WDFMEMORY mem = WDF_NO_HANDLE;
    NTSTATUS status = WdfRequestRetrieveInputMemory(req, &mem);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfRequestRetrieveInputMemory failed with status %!STATUS!", status);
        return status;
    }

    status = WdfMemoryCopyToBuffer(mem, 0, buf, buf_len);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfMemoryCopyToBuffer failed with status %!STATUS!", status);
        return status;
    }

    WdfRequestSetInformation(req, buf_len);
    return status;
}

_Use_decl_annotations_
VOID cx_evt_io_ctrl(
    WDFQUEUE queue,
    WDFREQUEST req,
    size_t out_len,
    size_t in_len,
    ULONG ctrl_code
)
{
    UNREFERENCED_PARAMETER(out_len);
    UNREFERENCED_PARAMETER(in_len);

    NTSTATUS status = STATUS_SUCCESS;
    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(WdfIoQueueGetDevice(queue));

    switch (ctrl_code)
    {
        case CX_IOCTL_GET_CONFIG:
            status = cx_evt_set_output(req, &dev_ctx->config, sizeof(DEVICE_CONFIG));
            break;

        case CX_IOCTL_GET_STATE:
            status = cx_evt_set_output(req, &dev_ctx->state, sizeof(DEVICE_STATE));
            break;

        case CX_IOCTL_GET_CAPTURE_STATE:
            status = cx_evt_set_output(req, &dev_ctx->state.is_capturing, sizeof(BOOLEAN));
            break;

        case CX_IOCTL_GET_OUFLOW_COUNT:
            status = cx_evt_set_output(req, &dev_ctx->state.ouflow_count, sizeof(ULONG));
            break;

        case CX_IOCTL_GET_VMUX:
            status = cx_evt_set_output(req, &dev_ctx->config.vmux, sizeof(ULONG));
            break;

        case CX_IOCTL_GET_LEVEL:
            status = cx_evt_set_output(req, &dev_ctx->config.level, sizeof(ULONG));
            break;

        case CX_IOCTL_GET_TENBIT:
            status = cx_evt_set_output(req, &dev_ctx->config.tenbit, sizeof(BOOLEAN));
            break;

        case CX_IOCTL_GET_SIXDB:
            status = cx_evt_set_output(req, &dev_ctx->config.sixdb, sizeof(BOOLEAN));
            break;

        case CX_IOCTL_GET_CENTER_OFFSET:
            status = cx_evt_set_output(req, &dev_ctx->config.center_offset, sizeof(ULONG));
            break;

        case CX_IOCTL_GET_BUS_NUMBER:
            status = cx_evt_set_output(req, &dev_ctx->bus_number, sizeof(ULONG));
            break;

        case CX_IOCTL_GET_DEVICE_ADDRESS:
            status = cx_evt_set_output(req, &dev_ctx->dev_addr, sizeof(ULONG));
            break;

        case CX_IOCTL_GET_WIN32_PATH:
        {
            DECLARE_UNICODE_STRING_SIZE(symlink_path, 128);
            status = RtlUnicodeStringPrintf(&symlink_path, L"%ws%d", WIN32_PATH, dev_ctx->dev_idx);

            if (!NT_SUCCESS(status))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "RtlUnicodeStringPrintf failed with status %!STATUS!", status);
                break;
            };

            status = cx_evt_set_output(req, symlink_path.Buffer, symlink_path.Length);
            break;
        }

        case CX_IOCTL_GET_REGISTER:
        {
            ULONG value = 0;
            status = cx_evt_get_input(req, &value, sizeof(ULONG));

            if (!NT_SUCCESS(status))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "cx_evt_copy_from failed with status %!STATUS!", status);
                break;
            }

            if (value < CX_REGISTER_BASE || value > CX_REGISTER_END)
            {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "address %08X out of range", value);
                status = STATUS_INVALID_PARAMETER;
                break;
            }

            ULONG reg_val = cx_read(&dev_ctx->mmio, value);
            status = cx_evt_set_output(req, &reg_val, sizeof(ULONG));
            break;
        }

        case CX_IOCTL_RESET_OUFLOW_COUNT:
            cx_ctrl_reset_ouflow_count(dev_ctx);
            break;

        case CX_IOCTL_SET_VMUX:
        {
            ULONG value = 0;
            status = cx_evt_get_input(req, &value, sizeof(ULONG));

            if (!NT_SUCCESS(status))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "cx_evt_copy_from failed with status %!STATUS!", status);
                break;
            }

            status = cx_ctrl_set_vmux(dev_ctx, value);
            break;
        }

        case CX_IOCTL_SET_LEVEL:
        {
            ULONG value = 0;
            status = cx_evt_get_input(req, &value, sizeof(ULONG));

            if (!NT_SUCCESS(status))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "cx_evt_copy_from failed with status %!STATUS!", status);
                break;
            }

            status = cx_ctrl_set_level(dev_ctx, value);
            break;
        }

        case CX_IOCTL_SET_TENBIT:
        {
            ULONG value = 0;
            status = cx_evt_get_input(req, &value, sizeof(ULONG));

            if (!NT_SUCCESS(status))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "cx_evt_copy_from failed with status %!STATUS!", status);
                break;
            }

            status = cx_ctrl_set_tenbit(dev_ctx, value ? TRUE : FALSE);
            break;
        }

        case CX_IOCTL_SET_SIXDB:
        {
            ULONG value = 0;
            status = cx_evt_get_input(req, &value, sizeof(ULONG));

            if (!NT_SUCCESS(status))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "cx_evt_copy_from failed with status %!STATUS!", status);
                break;
            }

            status = cx_ctrl_set_sixdb(dev_ctx, value ? TRUE : FALSE);
            break;
        }

        case CX_IOCTL_SET_CENTER_OFFSET:
        {
            ULONG value = 0;
            status = cx_evt_get_input(req, &value, sizeof(ULONG));

            if (!NT_SUCCESS(status))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "cx_evt_copy_from failed with status %!STATUS!", status);
                break;
            }

            status = cx_ctrl_set_center_offset(dev_ctx, value);
            break;
        }

        case CX_IOCTL_SET_REGISTER:
        {
            SET_REGISTER_DATA data = { 0 };
            status = cx_evt_get_input(req, &data, sizeof(SET_REGISTER_DATA));

            if (!NT_SUCCESS(status))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "cx_evt_copy_from failed with status %!STATUS!", status);
                break;
            }

            if (data.addr < CX_REGISTER_BASE || data.addr  > CX_REGISTER_END)
            {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "address %08X out of range", data.addr);
                status = STATUS_INVALID_PARAMETER;
                break;
            }

            TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "writing %08X to %08X", data.val, data.addr);
            cx_write(&dev_ctx->mmio, data.addr, data.val);
            break;
        }

        case CX_IOCTL_MMAP:
        {
            PFILE_CONTEXT file_ctx = cx_file_get_ctx(WdfRequestGetFileObject(req));

            if (file_ctx->ptr == NULL)
            {
                file_ctx->ptr = MmMapLockedPagesSpecifyCache(dev_ctx->user_mdl, UserMode, MmNonCached, NULL, FALSE, NormalPagePriority);
            }

            TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "mmap addr %p", file_ctx->ptr);

            status = cx_evt_set_output(req, &file_ctx->ptr, sizeof(UINT_PTR));
            break;
        }

        case CX_IOCTL_MUNMAP:
        {
            PFILE_CONTEXT file_ctx = cx_file_get_ctx(WdfRequestGetFileObject(req));

            if (file_ctx->ptr != NULL)
            {
                TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "munmap addr %p", file_ctx->ptr);

                MmUnmapLockedPages(file_ctx->ptr, dev_ctx->user_mdl);
                file_ctx->ptr = NULL;
            }

            break;
        }

        default:
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }

    WdfRequestComplete(req, status);
}

_Use_decl_annotations_
VOID cx_evt_io_read(
    WDFQUEUE queue,
    WDFREQUEST req,
    size_t req_len
)
{
    NTSTATUS status = STATUS_SUCCESS;
    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(WdfIoQueueGetDevice(queue));
    PFILE_CONTEXT file_ctx = cx_file_get_ctx(WdfRequestGetFileObject(req));

    // start capture if idle
    if (!dev_ctx->state.is_capturing)
    {
        KeClearEvent(&dev_ctx->isr_event);

        cx_start_capture(dev_ctx);

        status = KeWaitForSingleObject(&dev_ctx->isr_event, Executive, KernelMode, FALSE, NULL);

        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "KeWaitForSingleObject failed with status %!STATUS!", status);
        }

        InterlockedExchange((PLONG)&dev_ctx->state.initial_page, dev_ctx->state.last_gp_cnt);
    }

    // new reader, increment count
    if (!file_ctx->read_offset)
    {
        InterlockedIncrement((PLONG)&dev_ctx->state.reader_count);
    }

    WDFMEMORY mem = WDF_NO_HANDLE;
    status = WdfRequestRetrieveOutputMemory(req, &mem);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfRequestRetrieveOutputMemory failed with status %!STATUS!", status);
        WdfRequestComplete(req, STATUS_UNSUCCESSFUL);
        return;
    }

    size_t count = req_len;
    size_t offset = file_ctx->read_offset;
    size_t tgt_off = 0;
    ULONG page_no = cx_get_page_no(dev_ctx->state.initial_page, offset);

    while (count && dev_ctx->state.is_capturing)
    {
        while (count > 0 && page_no != dev_ctx->state.last_gp_cnt)
        {
            size_t page_off = offset % PAGE_SIZE;
            size_t len = page_off ? (PAGE_SIZE - page_off) : PAGE_SIZE;

            if (len > count) {
                len = count;
            }

            WdfMemoryCopyFromBuffer(mem, tgt_off, &dev_ctx->risc.page[page_no].va[page_off], len);

            if (!NT_SUCCESS(status))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfMemoryCopyFromBuffer failed with status %!STATUS!", status);
                WdfRequestComplete(req, STATUS_UNSUCCESSFUL);
                return;
            }

            count -= len;
            tgt_off += len;
            offset += len;

            page_no = cx_get_page_no(dev_ctx->state.initial_page, offset);
        }

        // check over/underflow, increment count if set
        if (cx_get_ouflow_state(&dev_ctx->mmio))
        {
            InterlockedIncrement((PLONG)&dev_ctx->state.ouflow_count);
            cx_reset_ouflow_state(&dev_ctx->mmio);
        }

        if (count)
        {
            KeClearEvent(&dev_ctx->isr_event);

            // gp_cnt == page_no but read buffer is not filled
            // wait for interrupt to trigger and continue
            status = KeWaitForSingleObject(&dev_ctx->isr_event, Executive, KernelMode, FALSE, NULL);

            if (!NT_SUCCESS(status))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "KeWaitForSingleObject failed with status %!STATUS!", status);
                WdfRequestComplete(req, STATUS_UNSUCCESSFUL);
                return;
            }
        }
    }

    // our read request does not contain an offset,
    // so we keep track of it for the duration of the capture
    InterlockedExchange64((PLONG64)&file_ctx->read_offset, offset);

    WdfRequestCompleteWithInformation(req, status, req_len - count);
}
