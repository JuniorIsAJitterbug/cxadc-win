<#
.SYNOPSIS
    PS module for configuring cxadc clock generator.

.DESCRIPTION
    Configure cxadc clock generator outputs.

.LINK
    http://github.com/JuniorIsAJitterbug/cxadc-win

.NOTES
    Jitterbug <jitterbug@posteo.co.uk>
#>

#Requires -Version 7.4
#Requires -PSEdition Core

using namespace Microsoft.Win32.SafeHandles
using namespace System.Buffers.Binary
using namespace System.IO
using namespace System.Management.Automation
using namespace System.Runtime.InteropServices

enum CxOutput {
    Clock0 = 0
    Clock1 = 1
    ADC = 2
}

enum UsbRequestType {
    VendorOut = 0x40 # Device Recipient + Vendor + Out
    VendorIn = 0xC0 # Device Recipient + Vendor + In
}

enum UsbRequest {
    CxRateIdx = 0x10
    CxRateOptions = 0x11
    AdcRate = 0x12
    AdcRateOptions = 0x13
}

enum UsbDescriptorType {
    Device = 1
    String = 3
}

enum DiControlFlags {
    Default = 0x01
    Present = 0x02
    AllClasses = 0x04
    Profile = 0x08
    DeviceInterface = 0x10
}

enum FormatMessageFlags {
    IgnoreInserts = 0x0200
    FromString = 0x0400
    FromHModule = 0x0800
    FromSystem = 0x1000
    ArgumentArray = 0x2000
    MaxWidthMask = 0x00FF
}

