// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * cxadc-win-tool - Example tool for using the cxadc-win driver
 *
 * Copyright (C) 2024-2025 Jitterbug <jitterbug@posteo.co.uk>
 */

using System.Buffers.Binary;
using System.Runtime.InteropServices;
using Windows.Win32;
using Windows.Win32.Foundation;
using Windows.Win32.Devices.DeviceAndDriverInstallation;
using Microsoft.Win32.SafeHandles;
using System.Text;

namespace cxadc_win_tool;

public class CxadcWin : IDisposable
{
    public const int ReadBufferSize = 2 * 1024 * 1024; // 2MB
    public const int VBIBufferSize = 64 * 1024 * 1024; // 64MB

    public readonly static Guid DeviceGuid = new("13EF40B0-05FF-4173-B613-3141AD2E3762");

    public enum IoctlCode : uint
    {
        GetConfig = 0x700,
        GetState = 0x701,
        GetCaptureState = 0x800,
        GetOUFlowCount = 0x810,
        GetVMux = 0x821,
        GetLevel = 0x822,
        GetTenbit = 0x823,
        GetSixDB = 0x824,
        GetCenterOffset = 0x825,
        GetRegister = 0x82F,
        GetBusNumber = 0x830,
        GetDeviceAddress = 0x831,
        GetWin32Path = 0x832,
        ResetOUFlowCount = 0x910,
        SetVMux = 0x921,
        SetLevel = 0x922,
        SetTenbit = 0x923,
        SetSixDB = 0x924,
        SetCenterOffset = 0x925,
        SetRegister = 0x92F,
    }

    public SafeFileHandle Handle { get => _handle; }
    public long TotalBytesRead { get => _totalBytesRead; }

    public uint VMux
    {
        get => IoctlGetUint(IoctlCode.GetVMux);
        set => IoctlSetUint(IoctlCode.SetVMux, value);
    }

    public uint Level
    {
        get => IoctlGetUint(IoctlCode.GetLevel);
        set => IoctlSetUint(IoctlCode.SetLevel, value);
    }

    public bool IsTenbitEnabled
    {
        get => IoctlGetBool(IoctlCode.GetTenbit);
        set => IoctlSetBool(IoctlCode.SetTenbit, value);
    }
    public bool IsSixDBEnabled
    {
        get => IoctlGetBool(IoctlCode.GetSixDB);
        set => IoctlSetBool(IoctlCode.SetSixDB, value);
    }
    public uint CenterOffset
    {
        get => IoctlGetUint(IoctlCode.GetCenterOffset);
        set => IoctlSetUint(IoctlCode.SetCenterOffset, value);
    }

    public bool IsCapturing
    {
        get => Convert.ToBoolean(IoctlGetBool(IoctlCode.GetCaptureState));
    }

    public uint OUFlowCount
    {
        get => IoctlGetUint(IoctlCode.GetOUFlowCount);
    }

    private string? _win32path = null;
    public string Win32Path
    {
        get => this._win32path ??= Encoding.Unicode.GetString(IoctlGetBytes(IoctlCode.GetWin32Path, 128));
    }

    private uint? _pciBusNumber = null;
    public uint PciBusNumber
    {
        get => this._pciBusNumber ??= IoctlGetUint(IoctlCode.GetBusNumber);
    }

    private uint? _deviceAddress = null;
    public uint DeviceAddress
    {
        get => this._deviceAddress ??= IoctlGetUint(IoctlCode.GetDeviceAddress);
    }

    public uint DeviceNumber
    {
        get => (DeviceAddress >> 16) & 0x0000FFFF;
    }

    public uint FunctionNumber
    {
        get => DeviceAddress & 0x0000FFFF;
    }

    private readonly SafeFileHandle _handle;
    private bool _disposed = false;
    private long _totalBytesRead = 0;

    public CxadcWin(string devicePath)
    {
        this._handle = File.OpenHandle(devicePath, FileMode.Open, FileAccess.ReadWrite, FileShare.ReadWrite, FileOptions.Asynchronous);

        if (this._handle.IsInvalid)
        {
            throw new Exception($"CreateFile failed: {Marshal.GetLastPInvokeErrorMessage()} ({Marshal.GetLastWin32Error()})");
        }
    }

    public async Task Read(Stream outputStream, CancellationToken cancelToken)
    {
        using var deviceStream = new FileStream(this._handle, FileAccess.Read, ReadBufferSize, true);
        var buffer = new byte[ReadBufferSize];
        this._totalBytesRead = 0;

        while (!cancelToken.IsCancellationRequested)
        {
            this._totalBytesRead += await deviceStream.ReadAsync(buffer, cancelToken);
            await outputStream.WriteAsync(buffer, cancelToken);
        }
    }

