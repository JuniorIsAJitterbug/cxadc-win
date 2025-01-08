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
#include "ioctl.tmh"

#include "ioctl.h"
#include "cx2388x.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, cx_evt_file_create)
#pragma alloc_text (PAGE, cx_evt_file_close)
#pragma alloc_text (PAGE, cx_evt_file_cleanup)
#endif

VOID
cx_evt_file_create(
    _In_ WDFDEVICE dev,
    _In_ WDFREQUEST req,
    _In_ WDFFILEOBJECT file_obj)
{
    UNREFERENCED_PARAMETER(dev);

    NTSTATUS status = STATUS_SUCCESS;
    PAGED_CODE();

    PFILE_CONTEXT file_ctx = cx_file_get_ctx(file_obj);
    file_ctx->read_offset = 0;
    file_ctx->mmap_data = (MMAP_DATA){ 0 };

    WdfRequestComplete(req, status);
}

VOID
cx_evt_file_close(
    _In_ WDFFILEOBJECT file_obj
)
{
    PAGED_CODE();

    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(WdfFileObjectGetDevice(file_obj));
    PFILE_CONTEXT file_ctx = cx_file_get_ctx(file_obj);

    if (file_ctx->read_offset)
    {
        InterlockedDecrement(&dev_ctx->state.reader_count);

        // stop capture if no other readers
        if (!dev_ctx->state.reader_count)
        {
            cx_stop_capture(dev_ctx);
        }
    }
}

VOID
cx_evt_file_cleanup(
    _In_ WDFFILEOBJECT file_obj
)
{
    PAGED_CODE();

    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(WdfFileObjectGetDevice(file_obj));
    PFILE_CONTEXT file_ctx = cx_file_get_ctx(file_obj);

    if (file_ctx->mmap_data.ptr != NULL)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "munmap addr %p", file_ctx->mmap_data.ptr);

        MmUnmapLockedPages(file_ctx->mmap_data.ptr, dev_ctx->user_mdl);
        file_ctx->mmap_data.ptr = NULL;
    }
}