Class WinUsbWrapper {
    [SafeFileHandle] $DeviceHandle = $null
    [IntPtr] $UsbHandle = [IntPtr]::Zero
    [string] $DevicePath

    WinUsbWrapper([string] $DeviceClassGuid) {
        [Guid] $DeviceClass = [Guid]::Empty
    
        if ([Guid]::TryParse($DeviceClassGuid, [ref] $DeviceClass) -eq $false) {
            throw "Error parsing guid"
        }
    }

    [void] OpenDevice([string] $DevicePath) {
        $this.DevicePath = $DevicePath

        $this.DeviceHandle = ([type]"Win32")::CreateFile(
            $this.DevicePath,
            [FileAccess]::ReadWrite,
            [FileShare]::ReadWrite,
            [IntPtr]::Zero,
            [FileMode]::Open,
            [FileOptions]::Asynchronous,
            [IntPtr]::Zero)

        if ($this.DeviceHandle -eq -1) {
            throw ("Error opening device: " + [WinUsbWrapper]::GetErrorString([Marshal]::GetLastWin32Error()))
        }

        [IntPtr] $UsbHandle_ = [IntPtr]::Zero

        if (([type]"WinUsb")::WinUsb_Initialize($this.DeviceHandle, [ref] $UsbHandle_) -eq $false) {
            throw ("Error initializing WinUSB: " + [WinUsbWrapper]::GetErrorString([Marshal]::GetLastWin32Error()))
        }

        $this.UsbHandle = $UsbHandle_
    }

    
    [void] CloseDevice() {
        if ($this.UsbHandle -ne [IntPtr]::Zero) {
            ([type]"WinUsb")::WinUsb_Free($this.UsbHandle)
            $this.UsbHandle = [IntPtr]::Zero
        }

        if (($this.DeviceHandle -ne -1) -and $null -ne $this.DeviceHandle) {
            ([type]"Win32")::CloseHandle($this.DeviceHandle)
            $this.DeviceHandle = $null
        }
    }

    [CxClockgenData] GetDevice() {
        return @{
            DevicePath = $this.DevicePath
            ClockRate0 = $this.GetCxRateOptions()[$this.GetCxRateIndex([CxOutput]::Clock0)]
            ClockRate1 = $this.GetCxRateOptions()[$this.GetCxRateIndex([CxOutput]::Clock1)]
            AdcRate = $this.GetAdcRate()
            Serial = $this.GetDeviceSerial()
            FirmwareVersion = $this.GetDeviceFirmwareVersion()
        }
    }

    [byte] GetCxRateIndex([CxOutput] $DeviceIndex) {
        $Buffer = New-Object -TypeName byte[] -ArgumentList 1
        $this.SendControlpacket(
            [UsbRequest]::CxRateIdx,
            [UsbRequestType]::VendorIn,
            $DeviceIndex,
            $Buffer)

        return $Buffer[0]
    }

    [void] SetCxRateIndex([CxOutput] $OutputIndex, [byte] $RateIndex) {
        $Buffer = New-Object -TypeName byte[] -ArgumentList 1
        $Buffer[0] = $RateIndex
        $this.SendControlpacket(
            [UsbRequest]::CxRateIdx,
            [UsbRequestType]::VendorOut,
            $OutputIndex,
            $Buffer)
    }

    [void] SetCxRate([CxOutput] $OutputIndex, [string] $Rate) {
        $Options = $this.GetCxRateOptions()
        [byte] $OptionIndex = 0

        foreach ($Option in $Options) {
            if ($Rate -like $Option) {
                $this.SetCxRateIndex($OutputIndex, $OptionIndex)
                return
            }

            $OptionIndex += 1
        }

        throw ("Invalid rate " + $Rate)
    }

    [string[]] GetCxRateOptions() {
        $Buffer = New-Object -TypeName byte[] -ArgumentList 255
        $Response = $this.SendControlpacket(
            [UsbRequest]::CxRateOptions,
            [UsbRequestType]::VendorIn,
            [CxOutput]::Clock0, # Clock0 and Clock1 share options
            $Buffer)
    
        # convert byte[] to ushort[]
        $RateIdxCount = ($Response.Length / 2)
        $RateIdxs = New-Object -TypeName ushort[] -ArgumentList $RateIdxCount
        [System.Buffer]::BlockCopy($Response, 0, $RateIdxs, 0, $Response.Length)

        $Rates = @()

        foreach($RateIdx in $RateIdxs) {
            $Transferred = 0
            $StringDescriptor = New-Object -TypeName USB_STRING_DESCRIPTOR
            $Ret = ([type]"WinUsb")::WinUsb_GetStringDescriptor(
                $this.UsbHandle,
                [UsbDescriptorType]::String,
                $RateIdx,
                0x00,
                [ref] $StringDescriptor,
                [Marshal]::SizeOf($StringDescriptor),
                [ref] $Transferred)

            if ($Ret -eq $false) {
                throw ("Error getting device descriptor: " + [WinUsbWrapper]::GetErrorString([Marshal]::GetLastWin32Error()))
            }

            $Rates += $StringDescriptor.bString
        }

        return $Rates
    }

    [uint] GetAdcRate() {
        $Buffer = New-Object -TypeName byte[] -ArgumentList 4
        $this.SendControlpacket(
            [UsbRequest]::AdcRate,
            [UsbRequestType]::VendorIn,
            [CxOutput]::Adc,
            $Buffer)

        return [BinaryPrimitives]::ReadUInt32LittleEndian($Buffer)
    }

    [void] SetAdcRate([uint] $Rate) {
        $Buffer = New-Object -TypeName byte[] -ArgumentList 4
        [BinaryPrimitives]::WriteUInt32LittleEndian($Buffer, $Rate) 
        $this.SendControlpacket(
            [UsbRequest]::AdcRate,
            [UsbRequestType]::VendorOut,
            [CxOutput]::Adc,
            $Buffer)
    }

    [uint[]] GetAdcRateOptions() {
        $Buffer = New-Object -TypeName byte[] -ArgumentList 255
        $Response = $this.SendControlpacket(
            [UsbRequest]::AdcRateOptions,
            [UsbRequestType]::VendorIn,
            [CxOutput]::Adc,
            $Buffer)

        # convert byte[] to uint[]
        $Rates = New-Object -TypeName uint[] -ArgumentList ($Response.Length / 4)
        [System.Buffer]::BlockCopy($Response, 0, $Rates, 0, $Response.Length)
    
        return $Rates
    }

    [ushort] GetDeviceFirmwareVersion() {
        $DeviceDescriptor = New-Object -TypeName USB_DEVICE_DESCRIPTOR
        $Transferred = 0
        $Ret = ([type]"WinUsb")::WinUsb_GetDeviceDescriptor(
            $this.UsbHandle,
            [UsbDescriptorType]::Device, 0x00, 0x00,
            [ref] $DeviceDescriptor,
            [Marshal]::SizeOf($DeviceDescriptor),
            [ref] $Transferred)

        if ($Ret -eq $false) {
            throw ("Error getting device descriptor: " + [WinUsbWrapper]::GetErrorString([Marshal]::GetLastWin32Error()))
        }

        return $DeviceDescriptor.bcdDevice
    }

    [string] GetDeviceSerial() {
        $DeviceDescriptor = New-Object -TypeName USB_DEVICE_DESCRIPTOR
        $Transferred = 0
        $Ret = ([type]"WinUsb")::WinUsb_GetDeviceDescriptor(
            $this.UsbHandle,
            [UsbDescriptorType]::Device,
            0x00,
            0x00,
            [ref] $DeviceDescriptor,
            [Marshal]::SizeOf($DeviceDescriptor),
            [ref] $Transferred)

        if ($Ret -eq $false) {
            throw ("Error getting device descriptor: " + [WinUsbWrapper]::GetErrorString([Marshal]::GetLastWin32Error()))
        }

        $StringDescriptor = New-Object -TypeName USB_STRING_DESCRIPTOR
        $Ret = ([type]"WinUsb")::WinUsb_GetStringDescriptor(
            $this.UsbHandle,
            [UsbDescriptorType]::String,
            $DeviceDescriptor.iSerialNumber,
            0x00,
            [ref] $StringDescriptor,
            [Marshal]::SizeOf($StringDescriptor),
            [ref] $Transferred)

        if ($Ret -eq $false) {
            throw ("Error getting device descriptor: " + [WinUsbWrapper]::GetErrorString([Marshal]::GetLastWin32Error()))
        }

        return $StringDescriptor.bString
    }

    [byte[]] hidden SendControlpacket([UsbRequest] $Request, [UsbRequestType] $RequestType, [CxOutput] $DeviceIndex, [byte[]] $Buffer) {
        $PacketParameters = @{
            TypeName = 'WINUSB_SETUP_PACKET'
            Property = @{
                Request = $Request
                RequestType = $RequestType
                Index = $DeviceIndex
                Length = $Buffer.Length
            }
        }

        $Packet = New-Object @PacketParameters
        $Transferred = 0
        $Ret = ([type]"WinUsb")::WinUsb_ControlTransfer(
            $this.UsbHandle,
            $Packet,
            $Buffer,
            $Buffer.Length,
            [ref] $Transferred,
            [IntPtr]::Zero)

        if ($Ret -eq $false) {
            throw ("Error sending control packet: " + [WinUsbWrapper]::GetErrorString([Marshal]::GetLastWin32Error()))
        }

        # resize returned array to # of transferred bytes
        [System.Array]::Resize([ref] $Buffer, $Transferred)
        
        return $Buffer
    }

    [string[]] static GetDevicePaths([Guid] $DeviceClass) {
        # get device info set
        $DeviceInfoSet = -1
        $DeviceIndex = 0
        $Paths = @()

        try {
            $DeviceInfoSet = ([type]"SetupApi")::SetupDiGetClassDevs(
                [ref]$DeviceClass,
                [IntPtr]::Zero,
                [IntPtr]::Zero,
                [DiControlFlags]::AllClasses -bor [DiControlFlags]::DeviceInterface -bor [DiControlFlags]::Present)

            if ($DeviceInfoSet -eq -1) {
                throw ("Error getting device info set: " + [WinUsbWrapper]::GetErrorString([Marshal]::GetLastWin32Error()))
            }

            # enumerate device interfaces
            while ($true) {
                $DeviceInterfaceData = New-Object -TypeName SP_DEVICE_INTERFACE_DATA
                $DeviceInterfaceData.cbSize = [Marshal]::SizeOf($DeviceInterfaceData)

                if (([type]"SetupApi")::SetupDiEnumDeviceInterfaces(
                    $DeviceInfoSet,
                    $null,
                    [ref] $DeviceClass,
                    $DeviceIndex,
                    [ref] $DeviceInterfaceData) -eq $false) {
                    # no more items
                    if ([Marshal]::GetLastWin32Error() -eq 259) {
                        if ($DeviceIndex -eq 0) {
                            throw ("No devices found")
                        }

                        break
                    }
                    
                    throw ("Error enumerating device interfaces: " + [WinUsbWrapper]::GetErrorString([Marshal]::GetLastWin32Error()))
                }

                # get device detail data
                $DeviceDetailData = New-Object -TypeName SP_DEVICE_INTERFACE_DETAIL_DATA
                $DeviceDetailData.cbSize = [IntPtr]::Size -eq 8 ? 8 : 6

                if (([type]"SetupApi")::SetupDiGetDeviceInterfaceDetail(
                    $DeviceInfoSet,
                    [ref] $DeviceInterfaceData,
                    [ref] $DeviceDetailData,
                    [Marshal]::SizeOf($DeviceDetailData),
                    [IntPtr]::Zero,
                    [IntPtr]::Zero) -eq $false) {
                    throw ("Error getting device detail data: " + [WinUsbWrapper]::GetErrorString([Marshal]::GetLastWin32Error()))
                }

                $Paths += $DeviceDetailData.DevicePath
                $DeviceIndex += 1
            }
        } finally {
            if ($DeviceInfoSet -ne -1) {
                ([type]"SetupApi")::SetupDiDestroyDeviceInfoList($DeviceInfoSet)
            }
        }

        return $Paths
    }

    [string] static GetErrorString([int] $MessageId) {
        $Buffer = New-Object -TypeName byte[] -ArgumentList 1024

        ([type]"Win32")::FormatMessage(
            [FormatMessageFlags]::FromSystem -bor [FormatMessageFlags]::IgnoreInserts -bor [FormatMessageFlags]::MaxWidthMask,
            [IntPtr]::Zero,
            $MessageId,
            0, $Buffer,
            $Buffer.Length,
            [IntPtr]::Zero)

        return [System.Text.Encoding]::Ascii.GetString($Buffer).Trim([char] 0)
    }

    [void] static InitNative() {
        try { ([type]"SetupApi") } catch {
            Add-Type -TypeDefinition @"
                using System;
                using System.Runtime.InteropServices;

                public static class SetupApi {
                    [DllImport("setupapi.dll", SetLastError = true)]
                    public static extern IntPtr SetupDiGetClassDevs(
                        ref Guid ClassGuid,
                        IntPtr Enumerator,
                        IntPtr hwndParent,
                        int Flags);

                    [DllImport("setupapi.dll", SetLastError = true)]
                    public static extern bool SetupDiDestroyDeviceInfoList(
                        IntPtr DeviceInfoSet);

                    [DllImport("setupapi.dll", SetLastError = true)]
                    public static extern bool SetupDiEnumDeviceInterfaces(
                        IntPtr lpDeviceInfoSet,
                        uint nDeviceInfoData,
                        ref Guid gClass,
                        uint nIndex,
                        ref SP_DEVICE_INTERFACE_DATA oInterfaceData);

                    [DllImport("setupapi.dll", SetLastError = true)]
                    public static extern bool SetupDiGetDeviceInterfaceDetail(
                        IntPtr lpDeviceInfoSet,
                        ref SP_DEVICE_INTERFACE_DATA oInterfaceData,
                        ref SP_DEVICE_INTERFACE_DETAIL_DATA oDetailData,
                        uint nDeviceInterfaceDetailDataSize,
                        IntPtr nRequiredSize,
                        IntPtr lpDeviceInfoData);
                }

                [StructLayout(LayoutKind.Sequential, Pack = 1)]
                public struct SP_DEVINFO_DATA {
                    public uint cbSize;
                    public Guid classGuid;
                    public uint devInst;
                    public IntPtr reserved;
                }  

                [StructLayout(LayoutKind.Sequential, Pack = 1)]
                public struct SP_DEVICE_INTERFACE_DATA {
                    public int cbSize;
                    public Guid InterfaceClassGuid;
                    public int Flags;
                    public IntPtr Reserved;
                }  

                [StructLayout(LayoutKind.Sequential, Pack = 1)]
                public struct SP_DEVICE_INTERFACE_DETAIL_DATA {
                    public int cbSize;
                    
                    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 512)]
                    public string DevicePath;
                }
"@
        }

        try { ([type]"Win32") } catch {
            Add-Type -TypeDefinition @"
                using System;
                using System.IO;
                using System.Runtime.InteropServices;
                using Microsoft.Win32.SafeHandles;

                public static class Win32 {
                    [DllImport("kernel32.dll", SetLastError = true)]
                    public static extern SafeFileHandle CreateFile(
                        string lpFileName,
                        FileAccess dwDesiredAccess,
                        FileShare dwShareMode,
                        IntPtr securityAttributes,
                        FileMode dwCreationDisposition,
                        FileOptions dwFlagsAndAttributes,
                        IntPtr hTemplateFile);
                        
                    [DllImport("kernel32.dll", SetLastError = true)]
                    public static extern SafeFileHandle CloseHandle(
                        SafeFileHandle hObject);

                    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
                    public static extern uint FormatMessage(
                        uint dwFlags,
                        IntPtr lpSource,
                        uint dwMessageId,
                        uint dwLanguageId,
                        byte[] lpBuffer,
                        uint nSize,
                        IntPtr Arguments);
                }
"@
        }

        try { ([type]"WinUsb") } catch {
            Add-Type -TypeDefinition @"
                using System;
                using System.Runtime.InteropServices;
                using Microsoft.Win32.SafeHandles;
                        
                public static class WinUsb {
                    [DllImport("winusb.dll", SetLastError = true)]
                    public static extern bool WinUsb_Initialize(
                        SafeFileHandle DeviceHandle,
                        ref IntPtr InterfaceHandle);
                
                    [DllImport("winusb.dll", SetLastError = true)]
                    public static extern bool WinUsb_Free(
                        IntPtr InterfaceHandle);
                
                    [DllImport("winusb.dll", SetLastError = true)]
                    public static extern bool WinUsb_ControlTransfer(
                        IntPtr InterfaceHandle,
                        WINUSB_SETUP_PACKET SetupPacket,
                        byte[] Buffer,
                        uint BufferLength,
                        ref uint LengthTransferred,
                        IntPtr Overlapped);

                    [DllImport("winusb.dll", EntryPoint = "WinUsb_GetDescriptor", SetLastError = true)]
                    public static extern bool WinUsb_GetDeviceDescriptor(
                        IntPtr InterfaceHandle,
                        byte DescriptorType,
                        byte Index,
                        ushort LanguageID,
                        ref USB_DEVICE_DESCRIPTOR Buffer,
                        uint BufferLength,
                        ref uint LengthTransferred);

                    [DllImport("winusb.dll", EntryPoint = "WinUsb_GetDescriptor", SetLastError = true)]
                    public static extern bool WinUsb_GetStringDescriptor(
                        IntPtr InterfaceHandle,
                        byte DescriptorType,
                        byte Index,
                        ushort LanguageID,
                        ref USB_STRING_DESCRIPTOR Buffer,
                        uint BufferLength,
                        ref uint LengthTransferred);
                }

                [StructLayout(LayoutKind.Sequential, Pack = 1)]
                public struct WINUSB_SETUP_PACKET {
                    public byte RequestType;
                    public byte Request;
                    public ushort Value;
                    public ushort Index;
                    public ushort Length;
                }

                [StructLayout(LayoutKind.Sequential, Pack = 1)]
                public struct USB_DEVICE_DESCRIPTOR {
                    public byte bLength;
                    public byte bDescriptorType;
                    public ushort bcdUSB;
                    public byte bDeviceClass;
                    public byte bDeviceSubClass;
                    public byte bDeviceProtocol;
                    public byte bMaxPacketSize0;
                    public ushort idVendor;
                    public ushort idProduct;
                    public ushort bcdDevice;
                    public byte iManufacturer;
                    public byte iProduct;
                    public byte iSerialNumber;
                    public byte bNumConfigurations;
                }

                [StructLayout(LayoutKind.Sequential, Pack = 1, CharSet = CharSet.Unicode)]
                public struct USB_STRING_DESCRIPTOR {
                    public byte bLength;
                    public byte bDescriptorType;
                    
                    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 512)]
                    public string bString;
                } 
