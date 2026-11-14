// SPDX-License-Identifier: GPL-2.0-or-later
namespace Cxadc_ControlPanel.Models;

public sealed record CxadcDevice(string Path, string InstanceName, string PciLocation);

public sealed record CxadcConfiguration(
    bool Active,
    int VideoMux,
    int Level,
    int CenterOffset,
    bool EnableTenbit,
    bool EnableSixDB);

public sealed record CxadcState(
    bool Active,
    bool IsCapturing,
    int ReaderCount,
    int LastGpCount,
    int InitialPage,
    uint OverUnderflowCount);

public sealed record CxadcFunctions(bool Video, bool Audio, bool Mpeg, bool Infrared);

public enum ClockOutput
{
    Clock0 = 0,
    Clock1 = 1,
    Adc = 2
}

public sealed record ClockGenInfo(
    string DevicePath,
    string Serial,
    ushort FirmwareVersion,
    string ClockRate0,
    string ClockRate1,
    uint AdcRate,
    IReadOnlyList<string> ClockRates,
    IReadOnlyList<uint> AdcRates)
{
    public string DisplayName => string.IsNullOrWhiteSpace(Serial) ? DevicePath : Serial;
}
