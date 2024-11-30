﻿// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * cxadc-win-tool - Example tool for using the cxadc-win driver
 *
 * Copyright (C) 2024 Jitterbug
 */

using LibUsbDotNet;
using LibUsbDotNet.Main;

namespace cxadc_win_tool;

// Use libusb-win32 filter driver to get/set clock
// There may be an easier/saner way to achieve this...
public class Clockgen : IDisposable
{
    private const ushort VendorId = 0x1209;
    private const ushort ProductId = 0x0001;

    private readonly IUsbDevice? _device;
    private bool _disposed = false;

    public Clockgen()
    {
        this._device = (IUsbDevice)UsbDevice.OpenUsbDevice(new UsbDeviceFinder(VendorId, ProductId));

        if (this._device == null || !this._device.IsOpen)
        {
            throw new Exception($"Device {VendorId:X4}:{ProductId:X4} not found, is libusb_win32 filter driver installed?");
        }
    }

    public double GetClock(uint clockIdx)
    {
        if (clockIdx > 1)
        {
            throw new Exception($"Invalid clock {clockIdx}");
        }

        var cmd = new UsbSetupPacket
        {
            RequestType = 0xA1,
            Request = 0x01,
            Value = 0x0100,
            Index = (short)(0x2000 + (clockIdx << 8)),
            Length = 0x0001
        };

        var buf = new byte[1];
        var ret = this._device!.ControlTransfer(ref cmd, buf, buf.Length, out var len);

        if (!ret || len == 0)
        {
            throw new Exception($"Error reading clock {UsbDevice.LastErrorNumber} / {UsbDevice.LastErrorString}");
        }

        return GetFreq(buf[0]);
    }

    public bool SetClock(uint clockIdx, byte freqIdx)
    {
        if (clockIdx > 1)
        {
            throw new Exception($"Invalid clock {clockIdx}");
        }

        var cmd = new UsbSetupPacket
        {
            RequestType = 0x21,
            Request = 0x01,
            Value = 0x0100,
            Index = (short)(0x2000 + (clockIdx << 8)),
            Length = 0x0001
        };

        var buf = new byte[1] { freqIdx };
        var ret = this._device!.ControlTransfer(ref cmd, buf, buf.Length, out var len);

        if (!ret || len == 0)
        {
            throw new Exception($"Error setting clock {UsbDevice.LastErrorNumber} / {UsbDevice.LastErrorString}");
        }

        return ret;
    }

    public double GetFreq(uint freqIdx) => freqIdx switch
    {
        1 => 20.00000,
        2 => 28.63636,
        3 => 40.00000,
        4 => 50.00000,
        _ => throw new Exception("Unknown freq")
    };

    ~Clockgen() => Dispose();

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
            this._device?.Close();
            UsbDevice.Exit();
        }

        this._disposed = true;
    }
}