"@
        }
    }
}

class CxClockgenData {
    [string] $DevicePath
    [string] $ClockRate0
    [string] $ClockRate1
    [uint] $AdcRate
    [string] $Serial
    [ushort] $FirmwareVersion
}

$ClockgenGuid = "9DED1FD1-C739-4D2A-A1E5-E060342724DE"

<#
    .SYNOPSIS
        Get cxadc clock generator device(s).

    .DESCRIPTION
        Get cxadc clock generator device(s) and their configuration.

    .PARAMETER Path
        Device path.
        This is should start with \\?\usb#...

    .INPUTS
        None.

    .OUTPUTS
        CxClockgenData object.
#>
function Get-CxadcClockGenDevice() {
    [OutputType([CxClockgenData[]])]
    param(
        [string] $Path
    )

    begin {
        [WinUsbWrapper]::InitNative()
        $WinUsb = [WinUsbWrapper]::new($ClockgenGuid)
    }

    process {
        [string[]] $Paths = @()
        [CxClockgenData[]] $Devices = @()

        if ($PSBoundParameters.ContainsKey("Path")) {
            $Paths = @($Path)
        } else {
            $Paths = [WinUsbWrapper]::GetDevicePaths($ClockgenGuid)
        }

        foreach ($DevicePath in $Paths) {
            try {
                $WinUsb.OpenDevice($DevicePath)
                $Devices += $WinUsb.GetDevice()
            } finally {
                $WinUsb.CloseDevice()
            }
        }

        $Devices
    }
}

