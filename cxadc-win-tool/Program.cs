// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * cxadc-win-tool - Example tool for using the cxadc-win driver
 *
 * Copyright (C) 2024-2025 Jitterbug <jitterbug@posteo.co.uk>
 */

using cxadc_win_tool;
using System.Buffers.Binary;
using System.CommandLine;
using System.CommandLine.Parsing;
using System.Text;

CancellationTokenSource cancelTokenSrc = new();
CancellationToken cancelToken = cancelTokenSrc.Token;

Console.CancelKeyPress += (sender, e) =>
{
    cancelTokenSrc.Cancel();
    e.Cancel = true;
};

// commandline handling
// global args
var inputDeviceArg = new Argument<string>(name: "device", description: "device path");


// capture command
var captureOutputArg = new Argument<string>(name: "output", description: "output path (- for STDOUT)");
var captureCommand = new Command("capture", description: "capture data")
{
    inputDeviceArg,
    captureOutputArg,
};

captureCommand.AddAlias("cap");

captureCommand.SetHandler(async (device, output) =>
{
    using var cx = new CxadcWin(device);
    using var outputStream = output == "-" ? Console.OpenStandardOutput(CxadcWin.ReadBufferSize)
        : new FileStream(output, FileMode.Create, FileAccess.Write, FileShare.Write, CxadcWin.ReadBufferSize, true);

    await cx.Read(outputStream, cancelToken);

}, inputDeviceArg, captureOutputArg);


// get command
var getCommand = new Command("get", description: "get device options")
{
    inputDeviceArg,
};

getCommand.SetHandler((device) =>
{
    using var cx = new CxadcWin(device);
    cx.PrintConfig();

}, inputDeviceArg);


// set command
var setNameArg = new Argument<string>("name").FromAmong("vmux", "level", "tenbit", "sixdb", "center_offset");
var setValueArg = new Argument<uint>("value");
var setCommand = new Command("set", description: "set device options")
{
    inputDeviceArg,
    setNameArg,
    setValueArg
};

setCommand.SetHandler((device, name, value) =>
{
    using var cx = new CxadcWin(device);
    var code = name switch
    {
        "vmux" => CxadcWin.IoctlCode.SetVMux,
        "level" => CxadcWin.IoctlCode.SetLevel,
        "tenbit" => CxadcWin.IoctlCode.SetTenbit,
        "sixdb" => CxadcWin.IoctlCode.SetSixDB,
        "center_offset" => CxadcWin.IoctlCode.SetCenterOffset,
        _ => throw new Exception("Unknown option")
    };

    cx.Set(code, value);

}, inputDeviceArg, setNameArg, setValueArg);


// register command
var registerAddressArg = new Argument<string>("address");
var registerValueArg = new Argument<string>("value");

var registerSetCommand = new Command("set")
{
    inputDeviceArg,
    registerAddressArg,
    registerValueArg
};

registerSetCommand.SetHandler((device, address, value) =>
{
    var buffer = new byte[8];
    BinaryPrimitives.WriteUInt32LittleEndian(buffer, Convert.ToUInt32(address, 16));
    BinaryPrimitives.WriteUInt32LittleEndian(buffer.AsSpan()[4..], Convert.ToUInt32(value, 16));

    using var cx = new CxadcWin(device);
    cx.Set(CxadcWin.IoctlCode.SetRegister, buffer);

}, inputDeviceArg, registerAddressArg, registerValueArg);

var registerGetCommand = new Command("get")
{
    inputDeviceArg,
    registerAddressArg
};

registerGetCommand.SetHandler((device, address) =>
{
    var buffer = new byte[4];
    BinaryPrimitives.WriteUInt32LittleEndian(buffer, Convert.ToUInt32(address, 16));

    using var cx = new CxadcWin(device);
    var value = cx.Get(CxadcWin.IoctlCode.GetRegister, buffer);

    Console.WriteLine($"{value}");
    Console.WriteLine($"0x{value:X8}");
    Console.WriteLine($"0b{Convert.ToString(value, 2).PadLeft(32, '0')}");

}, inputDeviceArg, registerAddressArg);

var registerCommand = new Command("register", description: "get/set registers")
{
    registerGetCommand,
    registerSetCommand
};
registerCommand.AddAlias("reg");


// reset command
var resetNameArg = new Argument<string>("name").FromAmong("ouflow_count");
var resetCommand = new Command("reset", description: "reset device state")
{
    inputDeviceArg,
    resetNameArg
};

resetCommand.SetHandler((device, name) =>
{
    using var cx = new CxadcWin(device);

    var code = name switch
    {
        "ouflow_count" => CxadcWin.IoctlCode.ResetOUFlowCount,
        _ => throw new Exception("Unknown option")
    };

    cx.Set(code, 0);

}, inputDeviceArg, resetNameArg);