    public byte[] Ioctl(IoctlCode code, FileAccess access, byte[]? inputData = null, int outBufferSize = 0)
    {
        unsafe
        {
            uint bytesRead = 0;
            var inBufferSize = inputData != null ? inputData.Length : 0;
            var inputBuffer = inBufferSize != 0 ? Marshal.AllocHGlobal(inBufferSize) : IntPtr.Zero;
            var outputBuffer = outBufferSize != 0 ? Marshal.AllocHGlobal(outBufferSize) : IntPtr.Zero;

            if (inputData != null && inputBuffer != IntPtr.Zero)
            {
                Marshal.Copy(inputData, 0, inputBuffer, (int)inBufferSize);
            }

            if (!PInvoke.DeviceIoControl(
                this._handle,
                GetCtlCode(code, access),
                inputBuffer.ToPointer(),
                (uint)inBufferSize,
                outputBuffer.ToPointer(),
                (uint)outBufferSize,
                &bytesRead,
                null))
            {
                throw new Exception($"DeviceIoControl failed: {Marshal.GetLastPInvokeErrorMessage()} ({Marshal.GetLastWin32Error()})");
            }

            var ret = new byte[bytesRead];
            
            if (bytesRead > 0 && outputBuffer != IntPtr.Zero)
            {
                Marshal.Copy(outputBuffer, ret, 0, (int)bytesRead);
            }

            return ret;
        }
    }

    public static List<string> EnumerateDevices()
    {
        var devicePaths = new List<string>();

        var flags = SETUP_DI_GET_CLASS_DEVS_FLAGS.DIGCF_PRESENT | SETUP_DI_GET_CLASS_DEVS_FLAGS.DIGCF_DEVICEINTERFACE;
        using var devHandle = PInvoke.SetupDiGetClassDevs(DeviceGuid, "", (HWND)null, flags);

        if (devHandle != null && devHandle.IsInvalid)
        {
            throw new Exception($"SetupDiGetDeviceInterfaceDetail failed: {Marshal.GetLastPInvokeErrorMessage()} ({Marshal.GetLastWin32Error()})");
        }

        var devIfaceData = new SP_DEVICE_INTERFACE_DATA { cbSize = (uint)Marshal.SizeOf<SP_DEVICE_INTERFACE_DATA>() };
        var count = 0u;

        while (PInvoke.SetupDiEnumDeviceInterfaces(devHandle, null, DeviceGuid, count++, ref devIfaceData))
        {
            uint requiredSize = 0;

            unsafe
            {
                // get req size for details
                if (!PInvoke.SetupDiGetDeviceInterfaceDetail(devHandle, devIfaceData, null, 0, &requiredSize, null)
                    && Marshal.GetLastWin32Error() != (int)WIN32_ERROR.ERROR_INSUFFICIENT_BUFFER)
                {
                    // failed with unexpected err
                    throw new Exception($"SetupDiGetDeviceInterfaceDetail failed: {Marshal.GetLastPInvokeErrorMessage()} ({Marshal.GetLastWin32Error()})");
                }

                var devIfaceDetailDataPtr = (SP_DEVICE_INTERFACE_DETAIL_DATA_W*)Marshal.AllocHGlobal((int)requiredSize);
                Marshal.StructureToPtr(
                    new SP_DEVICE_INTERFACE_DETAIL_DATA_W { cbSize = (uint)Marshal.SizeOf<SP_DEVICE_INTERFACE_DETAIL_DATA_W>() },
                    (nint)devIfaceDetailDataPtr, false);

                // get details
                if (!PInvoke.SetupDiGetDeviceInterfaceDetail(devHandle, devIfaceData, devIfaceDetailDataPtr, requiredSize, null, null))
                {
                    throw new Exception($"SetupDiGetDeviceInterfaceDetail failed: {Marshal.GetLastPInvokeErrorMessage()} ({Marshal.GetLastWin32Error()})");
                }

                unsafe
                {
                    var path = new string((char*)&devIfaceDetailDataPtr->DevicePath);

                    if (path != null)
                    {
                        devicePaths.Add(path);
                    }
                }
            }
        }

        // check if unexpected enum err
        if (Marshal.GetLastWin32Error() != (uint)WIN32_ERROR.ERROR_NO_MORE_ITEMS)
        {
            throw new Exception($"SetupDiEnumDeviceInterfaces failed: {Marshal.GetLastPInvokeErrorMessage()} ({Marshal.GetLastWin32Error()})");
        }

        return devicePaths;
    }