VOID cx_evt_io_ctrl(
    _In_ WDFQUEUE queue,
    _In_ WDFREQUEST req,
    _In_ size_t out_len,
    _In_ size_t in_len,
    _In_ ULONG ctrl_code
)
{
    NTSTATUS status = STATUS_SUCCESS;
    PUCHAR out_buf = NULL, in_buf = NULL;
    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(WdfIoQueueGetDevice(queue));
    PFILE_CONTEXT file_ctx = cx_file_get_ctx(WdfRequestGetFileObject(req));

    if (out_len)
    {
        status = WdfRequestRetrieveOutputBuffer(req, out_len, &out_buf, NULL);

        if (!NT_SUCCESS(status) || out_buf == NULL)
        {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfRequestRetrieveOutputBuffer failed with status %!STATUS!", status);
            WdfRequestComplete(req, status);
            return;
        }
    }

    if (in_len)
    {
        status = WdfRequestRetrieveInputBuffer(req, in_len, &in_buf, NULL);

        if (!NT_SUCCESS(status) || in_buf == NULL)
        {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfRequestRetrieveInputBuffer failed with status %!STATUS!", status);
            WdfRequestComplete(req, status);
            return;
        }
    }

    switch (ctrl_code)
    {
    case CX_IOCTL_GET_CAPTURE_STATE:
    {
        if (out_buf == NULL || out_len < sizeof(ULONG))
        {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        *(PULONG)out_buf = dev_ctx->state.is_capturing;
        break;
    }

    case CX_IOCTL_GET_OUFLOW_COUNT:
    {
        if (out_buf == NULL || out_len < sizeof(ULONG))
        {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        *(PULONG)out_buf = dev_ctx->state.ouflow_count;
        break;
    }

    case CX_IOCTL_GET_VMUX:
    {
        if (out_buf == NULL || out_len < sizeof(ULONG))
        {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        *(PULONG)out_buf = dev_ctx->attrs.vmux;
        break;
    }

    case CX_IOCTL_GET_LEVEL:
    {
        if (out_buf == NULL || out_len < sizeof(ULONG))
        {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        *(PULONG)out_buf = dev_ctx->attrs.level;
        break;
    }

    case CX_IOCTL_GET_TENBIT:
    {
        if (out_buf == NULL || out_len < sizeof(ULONG))
        {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        *(PULONG)out_buf = dev_ctx->attrs.tenbit;
        break;
    }

    case CX_IOCTL_GET_SIXDB:
    {
        if (out_buf == NULL || out_len < sizeof(ULONG))
        {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        *(PULONG)out_buf = dev_ctx->attrs.sixdb;
        break;
    }

    case CX_IOCTL_GET_CENTER_OFFSET:
    {
        if (out_buf == NULL || out_len < sizeof(ULONG))
        {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        *(PULONG)out_buf = dev_ctx->attrs.center_offset;
        break;
    }

    case CX_IOCTL_GET_BUS_NUMBER:
    {
        if (out_buf == NULL || out_len < sizeof(ULONG))
        {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        *(PULONG)out_buf = dev_ctx->bus_number;
        break;
    }

    case CX_IOCTL_GET_DEVICE_ADDRESS:
    {
        if (out_buf == NULL || out_len < sizeof(ULONG))
        {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        *(PULONG)out_buf = dev_ctx->dev_addr;
        break;
    }

    case CX_IOCTL_GET_REGISTER:
    {
        if (out_buf == NULL || in_buf == NULL || in_len < sizeof(ULONG) || out_len != sizeof(ULONG))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "invalid data for get register %lld / %lld", in_len, out_len);
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        ULONG address = *(PULONG)in_buf;

        if (address < CX_REGISTER_BASE || address > CX_REGISTER_END)
        {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "address %08X out of range", address);
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        *(PULONG)out_buf = cx_read(dev_ctx, address);
        break;
    }

    case CX_IOCTL_RESET_OUFLOW_COUNT:
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "resetting over/underflow count (current: %d)", dev_ctx->state.ouflow_count);
        dev_ctx->state.ouflow_count = 0;
        break;
    }

    case CX_IOCTL_SET_VMUX:
    {
        if (in_buf == NULL || in_len != sizeof(LONG))
        {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        LONG value = *(PLONG)in_buf;

        if (value < CX_IOCTL_VMUX_MIN || value > CX_IOCTL_VMUX_MAX)
        {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "invalid vmux %d", value);
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "setting vmux to %d", value);
        dev_ctx->attrs.vmux = value;
        cx_set_vmux(dev_ctx);
        break;
    }

    case CX_IOCTL_SET_LEVEL:
    {
        if (in_buf == NULL || in_len != sizeof(LONG))
        {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        LONG value = *(PLONG)in_buf;

        if (value < CX_IOCTL_LEVEL_MIN || value > CX_IOCTL_LEVEL_MAX)
        {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "invalid level %d", value);
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "setting level to %d", value);
        dev_ctx->attrs.level = value;
        cx_set_level(dev_ctx);
        break;
    }

    case CX_IOCTL_SET_TENBIT:
    {
        if (in_buf == NULL || in_len != sizeof(LONG))
        {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        LONG value = *(PLONG)in_buf;

        if (value < CX_IOCTL_TENBIT_MIN || value > CX_IOCTL_TENBIT_MAX)
        {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "invalid tenbit %d", value);
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "setting tenbit to %d", value);
        dev_ctx->attrs.tenbit = value;
        cx_set_tenbit(dev_ctx);
        break;
    }

    case CX_IOCTL_SET_SIXDB:
    {
        if (in_buf == NULL || in_len != sizeof(LONG))
        {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        LONG value = *(PLONG)in_buf;

        if (value < CX_IOCTL_SIXDB_MIN || value > CX_IOCTL_SIXDB_MAX)
        {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "invalid sixdb %d", value);
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "setting sixdb to %d", value);
        dev_ctx->attrs.sixdb = value;
        cx_set_level(dev_ctx);
        break;
    }

    case CX_IOCTL_SET_CENTER_OFFSET:
    {
        if (in_buf == NULL || in_len != sizeof(LONG))
        {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        LONG value = *(PLONG)in_buf;

        if (value < CX_IOCTL_CENTER_OFFSET_MIN || value > CX_IOCTL_CENTER_OFFSET_MAX)
        {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "invalid center_offset %d", value);
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "setting center_center to %d", value);
        dev_ctx->attrs.center_offset = value;
        cx_set_center_offset(dev_ctx);
        break;
    }

    case CX_IOCTL_SET_REGISTER:
    {
        if (in_buf == NULL || in_len != sizeof(SET_REGISTER_DATA))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "invalid data for set register %lld", in_len);
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        SET_REGISTER_DATA data = *(PSET_REGISTER_DATA)in_buf;

        if (data.addr < CX_REGISTER_BASE || data.addr  > CX_REGISTER_END)
        {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "address %08X out of range", data.addr);
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "writing %08X to %08X", data.val, data.addr);
        cx_write(dev_ctx, data.addr, data.val);
        break;
    }

    case CX_IOCTL_MMAP:
    {
        if (out_buf == NULL || out_len != sizeof(MMAP_DATA))
        {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        if (file_ctx->mmap_data.ptr == NULL)
        {
            file_ctx->mmap_data = (MMAP_DATA)
            {
                .ptr = MmMapLockedPagesSpecifyCache(dev_ctx->user_mdl, UserMode, MmNonCached, NULL, FALSE, NormalPagePriority)
            };
        }

        TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "mmap addr %p", file_ctx->mmap_data.ptr);

        *(PMMAP_DATA)out_buf = file_ctx->mmap_data;
        break;
    }

    case CX_IOCTL_MUNMAP:
    {
        if (file_ctx->mmap_data.ptr != NULL)
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "munmap addr %p", file_ctx->mmap_data.ptr);

            MmUnmapLockedPages(file_ctx->mmap_data.ptr, dev_ctx->user_mdl);
            file_ctx->mmap_data.ptr = NULL;
        }

        break;
    }

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    WdfRequestSetInformation(req, (ULONG_PTR)out_len);
    WdfRequestComplete(req, status);
}

VOID cx_evt_io_read(
    _In_ WDFQUEUE queue,
    _In_ WDFREQUEST req,
    _In_ size_t req_len
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

        InterlockedExchange(&dev_ctx->state.initial_page, dev_ctx->state.last_gp_cnt);
    }

    // new reader, increment count
    if (!file_ctx->read_offset)
    {
        InterlockedIncrement(&dev_ctx->state.reader_count);
    }

    WDFMEMORY mem;
    status = WdfRequestRetrieveOutputMemory(req, &mem);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfRequestRetrieveOutputMemory failed with status %!STATUS!", status);
        WdfRequestComplete(req, STATUS_UNSUCCESSFUL);
        return;
    }

    LONG64 count = req_len;
    LONG64 offset = file_ctx->read_offset;
    LONG64 tgt_off = 0;
    LONG page_no = cx_get_page_no(dev_ctx->state.initial_page, offset);

    while (count && dev_ctx->state.is_capturing)
    {
        while (count > 0 && page_no != dev_ctx->state.last_gp_cnt)
        {
            LONG64 page_off = offset % PAGE_SIZE;
            LONG64 len = page_off ? (PAGE_SIZE - page_off) : PAGE_SIZE;

            if (len > count) {
                len = count;
            }

            WdfMemoryCopyFromBuffer(mem, tgt_off, &dev_ctx->dma_risc_page[page_no].va[page_off], len);

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
        if (cx_get_ouflow_state(dev_ctx))
        {
            dev_ctx->state.ouflow_count += 1;
            cx_reset_ouflow_state(dev_ctx);
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
    InterlockedExchange64(&file_ctx->read_offset, offset);

    WdfRequestCompleteWithInformation(req, status, req_len - count);
}

__inline
ULONG cx_get_page_no(
    _In_ ULONG initial_page,
    _In_ size_t off
)
{
    return (((off % CX_VBI_BUF_SIZE) / PAGE_SIZE) + initial_page) % CX_VBI_BUF_COUNT;
}
