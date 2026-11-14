// SPDX-License-Identifier: GPL-2.0-or-later
using System.Buffers.Binary;
using System.ComponentModel;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
using Cxadc_ControlPanel.Models;

namespace Cxadc_ControlPanel.Services;

/// <summary>Native WinUSB client for the clockgen vendor protocol in CxadcClockGen.psm1.</summary>
public sealed class CxadcClockGenService
{
    private static readonly Guid ClockGenInterface = new("9DED1FD1-C739-4D2A-A1E5-E060342724DE");
    private const uint PresentDeviceInterfaces = 0x12;
    private const uint GenericReadWrite = 0xC0000000;
    private const uint FileShareReadWrite = 0x3;
    private const uint OpenExisting = 3;
    private const uint FileAttributeNormal = 0x80;
    private const byte VendorIn = 0xC0;
    private const byte VendorOut = 0x40;
    private const byte StringDescriptor = 3;
    private const byte DeviceDescriptor = 1;

    public IReadOnlyList<ClockGenInfo> GetDevices()
    {
        var result = new List<ClockGenInfo>();
        foreach (var path in EnumerateDevicePaths())
        {
            try
            {
                using var device = Open(path);
                result.Add(ReadDevice(device));
            }
            catch (Win32Exception)
            {
                // Ignore stale interfaces and devices which are not running the cxadc clockgen firmware.
            }
            catch (InvalidOperationException)
            {
                // Ignore interfaces which do not answer the cxadc clockgen protocol.
            }
        }
        return result;
    }

    public ClockGenInfo SetRate(ClockGenInfo device, ClockOutput output, string rate)
    {
        using var handle = Open(device.DevicePath);
        if (output == ClockOutput.Adc)
        {
            if (!uint.TryParse(rate, out var adcRate) || !device.AdcRates.Contains(adcRate))
                throw new ArgumentException("The selected ADC rate is not reported by this device.", nameof(rate));
            var data = new byte[4];
            BinaryPrimitives.WriteUInt32LittleEndian(data, adcRate);
            Transfer(handle, VendorOut, 0x12, 2, data);
        }
        else
        {
            var optionIndex = -1;
            for (var i = 0; i < device.ClockRates.Count; i++)
            {
                if (string.Equals(device.ClockRates[i], rate, StringComparison.OrdinalIgnoreCase))
                {
                    optionIndex = i;
                    break;
                }
            }
            if (optionIndex < 0 || optionIndex > byte.MaxValue)
                throw new ArgumentException("The selected clock rate is not reported by this device.", nameof(rate));
            Transfer(handle, VendorOut, 0x10, (ushort)output, [(byte)optionIndex]);
        }

        return ReadDevice(handle);
    }

    private static ClockGenInfo ReadDevice(DeviceHandle handle)
    {
        var clockRates = GetClockRates(handle);
        var clock0Index = ReadByte(handle, 0x10, 0);
        var clock1Index = ReadByte(handle, 0x10, 1);
        if (clock0Index >= clockRates.Count || clock1Index >= clockRates.Count)
            throw new InvalidOperationException("The clockgen returned an invalid clock rate index.");

        var adcRate = ReadUInt32(handle, 0x12, 2);
        var adcRates = GetAdcRates(handle);
        var descriptor = GetDeviceDescriptor(handle);
        var serial = descriptor[16] == 0 ? string.Empty : GetStringDescriptor(handle, descriptor[16]);

        return new ClockGenInfo(
            handle.Path,
            serial,
            BinaryPrimitives.ReadUInt16LittleEndian(descriptor.AsSpan(12, 2)),
            clockRates[(int)clock0Index],
            clockRates[(int)clock1Index],
            adcRate,
            clockRates,
            adcRates);
    }

    private static IReadOnlyList<string> GetClockRates(DeviceHandle handle)
    {
        var response = Transfer(handle, VendorIn, 0x11, 0, new byte[255]);
        if ((response.Length & 1) != 0)
            throw new InvalidOperationException("The clockgen returned a malformed clock options list.");
        var rates = new List<string>();
        for (var offset = 0; offset < response.Length; offset += 2)
        {
            var descriptorIndex = BinaryPrimitives.ReadUInt16LittleEndian(response.AsSpan(offset, 2));
            rates.Add(GetStringDescriptor(handle, checked((byte)descriptorIndex)));
        }
        return rates;
    }

