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
#include "ioctl.tmh"

#include "ioctl.h"
#include "cx2388x.h"

VOID cx_evt_io_ctrl(
    _In_ WDFQUEUE queue,
    _In_ WDFREQUEST req,
    _In_ size_t out_len,
    _In_ size_t in_len,
    _In_ ULONG ctrl_code)
{
    NTSTATUS status = STATUS_SUCCESS;
    PULONG out_buf, in_buf;
    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(WdfIoQueueGetDevice(queue));

    switch (ctrl_code)
    {
    case CX_IOCTL_SET_VMUX:
        status = WdfRequestRetrieveInputBuffer(req, in_len, &in_buf, NULL);

        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfRequestRetrieveInputBuffer (vmux) failed with status %!STATUS!", status);
            break;
        }

        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "setting vmux to %d", *in_buf);
        dev_ctx->attrs.vmux = *in_buf;
        cx_set_vmux(dev_ctx);
        break;

    case CX_IOCTL_GET_VMUX:
        status = WdfRequestRetrieveOutputBuffer(req, out_len, &out_buf, NULL);

        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfRequestRetrieveOutputBuffer (vmux) failed with status %!STATUS!", status);
            break;
        }

        *out_buf = dev_ctx->attrs.vmux;
        WdfRequestSetInformation(req, (ULONG_PTR)sizeof(*out_buf));
        break;

    case CX_IOCTL_SET_LEVEL:
        status = WdfRequestRetrieveInputBuffer(req, in_len, &in_buf, NULL);

        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfRequestRetrieveInputBuffer (level) failed with status %!STATUS!", status);
            break;
        }

        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "setting level to %d", *in_buf);
        dev_ctx->attrs.level = *in_buf;
        cx_set_level(dev_ctx);
        break;

    case CX_IOCTL_GET_LEVEL:
        status = WdfRequestRetrieveOutputBuffer(req, out_len, &out_buf, NULL);

        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfRequestRetrieveOutputBuffer (level) failed with status %!STATUS!", status);
            break;
        }

        *out_buf = dev_ctx->attrs.level;
        WdfRequestSetInformation(req, (ULONG_PTR)sizeof(*out_buf));
        break;

    case CX_IOCTL_SET_TENBIT:
        status = WdfRequestRetrieveInputBuffer(req, in_len, &in_buf, NULL);

        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfRequestRetrieveInputBuffer (tenbit) failed with status %!STATUS!", status);
            break;
        }

        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "setting tenbit to %d", *in_buf);
        dev_ctx->attrs.tenbit = *in_buf;
        cx_set_tenbit(dev_ctx);
        break;

    case CX_IOCTL_GET_TENBIT:
        status = WdfRequestRetrieveOutputBuffer(req, out_len, &out_buf, NULL);

        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfRequestRetrieveOutputBuffer (tenbit) failed with status %!STATUS!", status);
            break;
        }

        *out_buf = dev_ctx->attrs.tenbit;
        WdfRequestSetInformation(req, (ULONG_PTR)sizeof(*out_buf));
        break;

    case CX_IOCTL_SET_SIXDB:
        status = WdfRequestRetrieveInputBuffer(req, in_len, &in_buf, NULL);

        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfRequestRetrieveInputBuffer (sixdb) failed with status %!STATUS!", status);
            break;
        }

        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "setting sixdb to %d", *in_buf);
        dev_ctx->attrs.sixdb = *in_buf;
        cx_set_level(dev_ctx);
        break;

    case CX_IOCTL_GET_SIXDB:
        status = WdfRequestRetrieveOutputBuffer(req, out_len, &out_buf, NULL);

        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfRequestRetrieveOutputBuffer (sixdb) failed with status %!STATUS!", status);
            break;
        }

        *out_buf = dev_ctx->attrs.sixdb;
        WdfRequestSetInformation(req, (ULONG_PTR)sizeof(*out_buf));
        break;

    case CX_IOCTL_SET_CENTER_OFFSET:
        status = WdfRequestRetrieveInputBuffer(req, in_len, &in_buf, NULL);

        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfRequestRetrieveInputBuffer (center_center) failed with status %!STATUS!", status);
            break;
        }

        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "setting center_center to %d", *in_buf);
        dev_ctx->attrs.center_offset = *in_buf;
        cx_set_center_offset(dev_ctx);
        break;

    case CX_IOCTL_GET_CENTER_OFFSET:
        status = WdfRequestRetrieveOutputBuffer(req, out_len, &out_buf, NULL);

        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfRequestRetrieveOutputBuffer (center_offset) failed with status %!STATUS!", status);
            break;
        }

        *out_buf = dev_ctx->attrs.center_offset;
        WdfRequestSetInformation(req, (ULONG_PTR)sizeof(*out_buf));
        break;

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    WdfRequestComplete(req, status);
}

VOID cx_evt_io_read(
    _In_ WDFQUEUE queue,
    _In_ WDFREQUEST req,
    _In_ size_t req_len
)
{
    NTSTATUS status = STATUS_SUCCESS;
    PDEVICE_CONTEXT dev_ctx;
    WDFMEMORY mem;
    size_t count, tgt_off, offset;
    ULONG page_no;

    dev_ctx = cx_device_get_ctx(WdfIoQueueGetDevice(queue));

    if (!dev_ctx->is_reading)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DBG_GENERAL, "starting capture");

        KeClearEvent(&dev_ctx->isr_event);

        cx_start_capture(dev_ctx);

        status = KeWaitForSingleObject(&dev_ctx->isr_event, Executive, KernelMode, TRUE, NULL);

        if (!NT_SUCCESS(status))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "KeWaitForSingleObject failed with status %!STATUS!", status);
        }

        dev_ctx->initial_page = dev_ctx->gp_cnt;
        dev_ctx->read_offset = 0;
    }

    status = WdfRequestRetrieveOutputMemory(req, &mem);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_GENERAL, "WdfRequestRetrieveOutputMemory failed with status %!STATUS!", status);
        WdfRequestComplete(req, STATUS_UNSUCCESSFUL);
        return;
    }

    count = req_len;
    offset = dev_ctx->read_offset;
    page_no = cx_get_page_no(dev_ctx->initial_page, offset);
    tgt_off = 0;

    dev_ctx->is_reading = TRUE;

    while (count)
    {
        while (count > 0 && page_no != dev_ctx->gp_cnt)
        {
            size_t page_off = offset % PAGE_SIZE;
            size_t len = page_off ? (PAGE_SIZE - page_off) : PAGE_SIZE;

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

            page_no = cx_get_page_no(dev_ctx->initial_page, offset);
        }

        if (count)
        {
            KeClearEvent(&dev_ctx->isr_event);

            // gp_cnt == page_no but read buffer is not filled
            // wait for interrupt to trigger and continue
            status = KeWaitForSingleObject(&dev_ctx->isr_event, Executive, KernelMode, TRUE, NULL);
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
    dev_ctx->read_offset = offset;

    WdfRequestCompleteWithInformation(req, status, req_len);
}

__inline
ULONG cx_get_page_no(
    _In_ ULONG initial_page,
    _In_ size_t off
)
{
    return (((off % CX_VBI_BUF_SIZE) / PAGE_SIZE) + initial_page) % CX_VBI_BUF_COUNT;
}
