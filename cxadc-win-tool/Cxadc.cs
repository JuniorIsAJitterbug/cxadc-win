﻿// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * cxadc-win-tool - Example tool for using the cxadc-win driver
 *
 * Copyright (C) 2024 Jitterbug
 */

using System.Runtime.InteropServices;
using Windows.Win32;
using Windows.Win32.Storage.FileSystem;

namespace cxadc_win_tool;

public class Cxadc : IDisposable
{
    public const uint CX_IOCTL_SET_VMUX = 0x901;
    public const uint CX_IOCTL_GET_VMUX = 0x961;
    public const uint CX_IOCTL_SET_LEVEL = 0x902;
    public const uint CX_IOCTL_GET_LEVEL = 0x962;
    public const uint CX_IOCTL_SET_TENBIT = 0x903;
    public const uint CX_IOCTL_GET_TENBIT = 0x963;
    public const uint CX_IOCTL_SET_SIXDB = 0x904;
    public const uint CX_IOCTL_GET_SIXDB = 0x964;
    public const uint CX_IOCTL_SET_CENTER_OFFSET = 0x905;
    public const uint CX_IOCTL_GET_CENTER_OFFSET = 0x965;

    const uint FILE_DEVICE_UNKNOWN = 0x00000022;
    const uint METHOD_BUFFERED = 0;
    const uint FILE_READ_DATA = 0x0001;
    const uint FILE_WRITE_DATA = 0x0002;

    private readonly SafeHandle _handle;
    private bool _disposed = false;

    public Cxadc(string devicePath)
    {
        this._handle = PInvoke.CreateFile(
            devicePath,
            (uint)0x80000000L | (uint)0x40000000L, // R/W
            FILE_SHARE_MODE.FILE_SHARE_READ | FILE_SHARE_MODE.FILE_SHARE_WRITE,
            null,
            FILE_CREATION_DISPOSITION.OPEN_EXISTING,
            0,
            null);

        var err = Marshal.GetLastWin32Error();

        if (err != 0)
        {
            throw new Exception($"Unable to open {devicePath} err {err}");
        }
    }

    public int Read(Span<byte> buffer)
    {
        uint bytesRead = 0;
        bool ret;

        unsafe
        {
            ret = PInvoke.ReadFile(this._handle, buffer, &bytesRead, null);
        }

        if (!ret)
        {
            var err = Marshal.GetLastWin32Error();

            if (err != 0)
            {
                throw new Exception($"read failed with err {err}");
            }
        }

        return (int)bytesRead;
    }

    public uint Get(uint code)
    {
        nint buffer = nint.Zero;
        uint bytesRead = 0;
        uint ret = 0;

        unsafe
        {
            buffer = Marshal.AllocHGlobal(sizeof(uint));

            PInvoke.DeviceIoControl(
                this._handle,
                GetCtlCode(code, FILE_READ_DATA),
                null,
                0,
                buffer.ToPointer(),
                sizeof(uint),
                &bytesRead,
                null);

            var err = Marshal.GetLastWin32Error();

            if (err != 0)
            {
                throw new Exception($"get failed with err {err}");
            }

            ret = *(uint*)buffer;
        }

        return ret;
    }

    public uint Set(uint code, uint data)
    {
        nint buffer = nint.Zero;
        uint bytesRead = 0;
        uint ret = 0;

        unsafe
        {
            buffer = Marshal.AllocHGlobal(sizeof(uint));

            PInvoke.DeviceIoControl(
                this._handle,
                GetCtlCode(code, FILE_WRITE_DATA),
                &data,
                sizeof(uint),
                buffer.ToPointer(),
                sizeof(uint),
                &bytesRead,
                null);

            var err = Marshal.GetLastWin32Error();

            if (err != 0)
            {
                throw new Exception($"set failed with err {err}");
            }

            ret = *(uint*)buffer;
        }

        return ret;
    }

    ~Cxadc() => Dispose();

    public void Dispose()
    {
        this.Dispose(true);
        GC.SuppressFinalize(this);
    }

    protected virtual void Dispose(bool disposing)
    {
        if (this._disposed)
        {
            return;
        }

        if (disposing)
        {
            if (!this._handle.IsClosed)
            {
                this._handle.Close();
            }
        }

        this._disposed = true;
    }

    public static uint GetCtlCode(uint function, uint method)
    {
        return FILE_DEVICE_UNKNOWN << 16 | method << 14 | function << 2 | METHOD_BUFFERED;
    }
}