// leveladj command
var levelAdjStartingLevelOption = new Option<uint>(name: "--level", description: "starting level", getDefaultValue: () => 16);
var levelAdjSampleCountOption = new Option<uint>(name: "--samples", getDefaultValue: () => CxadcWin.VBIBufferSize + CxadcWin.ReadBufferSize);
var levelAdjOutOfRangeLimitOption = new Option<uint>(name: "--oor-limit", getDefaultValue: () => 20);
var levelAdjVerifyPassesOption = new Option<uint>(name: "--verify-passes", getDefaultValue: () => 3);
var levelAdjCommand = new Command("leveladj", "automatic level adjustment")
{
    inputDeviceArg,
    levelAdjStartingLevelOption,
    levelAdjSampleCountOption,
    levelAdjVerifyPassesOption
};

levelAdjCommand.SetHandler(async (device, startingLevel, sampleCount, oorLimit, verifyPasses) =>
{
    using var cx = new CxadcWin(device);
    await cx.LevelAdjust(startingLevel, startingLevel, sampleCount, oorLimit, cancelToken);

},
inputDeviceArg,
levelAdjStartingLevelOption,
levelAdjSampleCountOption,
levelAdjOutOfRangeLimitOption,
levelAdjVerifyPassesOption);


// clockgen command
var clockgenCxClockIndexArg = new Argument<uint>("clock").FromAmong("0", "1");
var clockgenCxClockValueArg = new Argument<uint>("value", description: "1 = 20.00 MHz, 2 = 28.636 MHz, 3 = 40.00 MHz, 4 = 50.000 MHz").FromAmong("1", "2", "3", "4");

var clockgenCxGetCommand = new Command("get", "get frequency")
{
    clockgenCxClockIndexArg
};

clockgenCxGetCommand.SetHandler((idx) =>
{
    using var clockgen = new Clockgen();
    Console.WriteLine($"{clockgen.GetClock(idx):0.000}");

}, clockgenCxClockIndexArg);

var clockgenCxSetCommand = new Command("set", "set frequency")
{
    clockgenCxClockIndexArg,
    clockgenCxClockValueArg
};

clockgenCxSetCommand.SetHandler((idx, valueIdx) =>
{
    using var clockgen = new Clockgen();
    var currentFreqIdx = clockgen.GetFreqIdx(idx);

    // stepped change of clock to prevent crash
    while (valueIdx != currentFreqIdx)
    {
        currentFreqIdx += (byte)(valueIdx < currentFreqIdx ? -1 : 1);
        clockgen.SetClock(idx, currentFreqIdx);
        Thread.Sleep(100);
    }

}, clockgenCxClockIndexArg, clockgenCxClockValueArg);

var clockgenCxCommand = new Command("cx", "configure cx rates")
{
    clockgenCxGetCommand,
    clockgenCxSetCommand
};

var clockgenAudioRateArg = new Argument<int>("set").FromAmong("46875", "48000");

var clockgenAudioRateSetCommand = new Command("set")
{
    clockgenAudioRateArg
};

clockgenAudioRateSetCommand.SetHandler((rate) =>
{
    using var clockgen = new Clockgen();
    clockgen.SetAudioRate(rate);

}, clockgenAudioRateArg);

var clockgenAudioRateGetCommand = new Command("get");

clockgenAudioRateGetCommand.SetHandler(() =>
{
    using var clockgen = new Clockgen();
    Console.WriteLine(clockgen.GetAudioRate());

});

var clockgenAudioCommand = new Command("audio", "configure audio rate")
{
    clockgenAudioRateSetCommand,
    clockgenAudioRateGetCommand
};

var clockgenCommand = new Command("clockgen", "configure clockgen")
{
    clockgenCxCommand,
    clockgenAudioCommand
};


// scan command
var scanCommand = new Command("scan", "list devices");

scanCommand.SetHandler(() =>
{
    foreach (var device in CxadcWin.EnumerateDevices())
    {
        using var cx = new CxadcWin(device);
        Console.WriteLine(Encoding.Unicode.GetString(cx.Get(CxadcWin.IoctlCode.GetWin32Path, 128, new byte[128])));
    }

});


// status command
var statusCommand = new Command("status", "show all device config");

statusCommand.SetHandler(() =>
{
    foreach (var device in CxadcWin.EnumerateDevices())
    {
        using var cx = new CxadcWin(device);
        cx.PrintConfig();
    }

    if (Clockgen.Exists())
    {
        using var clockgen = new Clockgen();
        clockgen.PrintConfig();
    }

});


// root
var rootCommand = new RootCommand("cxadc-win-tool - https://github.com/JuniorIsAJitterbug/cxadc-win")
{
    statusCommand,
    scanCommand,
    captureCommand,
    getCommand,
    setCommand,
    resetCommand,
    registerCommand,
    clockgenCommand,
    levelAdjCommand
};

await rootCommand.InvokeAsync(args);