    public async Task LevelAdjust(uint startingLevel, uint sampleCount, uint oorLimit, uint verifyPasses, CancellationToken cancelToken)
    {
        using var deviceStream = new FileStream(this._handle, FileAccess.Read, ReadBufferSize, true);

        // based on code from cxadc-linux3 leveladj.c
        var verifyCount = 0;
        var tenbit = this.IsTenbitEnabled;
        var levelInc = 1;

        var level = startingLevel;
        var lowerLimit = tenbit ? 0x0800 : 0x08;
        var upperLimit = tenbit ? 0xF800 : 0xF8;
        var valueSize = tenbit ? sizeof(ushort) : sizeof(byte);
        var maxValue = tenbit ? ushort.MaxValue : byte.MaxValue;

        while (verifyCount < verifyPasses && !cancelToken.IsCancellationRequested)
        {
            var buffer = new byte[sampleCount];
            var lowestValue = maxValue;
            var highestValue = 0;
            uint oorCount = 0;

            Console.WriteLine($"Testing level {level}");
            this.Level = level;

            if (await deviceStream.ReadAsync(buffer, cancelToken) != sampleCount)
            {
                throw new Exception($"Unable to read {sampleCount} bytes");
            }

            // check data for clipping
            for (var i = 0; i < sampleCount / valueSize && oorCount <= oorLimit && !cancelToken.IsCancellationRequested; i += valueSize)
            {
                var value = tenbit ? BinaryPrimitives.ReadUInt16LittleEndian(buffer.AsSpan()[i..(i + valueSize)]) : buffer[i];

                // save new low/high values
                if (value < lowestValue)
                {
                    lowestValue = value;
                }

                if (value > highestValue)
                {
                    highestValue = value;
                }

                // increment counter if value is out of range
                if (value <= lowerLimit || value >= upperLimit)
                {
                    oorCount += 1;
                }

                // force level change if clipping
                if (value == maxValue || value == 0)
                {
                    oorCount += oorLimit;
                    break;
                }
            }

            Console.WriteLine("Low: {0,-5} High: {1,-5} Over: {2,-5} {3}", lowestValue, highestValue, oorCount, oorCount >= oorLimit ? "CLIPPING" : "");

            // increase verify count if we've stopped changing level
            if (levelInc == 0)
            {
                verifyCount += 1;
            }

            // if level has just been decreased, stop increasing
            if (levelInc == -1)
            {
                levelInc = 0;
            }

            // decrease level if over out of range limit or clipping
            if (oorCount >= oorLimit)
            {
                levelInc = -1;

                // reset verify count
                verifyCount = 0;
            }

            // prevent level from going out of range
            if (level == 31 || level == 0)
            {
                levelInc = 0;
            }

            level += (uint)levelInc;
        }

        Console.WriteLine($"Stopped on level {level}");
    }

    public void PrintConfig()
    {
        Console.WriteLine("{0,-15} {1,-8} {2,15}", "path", this.Win32Path, this.IsCapturing ? "**capturing**" : "");
        Console.WriteLine("{0,-15} {1,-8}", "location", $"{this.PciBusNumber:00}:{this.DeviceNumber:00}.{this.FunctionNumber:0}");
        Console.WriteLine("{0,-15} {1,-8}", "vmux", this.VMux);
        Console.WriteLine("{0,-15} {1,-8}", "level", this.Level);
        Console.WriteLine("{0,-15} {1,-8}", "tenbit", this.IsTenbitEnabled);
        Console.WriteLine("{0,-15} {1,-8}", "sixdb", this.IsSixDBEnabled);
        Console.WriteLine("{0,-15} {1,-8}", "center_offset", this.CenterOffset);
        Console.WriteLine("{0,-15} {1,-8}", "ouflow_count", this.OUFlowCount);
        Console.WriteLine();
    }

    // ioctl set fns
    public void SetRegister(string address, string value) =>
        SetRegister(Convert.ToUInt32(address, 16), Convert.ToUInt32(value, 16));

    public void SetRegister(uint address, uint value)
    {
        var inputData = new byte[8];
        BinaryPrimitives.WriteUInt32LittleEndian(inputData, address);
        BinaryPrimitives.WriteUInt32LittleEndian(inputData.AsSpan()[4..], value);
        Ioctl(IoctlCode.SetRegister, FileAccess.Write, inputData, 0);
    }
    public void IoctlSetBool(IoctlCode code, bool inputData, int outBufferSize = 0) =>
        Ioctl(code, FileAccess.Write, [Convert.ToByte(inputData)], outBufferSize);

    public void IoctlSetUint(IoctlCode code, uint value, int outBufferSize = 0)
    {
        var inputData = new byte[4];
        BinaryPrimitives.WriteUInt32LittleEndian(inputData, value);
        Ioctl(code, FileAccess.Write, inputData, outBufferSize);
    }

    // Ioctl get fns
    public uint GetRegister(string address) =>
        GetRegister(Convert.ToUInt32(address, 16));

    public uint GetRegister(uint address)
    {
        var inputData = new byte[4];
        BinaryPrimitives.WriteUInt32LittleEndian(inputData, address);
        return BinaryPrimitives.ReadUInt32LittleEndian(Ioctl(IoctlCode.GetRegister, FileAccess.Read, inputData, 4));
    }

    public bool IoctlGetBool(IoctlCode code) =>
        Ioctl(code, FileAccess.Read, null, 1)[0] == 1;

    public uint IoctlGetUint(IoctlCode code, byte[]? inputData = null) =>
        BinaryPrimitives.ReadUInt32LittleEndian(Ioctl(code, FileAccess.Read, inputData, 4));

    public byte[] IoctlGetBytes(IoctlCode code, int outBufferSize = 0) =>
        Ioctl(code, FileAccess.Read, null, outBufferSize);

    internal static uint GetCtlCode(IoctlCode function, FileAccess method) =>
        PInvoke.FILE_DEVICE_UNKNOWN << 16 | (uint)method << 14 | (uint)function << 2 | PInvoke.METHOD_BUFFERED;

    ~CxadcWin() => Dispose();

    public void Dispose()
    {
        Dispose(true);
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
}
