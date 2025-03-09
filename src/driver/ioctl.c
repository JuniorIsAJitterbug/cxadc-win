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
    PAGED_CODE();

    PFILE_CONTEXT file_ctx = cx_file_get_ctx(file_obj);

    *file_ctx = (FILE_CONTEXT){
        .read_offset = 0,
        .ptr = NULL
    };

    WdfRequestComplete(req, STATUS_SUCCESS);
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
        TRACE_INFO("munmap addr %p", file_ctx->ptr);

        MmUnmapLockedPages(file_ctx->ptr, dev_ctx->user_mdl);
        file_ctx->ptr = NULL;
    }
}

_Use_decl_annotations_
NTSTATUS cx_evt_set_output(WDFREQUEST req, size_t out_len, PVOID buf, size_t buf_len)
{
    if (out_len < buf_len)
    {
        TRACE_ERROR("cx_evt_set_output, buffer too small %Iu < %Iu", buf_len, out_len);
        return STATUS_BUFFER_TOO_SMALL;
    }

    WDFMEMORY mem = WDF_NO_HANDLE;

    RETURN_NTSTATUS_IF_FAILED(WdfRequestRetrieveOutputMemory(req, &mem));
    RETURN_NTSTATUS_IF_FAILED(WdfMemoryCopyFromBuffer(mem, 0, buf, buf_len));

    WdfRequestSetInformation(req, buf_len);
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS cx_evt_get_input(WDFREQUEST req, size_t in_len, PVOID buf, size_t buf_len)
{
    if (in_len < buf_len)
    {
        TRACE_ERROR("cx_evt_get_input, buffer too small %Iu < %Iu", buf_len, in_len);
        return STATUS_BUFFER_TOO_SMALL;
    }

    WDFMEMORY mem = WDF_NO_HANDLE;

    RETURN_NTSTATUS_IF_FAILED(WdfRequestRetrieveInputMemory(req, &mem));
    RETURN_NTSTATUS_IF_FAILED(WdfMemoryCopyToBuffer(mem, 0, buf, buf_len));

    return STATUS_SUCCESS;
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
    NTSTATUS status = STATUS_SUCCESS;
    PDEVICE_CONTEXT dev_ctx = cx_device_get_ctx(WdfIoQueueGetDevice(queue));

    switch (ctrl_code)
    {
        case CX_IOCTL_CONFIG_GET:
            RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, cx_evt_set_output(req, out_len, &dev_ctx->config, sizeof(dev_ctx->config)));
            break;

        case CX_IOCTL_STATE_GET:
            RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, cx_evt_set_output(req, out_len, &dev_ctx->state, sizeof(dev_ctx->state)));
            break;

        case CX_IOCTL_STATE_CAPTURE_STATE_GET:
            RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, cx_evt_set_output(req, out_len, &dev_ctx->state.is_capturing, sizeof(dev_ctx->state.is_capturing)));
            break;

        case CX_IOCTL_STATE_OUFLOW_GET:
            RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, cx_evt_set_output(req, out_len, &dev_ctx->state.ouflow_count, sizeof(dev_ctx->state.ouflow_count)));
            break;

        case CX_IOCTL_CONFIG_VMUX_GET:
            RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, cx_evt_set_output(req, out_len, &dev_ctx->config.vmux, sizeof(dev_ctx->config.vmux)));
            break;

        case CX_IOCTL_CONFIG_LEVEL_GET:
            RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, cx_evt_set_output(req, out_len, &dev_ctx->config.level, sizeof(dev_ctx->config.level)));
            break;

        case CX_IOCTL_CONFIG_TENBIT_GET:
            RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, cx_evt_set_output(req, out_len, &dev_ctx->config.tenbit, sizeof(dev_ctx->config.tenbit)));
            break;

        case CX_IOCTL_CONFIG_SIXDB_GET:
            RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, cx_evt_set_output(req, out_len, &dev_ctx->config.sixdb, sizeof(dev_ctx->config.sixdb)));
            break;

        case CX_IOCTL_CONFIG_CENTER_OFFSET_GET:
            RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, cx_evt_set_output(req, out_len, &dev_ctx->config.center_offset, sizeof(dev_ctx->config.center_offset)));
            break;

        case CX_IOCTL_HW_BUS_NUMBER_GET:
            RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, cx_evt_set_output(req, out_len, &dev_ctx->bus_number, sizeof(dev_ctx->bus_number)));
            break;

        case CX_IOCTL_HW_DEVICE_ADDRESS_GET:
            RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, cx_evt_set_output(req, out_len, &dev_ctx->dev_addr, sizeof(dev_ctx->dev_addr)));
            break;

        case CX_IOCTL_STATE_WIN32_PATH_GET:
        {
            DECLARE_UNICODE_STRING_SIZE(symlink_path, 128);
            RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, RtlUnicodeStringPrintf(&symlink_path, L"%ws%d", WIN32_PATH, dev_ctx->dev_idx));
            RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, cx_evt_set_output(req, out_len, symlink_path.Buffer, symlink_path.Length));
            break;
        }

        case CX_IOCTL_HW_REGISTER_GET:
        {
            ULONG value = 0;
            RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, cx_evt_get_input(req, in_len, &value, sizeof(ULONG)));

            if (value < CX_REGISTER_BASE || value > CX_REGISTER_END)
            {
                TRACE_ERROR("CX_IOCTL_GET_REGISTER address %08X out of range", value);
                status = STATUS_INVALID_PARAMETER;
                break;
            }

            ULONG reg_val = cx_read(&dev_ctx->mmio, value);
            RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, cx_evt_set_output(req, out_len, &reg_val, sizeof(ULONG)));
            break;
        }

        case CX_IOCTL_STATE_OUFLOW_RESET:
            cx_ctrl_reset_ouflow_count(dev_ctx);
            break;

        case CX_IOCTL_CONFIG_VMUX_SET:
        {
            ULONG value = 0;
            RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, cx_evt_get_input(req, in_len, &value, sizeof(dev_ctx->config.vmux)));
            RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, cx_ctrl_set_vmux(dev_ctx, value));
            break;
        }

        case CX_IOCTL_CONFIG_LEVEL_SET:
        {
            ULONG value = 0;
            RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, cx_evt_get_input(req, in_len, &value, sizeof(dev_ctx->config.level)));
            RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, cx_ctrl_set_level(dev_ctx, value));
            break;
        }

        case CX_IOCTL_CONFIG_TENBIT_SET:
        {
            ULONG value = 0;
            RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, cx_evt_get_input(req, in_len, &value, sizeof(dev_ctx->config.tenbit)));
            RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, cx_ctrl_set_tenbit(dev_ctx, value ? TRUE : FALSE));
            break;
        }

        case CX_IOCTL_CONFIG_SIXDB_SET:
        {
            ULONG value = 0;
            RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, cx_evt_get_input(req, in_len, &value, sizeof(dev_ctx->config.sixdb)));
            RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, cx_ctrl_set_sixdb(dev_ctx, value ? TRUE : FALSE));
            break;
        }

        case CX_IOCTL_CONFIG_CENTER_OFFSET_SET:
        {
            ULONG value = 0;
            RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, cx_evt_get_input(req, in_len, &value, sizeof(dev_ctx->config.center_offset)));
            RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, cx_ctrl_set_center_offset(dev_ctx, value));
            break;
        }

        case CX_IOCTL_HW_REGISTER_SET:
        {
            SET_REGISTER_DATA data = { 0 };

            RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, cx_evt_get_input(req, in_len, &data, sizeof(SET_REGISTER_DATA)));

            if (data.addr < CX_REGISTER_BASE || data.addr  > CX_REGISTER_END)
            {
                TRACE_ERROR("CX_IOCTL_SET_REGISTER address %08X out of range", data.addr);
                status = STATUS_INVALID_PARAMETER;
                break;
            }

            TRACE_INFO("CX_IOCTL_SET_REGISTER writing %08X to %08X", data.val, data.addr);
            cx_write(&dev_ctx->mmio, data.addr, data.val);
            WdfRequestSetInformation(req, 0);
            break;
        }

        case CX_IOCTL_HW_MMAP:
        {
            PFILE_CONTEXT file_ctx = cx_file_get_ctx(WdfRequestGetFileObject(req));

            if (file_ctx->ptr == NULL)
            {
                file_ctx->ptr = MmMapLockedPagesSpecifyCache(dev_ctx->user_mdl, UserMode, MmNonCached, NULL, FALSE, NormalPagePriority);
            }

            RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, cx_evt_set_output(req, out_len, &file_ctx->ptr, sizeof(UINT_PTR)));
            TRACE_INFO("CX_IOCTL_MMAP addr %p", file_ctx->ptr);
            break;
        }

        case CX_IOCTL_HW_MUNMAP:
        {
            PFILE_CONTEXT file_ctx = cx_file_get_ctx(WdfRequestGetFileObject(req));

            if (file_ctx->ptr != NULL)
            {
                TRACE_INFO("CX_IOCTL_MUNMAP addr %p", file_ctx->ptr);

                MmUnmapLockedPages(file_ctx->ptr, dev_ctx->user_mdl);
                file_ctx->ptr = NULL;
            }

            WdfRequestSetInformation(req, 0);
            break;
        }

        default:
            TRACE_ERROR("unknown ioctl %X", ctrl_code);
            status = STATUS_INVALID_DEVICE_REQUEST;
            WdfRequestSetInformation(req, 0);
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
    LARGE_INTEGER timeout =
    {
        .QuadPart = WDF_REL_TIMEOUT_IN_MS(READ_TIMEOUT)
    };

    // start capture if idle
    if (!dev_ctx->state.is_capturing)
    {
        KeClearEvent(&dev_ctx->isr_event);

        cx_start_capture(dev_ctx);

        RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, KeWaitForSingleObject(&dev_ctx->isr_event, Executive, KernelMode, FALSE, &timeout));

        InterlockedExchange((PLONG)&dev_ctx->state.initial_page, dev_ctx->state.last_gp_cnt);
    }

    // new reader, increment count
    if (!file_ctx->read_offset)
    {
        InterlockedIncrement((PLONG)&dev_ctx->state.reader_count);
    }

    WDFMEMORY mem = WDF_NO_HANDLE;
    RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, WdfRequestRetrieveOutputMemory(req, &mem));

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

            RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, WdfMemoryCopyFromBuffer(mem, tgt_off, &dev_ctx->risc.page[page_no].va[page_off], len));

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
            RETURN_COMPLETE_WDFREQUEST_IF_FAILED(req, KeWaitForSingleObject(&dev_ctx->isr_event, Executive, KernelMode, FALSE, &timeout));
        }
    }

    // our read request does not contain an offset,
    // so we keep track of it for the duration of the capture
    InterlockedExchange64((PLONG64)&file_ctx->read_offset, offset);

    WdfRequestCompleteWithInformation(req, status, req_len - count);
}