<#
    .SYNOPSIS
        Set output clock rate.

    .DESCRIPTION
        Set output clock rate of a cxadc clock generator device.

    .PARAMETER Path
        Device path.
        This is should start with \\?\usb#...

    .PARAMETER Device
        A CxClockgenData object.

    .PARAMETER Output
        Clock output to set.

    .PARAMETER Rate
        Rate to set.
        View valid rates with Get-CxadcClockGenRateOptions.

    .INPUTS
        A CxClockgenData object can be piped to this command.

    .OUTPUTS
        CxClockgenData object containing updated configuration.
#>
function Set-CxadcClockGenRate {
    [CmdletBinding(DefaultParameterSetName="PathSet")]
    [OutputType([CxClockgenData])]
    Param(
        [Parameter(ParameterSetName = "PathSet", Mandatory)]
        [string] $Path,

        [Parameter(ParameterSetName = "DeviceSet", Mandatory, ValueFromPipeline)]
        [CxClockgenData] $Device,

        [Parameter(Mandatory)]
        [CxOutput] $Output,

        [Parameter(Mandatory)]
        [string] $Rate
    )

    begin {
        [WinUsbWrapper]::InitNative()
        $WinUsb = [WinUsbWrapper]::new($ClockgenGuid)
    }

    clean {
        $WinUsb.CloseDevice()
    }

    process {
        if ($PSBoundParameters.ContainsKey("Path")) {
            $Device = Get-CxadcClockGenDevice -Path $Path
        }

        $WinUsb.OpenDevice($Device.DevicePath)

        switch ($Output) {
            ([CxOutput]::Clock0) { $WinUsb.SetCxRate($Output, $Rate) }
            ([CxOutput]::Clock1) { $WinUsb.SetCxRate($Output, $Rate) }
            ([CxOutput]::ADC) { $WinUsb.SetAdcRate($Rate) }
        }

        return $WinUsb.GetDevice()
    }
}