    private static IReadOnlyList<uint> GetAdcRates(DeviceHandle handle)
    {
        var response = Transfer(handle, VendorIn, 0x13, 2, new byte[255]);
        if ((response.Length & 3) != 0)
            throw new InvalidOperationException("The clockgen returned a malformed ADC options list.");
        var rates = new List<uint>();
        for (var offset = 0; offset < response.Length; offset += 4)
            rates.Add(BinaryPrimitives.ReadUInt32LittleEndian(response.AsSpan(offset, 4)));
        return rates;
    }

    private static byte ReadByte(DeviceHandle handle, byte request, ushort index) =>
        Transfer(handle, VendorIn, request, index, new byte[1]).FirstOrDefault();

    private static uint ReadUInt32(DeviceHandle handle, byte request, ushort index)
    {
        var data = Transfer(handle, VendorIn, request, index, new byte[4]);
        if (data.Length != 4)
            throw new InvalidOperationException("The clockgen returned an incomplete ADC rate.");
        return BinaryPrimitives.ReadUInt32LittleEndian(data);
    }

    private static byte[] GetDeviceDescriptor(DeviceHandle handle)
    {
        var data = new byte[18];
        GetDescriptor(handle, DeviceDescriptor, 0, data);
        if (data[0] < 18 || data[1] != DeviceDescriptor)
            throw new InvalidOperationException("Invalid USB device descriptor.");
        return data;
    }

    private static string GetStringDescriptor(DeviceHandle handle, byte index)
    {
        var data = new byte[512];
        var length = GetDescriptor(handle, StringDescriptor, index, data);
        if (length < 2 || data[1] != StringDescriptor)
            throw new InvalidOperationException("Invalid USB string descriptor.");
        var byteLength = Math.Min(data[0], length);
        return System.Text.Encoding.Unicode.GetString(data, 2, Math.Max(0, byteLength - 2));
    }

    private static int GetDescriptor(DeviceHandle handle, byte type, byte index, byte[] buffer)
    {
        var memory = Marshal.AllocHGlobal(buffer.Length);
        try
        {
            if (!Native.WinUsbGetDescriptor(handle.Interface, type, index, 0, memory, (uint)buffer.Length, out var transferred))
                throw LastWin32("Unable to read a clockgen USB descriptor.");
            Marshal.Copy(memory, buffer, 0, (int)transferred);
            return (int)transferred;
        }
        finally
        {
            Marshal.FreeHGlobal(memory);
        }
    }

    private static byte[] Transfer(DeviceHandle handle, byte requestType, byte request, ushort index, byte[] buffer)
    {
        var packet = new SetupPacket
        {
            RequestType = requestType,
            Request = request,
            Value = 0,
            Index = index,
            Length = checked((ushort)buffer.Length)
        };
        if (!Native.WinUsbControlTransfer(handle.Interface, packet, buffer, (uint)buffer.Length, out var transferred, IntPtr.Zero))
            throw LastWin32($"Clockgen USB request 0x{request:X2} failed.");
        return buffer.AsSpan(0, (int)transferred).ToArray();
    }

    private static IReadOnlyList<string> EnumerateDevicePaths()
    {
        var interfaceGuid = ClockGenInterface;
        var deviceSet = Native.SetupDiGetClassDevs(ref interfaceGuid, IntPtr.Zero, IntPtr.Zero, PresentDeviceInterfaces);
        if (deviceSet == new IntPtr(-1))
            throw LastWin32("Unable to enumerate USB clockgen devices.");

        try
        {
            var paths = new List<string>();
            for (uint index = 0; ; index++)
            {
                var interfaceData = new DeviceInterfaceData { Size = Marshal.SizeOf<DeviceInterfaceData>() };
                if (!Native.SetupDiEnumDeviceInterfaces(deviceSet, IntPtr.Zero, ref interfaceGuid, index, ref interfaceData))
                {
                    if (Marshal.GetLastWin32Error() == 259)
                        break;
                    throw LastWin32("Unable to enumerate a clockgen device interface.");
                }

                Native.SetupDiGetDeviceInterfaceDetail(deviceSet, ref interfaceData, IntPtr.Zero, 0, out var required, IntPtr.Zero);
                var detail = Marshal.AllocHGlobal((int)required);
                try
                {
                    Marshal.WriteInt32(detail, IntPtr.Size == 8 ? 8 : 6);
                    if (!Native.SetupDiGetDeviceInterfaceDetail(deviceSet, ref interfaceData, detail, required, out _, IntPtr.Zero))
                        throw LastWin32("Unable to read a clockgen device path.");
                    var path = Marshal.PtrToStringUni(IntPtr.Add(detail, 4));
                    if (!string.IsNullOrWhiteSpace(path))
                        paths.Add(path);
                }
                finally
                {
                    Marshal.FreeHGlobal(detail);
                }
            }
            return paths;
        }
        finally
        {
            Native.SetupDiDestroyDeviceInfoList(deviceSet);
        }
    }

