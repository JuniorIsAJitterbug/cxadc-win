// SPDX-License-Identifier: GPL-2.0-or-later
using System.Globalization;
using System.Management;
using Cxadc_ControlPanel.Models;

namespace Cxadc_ControlPanel.Services;

/// <summary>Reads and writes the driver's published root/WMI classes.</summary>
public sealed class CxadcWmiService
{
    public const int RegisterMinimum = 0x200000;
    public const int RegisterMaximum = 0x5CFFFF;
    private const int FunctionRegister = 0x365000;
    private readonly ManagementScope _scope = new(@"\\.\root\WMI");

    public IReadOnlyList<CxadcDevice> GetDevices()
    {
        using var searcher = new ManagementObjectSearcher(_scope, new ObjectQuery("SELECT * FROM CxadcWin_Path"));
        using var results = searcher.Get();
        var devices = new List<CxadcDevice>();

        foreach (ManagementObject item in results)
        {
            var path = Convert.ToString(item["Path"], CultureInfo.InvariantCulture);
            var instance = Convert.ToString(item["InstanceName"], CultureInfo.InvariantCulture);
            if (!string.IsNullOrWhiteSpace(path) && !string.IsNullOrWhiteSpace(instance))
                devices.Add(new CxadcDevice(path, instance, GetPciLocation(instance)));
            item.Dispose();
        }

        return devices;
    }

    private string GetPciLocation(string instanceName)
    {
        using var searcher = new ManagementObjectSearcher(_scope, new ObjectQuery("SELECT * FROM CxadcWin_Hardware"));
        using var results = searcher.Get();
        foreach (ManagementObject item in results)
        {
            if (string.Equals(Convert.ToString(item["InstanceName"], CultureInfo.InvariantCulture), instanceName, StringComparison.OrdinalIgnoreCase))
            {
                var bus = Convert.ToUInt32(item["PciBusNumber"], CultureInfo.InvariantCulture);
                var address = Convert.ToUInt32(item["PciAddress"], CultureInfo.InvariantCulture);
                item.Dispose();
                return string.Create(CultureInfo.InvariantCulture, $"{bus:D2}:{(address >> 16) & 0xFFFF:D2}.{address & 0xFFFF:D1}");
            }
            item.Dispose();
        }
        return "";
    }

    public CxadcConfiguration GetConfiguration(CxadcDevice device)
    {
        using var item = FindInstance("CxadcWin_Config", device.InstanceName);
        return new CxadcConfiguration(
            ReadBool(item, "Active"),
            ReadInt(item, "VideoMux"),
            ReadInt(item, "Level"),
            ReadInt(item, "CenterOffset"),
            ReadBool(item, "EnableTenbit"),
            ReadBool(item, "EnableSixDB"));
    }

    public CxadcState GetState(CxadcDevice device)
    {
        using var item = FindInstance("CxadcWin_State", device.InstanceName);
        return new CxadcState(
            ReadBool(item, "Active"),
            ReadBool(item, "IsCapturing"),
            ReadInt(item, "ReaderCount"),
            ReadInt(item, "LastGpCount"),
            ReadInt(item, "InitialPage"),
            Convert.ToUInt32(item["OverUnderflowCount"], CultureInfo.InvariantCulture));
    }

    public void SetConfiguration(CxadcDevice device, CxadcConfiguration configuration)
    {
        if (configuration.VideoMux is < 0 or > 3)
            throw new ArgumentOutOfRangeException(nameof(configuration), "Video Mux must be 0–3.");
        if (configuration.Level is < 0 or > 31)
            throw new ArgumentOutOfRangeException(nameof(configuration), "Level must be 0–31.");
        if (configuration.CenterOffset is < 0 or > 63)
            throw new ArgumentOutOfRangeException(nameof(configuration), "Center Offset must be 0–63.");

        using var item = FindInstance("CxadcWin_Config", device.InstanceName);
        item["VideoMux"] = configuration.VideoMux;
        item["Level"] = configuration.Level;
        item["CenterOffset"] = configuration.CenterOffset;
        item["EnableTenbit"] = configuration.EnableTenbit;
        item["EnableSixDB"] = configuration.EnableSixDB;
        item.Put();
    }

    public uint ResetOverflowCount(CxadcDevice device)
    {
        using var item = FindInstance("CxadcWin_State", device.InstanceName);
        using var result = item.InvokeMethod("ResetOverUnderflowCount", null, null);
        return result is null || result["PreviousValue"] is null
            ? 0
            : Convert.ToUInt32(result["PreviousValue"], CultureInfo.InvariantCulture);
    }

    public uint ReadRegister(CxadcDevice device, uint address)
    {
        ValidateRegisterAddress(address);
        using var item = FindInstance("CxadcWin_Registers", device.InstanceName);
        using var input = item.GetMethodParameters("Get");
        input["Address"] = address;
        using var output = item.InvokeMethod("Get", input, null);
        return Convert.ToUInt32(output?["Value"], CultureInfo.InvariantCulture);
    }

    public void WriteRegister(CxadcDevice device, uint address, uint value)
    {
        ValidateRegisterAddress(address);
        using var item = FindInstance("CxadcWin_Registers", device.InstanceName);
        using var input = item.GetMethodParameters("Set");
        input["Address"] = address;
        input["Value"] = value;
        using var output = item.InvokeMethod("Set", input, null);
    }

    public CxadcFunctions GetFunctions(CxadcDevice device)
    {
        var value = ReadRegister(device, FunctionRegister);
        return new CxadcFunctions(
            (value & 0x01) != 0,
            (value & 0x02) != 0,
            (value & 0x04) != 0,
            (value & 0x10) != 0);
    }

    public void SetFunctions(CxadcDevice device, CxadcFunctions functions)
    {
        var current = ReadRegister(device, FunctionRegister);
        const uint mask = 0x17;
        var requested = (functions.Video ? 0x01u : 0u)
            | (functions.Audio ? 0x02u : 0u)
            | (functions.Mpeg ? 0x04u : 0u)
            | (functions.Infrared ? 0x10u : 0u);
        WriteRegister(device, FunctionRegister, (current & ~mask) | requested);
    }

    private ManagementObject FindInstance(string className, string instanceName)
    {
        using var searcher = new ManagementObjectSearcher(_scope, new ObjectQuery($"SELECT * FROM {className}"));
        using var results = searcher.Get();
        foreach (ManagementObject item in results)
        {
            if (string.Equals(Convert.ToString(item["InstanceName"], CultureInfo.InvariantCulture), instanceName, StringComparison.OrdinalIgnoreCase))
                return item;
            item.Dispose();
        }

        throw new InvalidOperationException($"The driver did not return a {className} instance for {instanceName}.");
    }

    private static bool ReadBool(ManagementObject item, string name) =>
        Convert.ToBoolean(item[name], CultureInfo.InvariantCulture);

    private static int ReadInt(ManagementObject item, string name) =>
        Convert.ToInt32(item[name], CultureInfo.InvariantCulture);

    private static void ValidateRegisterAddress(uint address)
    {
        if (address < RegisterMinimum || address > RegisterMaximum)
            throw new ArgumentOutOfRangeException(nameof(address), $"Register address must be between 0x{RegisterMinimum:X6} and 0x{RegisterMaximum:X6}.");
    }
}