<#
    .SYNOPSIS
        Get valid output clock rates.

    .DESCRIPTION
        Get valid output clock rates of a cxadc clock generator device.

    .PARAMETER Path
        Device path.
        This is should start with \\?\usb#...

    .PARAMETER Device
        A CxClockgenData object.

    .INPUTS
        A CxClockgenData object can be piped to this command.

    .OUTPUTS
        Object containing valid rates for each output.
#>
function Get-CxadcClockGenRateOptions() {
    [CmdletBinding(DefaultParameterSetName="PathSet")]
    [OutputType([CxClockgenData])]
    Param(
        [Parameter(ParameterSetName = "PathSet", Mandatory)]
        [string] $Path,

        [Parameter(ParameterSetName = "DeviceSet", Mandatory, ValueFromPipeline)]
        [CxClockgenData] $Device
    )

    begin {
        [WinUsbWrapper]::InitNative()
        $WinUsb = [WinUsbWrapper]::new($ClockgenGuid)
    }

    clean {
        $WinUsb.CloseDevice()
    }

    process {
        if ($PSBoundParameters.ContainsKey("Path")) {
            $Device = Get-CxadcClockGenDevice -Path $Path
        }

        $WinUsb.OpenDevice($Device.DevicePath)
        $CxOptions = $WinUsb.GetCxRateOptions()
        $AdcOptions = $WinUsb.GetAdcRateOptions()

        $Options = @(
            [PSCustomObject]@{
                Output = ([CxOutput]::ADC)
                Values = $AdcOptions
            },
            [PSCustomObject]@{
                Output = ([CxOutput]::Clock0)
                Values = $CxOptions
            },
            [PSCustomObject]@{
                Output = ([CxOutput]::Clock1)
                Values = $CxOptions
            }
        )

        $Options
    }    
}

Export-ModuleMember -Function Get-CxadcClockGenDevice
Export-ModuleMember -Function Set-CxadcClockGenRate
Export-ModuleMember -Function Get-CxadcClockGenRateOptions