    private static DeviceHandle Open(string path)
    {
        var file = Native.CreateFile(path, GenericReadWrite, FileShareReadWrite, IntPtr.Zero, OpenExisting, FileAttributeNormal, IntPtr.Zero);
        if (file.IsInvalid)
        {
            file.Dispose();
            throw LastWin32("Unable to open the clockgen. Check that it is connected and has the cxadc firmware.");
        }

        if (!Native.WinUsbInitialize(file, out var interfaceHandle))
        {
            file.Dispose();
            throw LastWin32("Unable to initialize WinUSB for the clockgen.");
        }
        return new DeviceHandle(path, file, interfaceHandle);
    }

    private static Win32Exception LastWin32(string message) => new(Marshal.GetLastWin32Error(), message);

    private sealed class DeviceHandle : IDisposable
    {
        public string Path { get; }
        public SafeFileHandle File { get; }
        public IntPtr Interface { get; }
        public DeviceHandle(string path, SafeFileHandle file, IntPtr handle) => (Path, File, Interface) = (path, file, handle);
        public void Dispose()
        {
            Native.WinUsbFree(Interface);
            File.Dispose();
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct DeviceInterfaceData
    {
        public int Size;
        public Guid InterfaceClassGuid;
        public int Flags;
        public IntPtr Reserved;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    private struct SetupPacket
    {
        public byte RequestType;
        public byte Request;
        public ushort Value;
        public ushort Index;
        public ushort Length;
    }

    private static class Native
    {
        [DllImport("setupapi.dll", EntryPoint = "SetupDiGetClassDevsW", SetLastError = true)]
        public static extern IntPtr SetupDiGetClassDevs(ref Guid classGuid, IntPtr enumerator, IntPtr hwndParent, uint flags);

        [DllImport("setupapi.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool SetupDiEnumDeviceInterfaces(IntPtr deviceInfoSet, IntPtr deviceInfoData, ref Guid interfaceClassGuid, uint memberIndex, ref DeviceInterfaceData deviceInterfaceData);

        [DllImport("setupapi.dll", EntryPoint = "SetupDiGetDeviceInterfaceDetailW", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool SetupDiGetDeviceInterfaceDetail(IntPtr deviceInfoSet, ref DeviceInterfaceData deviceInterfaceData, IntPtr deviceInterfaceDetailData, uint deviceInterfaceDetailDataSize, out uint requiredSize, IntPtr deviceInfoData);

        [DllImport("setupapi.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool SetupDiDestroyDeviceInfoList(IntPtr deviceInfoSet);

        [DllImport("kernel32.dll", EntryPoint = "CreateFileW", CharSet = CharSet.Unicode, SetLastError = true)]
        public static extern SafeFileHandle CreateFile(string fileName, uint desiredAccess, uint shareMode, IntPtr securityAttributes, uint creationDisposition, uint flagsAndAttributes, IntPtr templateFile);

        [DllImport("winusb.dll", EntryPoint = "WinUsb_Initialize", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool WinUsbInitialize(SafeFileHandle deviceHandle, out IntPtr interfaceHandle);

        [DllImport("winusb.dll", EntryPoint = "WinUsb_Free", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool WinUsbFree(IntPtr interfaceHandle);

        [DllImport("winusb.dll", EntryPoint = "WinUsb_ControlTransfer", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool WinUsbControlTransfer(IntPtr interfaceHandle, SetupPacket setupPacket, [In, Out] byte[] buffer, uint bufferLength, out uint lengthTransferred, IntPtr overlapped);

        [DllImport("winusb.dll", EntryPoint = "WinUsb_GetDescriptor", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool WinUsbGetDescriptor(IntPtr interfaceHandle, byte descriptorType, byte index, ushort languageId, IntPtr buffer, uint bufferLength, out uint lengthTransferred);
    }
}
