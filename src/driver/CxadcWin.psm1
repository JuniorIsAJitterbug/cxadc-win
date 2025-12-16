<#
.SYNOPSIS
    PS module for the cxadc-win driver.

.DESCRIPTION
    Configure device parameters and view device info via WMI.

.LINK
    http://github.com/JuniorIsAJitterbug/cxadc-win

.NOTES
    Copyright (C) 2024-2025 Jitterbug <jitterbug@posteo.co.uk>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, see
    <https://www.gnu.org/licenses/>.
#>

#Requires -Version 7.4
#Requires -PSEdition Core

class CxDevice {
    [string] $Path
    [string] $InstanceName
    [string] $Location
}

function Get-CxadcWinDevice {
    <#
        .SYNOPSIS
            Get cxadc device(s).

        .DESCRIPTION
            Get cxadc device(s) and their Win32 path.

        .PARAMETER Path
            Win32 path of device.

        .INPUTS
            None.

        .OUTPUTS
            CxDevice object.
    #>
    [OutputType([CxDevice])]
    Param(
        [string] $Path
    )
    
    $Devices = @()
    $DevicePaths = @()

    $GetPathParams = @{
        Namespace = "root/WMI"
        ClassName = "CxadcWin_Path"
    }

    $GetHardwareParams = @{
        Namespace = "root/WMI"
        ClassName = "CxadcWin_Hardware"
    }
    
    $Filter = $null

    if ($PSBoundParameters.ContainsKey("Path")) {
        $Filter = ("Path='{0}'" -f $Path.Replace("\", "\\"))
    }
        
    $Devicepaths = Get-CimInstance @GetPathParams -Filter $Filter

    if ($DevicePaths.Count -eq 0 -and $null -ne $Filter) {
        throw "Unable to find device " + $Path
    }
    
    foreach ($DevicePath in $DevicePaths) {
        $Hw = Get-CimInstance @GetHardwareParams -Filter ("InstanceName='{0}'" -f $DevicePath.InstanceName.Replace("\", "\\"))
    
        $Devices += [CxDevice]@{
            Path = $DevicePath.Path
            InstanceName = $DevicePath.InstanceName
            Location = ("{0:d2}:{1:d2}.{2:d1}" -f $Hw.PciBusNumber, (($Hw.PciBusAddress -shr 16) -band 0x0000FFFF), ($Hw.PciBusAddress -band 0x0000FFFF))
        }
    }
    
    $Devices
}

function Get-CxadcWinConfig {
    <#
        .SYNOPSIS
            Get cxadc device(s) configuration.

        .DESCRIPTION
            Get cxadc device(s) configuration via WMI.

        .PARAMETER Path
            Win32 path of device.

        .PARAMETER Device
            A CxDevice object.

        .INPUTS
            A CxDevice object can be piped to this command.

        .OUTPUTS
            CimInstance object containing configuration from WMI.
    #>
    [CmdletBinding(DefaultParameterSetName="PathSet")]
    [OutputType([CimInstance])]
    Param(
        [Parameter(ParameterSetName = "PathSet", Mandatory)]
        [string] $Path,

        [Parameter(ParameterSetName = "DeviceSet", Mandatory, ValueFromPipeline)]
        [CxDevice] $Device
    )

    $GetConfigParams = @{
        Namespace = "root/WMI"
        ClassName = "CxadcWin_Config"
    }

    if ($PSBoundParameters.ContainsKey("Path")) {
        $Device = Get-CxadcWinDevice -Path $Path
    }

    Get-CimInstance @GetConfigParams -Filter ("InstanceName='{0}'" -f $Device.InstanceName.Replace("\", "\\"))
}

function Get-CxadcWinState {
    <#
        .SYNOPSIS
            Get cxadc device(s) state.

        .DESCRIPTION
            Get cxadc device(s) state via WMI.

        .PARAMETER Path
            Win32 path of device.

        .PARAMETER Device
            A CxDevice object.

        .INPUTS
            A CxDevice object can be piped to this command.

        .OUTPUTS
            CimInstance object containing state from WMI.
    #>
    [CmdletBinding(DefaultParameterSetName="PathSet")]
    [OutputType([CimInstance])]
    Param(
        [Parameter(ParameterSetName = "PathSet", Mandatory)]
        [string] $Path,

        [Parameter(ParameterSetName = "DeviceSet", Mandatory, ValueFromPipeline)]
        [CxDevice] $Device
    )

    $GetStateParams = @{
        Namespace = "root/WMI"
        ClassName = "CxadcWin_State"
    }

    if ($PSBoundParameters.ContainsKey("Path")) {
        $Device = Get-CxadcWinDevice -Path $Path
    }

    Get-CimInstance @GetStateParams -Filter ("InstanceName='{0}'" -f $Device.InstanceName.Replace("\", "\\"))
}

function Set-CxadcWinConfig {
    <#
        .SYNOPSIS
            Set cxadc device(s) configuration.

        .DESCRIPTION
            Set cxadc device(s) configuration via WMI.

        .PARAMETER Path
            Win32 path of device.

        .PARAMETER Device
            A CxDevice object.

        .PARAMETER VideoMux
            Physical input to capture.

        .PARAMETER Level
            Digital gain applied by device.

        .PARAMETER EnableTenbit
            Enables 10-bit capture, instead of the default 8-bit.

        .PARAMETER EnableSixDB
            Apply +6dB gain to input signal.

        .PARAMETER CenterOffset
            DC offset for centering RF signal.

        .INPUTS
            A CxDevice object can be piped to this command.

        .OUTPUTS
            CimInstance object containing new configuration.
    #>
    [CmdletBinding(DefaultParameterSetName="PathSet")]
    [OutputType([CimInstance])]
    Param(
        [Parameter(ParameterSetName = "PathSet", Mandatory)]
        [string] $Path,

        [Parameter(ParameterSetName = "DeviceSet", Mandatory, ValueFromPipeline)]
        [CxDevice] $Device,

        [int] $VideoMux,
        [int] $Level,
        [boolean] $EnableTenbit,
        [boolean] $EnableSixDB,
        [int] $CenterOffset
    )

    $GetConfigParams = @{
        Namespace = "root/WMI"
        ClassName = "CxadcWin_Config"
    }

    try {
        if ($PSBoundParameters.ContainsKey("Path")) {
            $Device = Get-CxadcWinDevice -Path $Path
        }

        $Config = @{}

        if ($PSBoundParameters.ContainsKey("VideoMux")) {
            $Config.VideoMux = $VideoMux
        }

        if ($PSBoundParameters.ContainsKey("Level")) {
            $Config.Level = $Level
        }

        if ($PSBoundParameters.ContainsKey("EnableTenbit")) {
            $Config.EnableTenbit = $EnableTenbit
        }

        if ($PSBoundParameters.ContainsKey("EnableSixDB")) {
            $Config.EnableSixDB = $EnableSixDB
        }

        if ($PSBoundParameters.ContainsKey("CenterOffset")) {
            $Config.CenterOffset = $CenterOffset
        }

        $CimInstance = Get-CimInstance @GetConfigParams -Filter ("InstanceName='{0}'" -f $Device.InstanceName.Replace("\", "\\"))

        if ($null -eq $CimInstance) {
            throw "Error opening CxadcWin_Registers CimInstance"
        }

        Set-CimInstance -CimInstance $CimInstance -Property $Config -PassThru
    } catch {
        Write-Error $_
        return
    }
}

function Get-CxadcWinRegister {
    <#
        .SYNOPSIS
            Get cxadc device register(s).

        .DESCRIPTION
            Get cxadc device register(s) via WMI.

        .PARAMETER Path
            Win32 path of device.

        .PARAMETER Device
            A CxDevice object.

        .PARAMETER Addresses
            Memory addresses to read.

        .INPUTS
            A CxDevice object can be piped to this command.

        .OUTPUTS
            Object containing address value(s) in decimal, hexadecimal and binary.
    #>
    [CmdletBinding(DefaultParameterSetName="PathSet")]
    [OutputType([PSCustomObject[]])]
    Param(
        [Parameter(ParameterSetName = "PathSet", Mandatory)]
        [string] $Path,

        [Parameter(ParameterSetName = "DeviceSet", Mandatory, ValueFromPipeline)]
        [CxDevice] $Device,

        [Parameter(Mandatory)]
        [int[]] $Addresses
    )

    $GetRegisterParams = @{
        Namespace = "root/WMI"
        ClassName = "CxadcWin_Registers"
    }

    try {
        if ($PSBoundParameters.ContainsKey("Path")) {
            $Device = Get-CxadcWinDevice -Path $Path
        }

        $Return = [PSCustomObject[]]@()

        foreach ($Addr in $Addresses) {
            if (-not ($Addr -ge 0x200000 -and $Addr -le 0x5CFFFF)) {
                Write-Warning ("Address 0x{0:X8} ignored, out of range" -f $Addr)
                continue
            }

            $CimInstance = Get-CimInstance @GetRegisterParams -Filter ("InstanceName='{0}'" -f $Device.InstanceName.Replace("\", "\\"))

            if ($null -eq $CimInstance) {
                throw "Error opening CxadcWin_Registers CimInstance"
            }

            $Response = Invoke-CimMethod -CimInstance $CimInstance -MethodName Get -Arguments @{Address = $Addr}

            if ($null -eq $Response) {
                throw ("Error invoking Get(0x{0:X6})" -f $Addr)
            }

            $Return += [PSCustomObject]@{
                Address = $Addr
                Value = $Response.Value
                HexAddress = "0x{0:X6}" -f $Addr
                HexValue = "0x{0:X8}" -f $Response.Value
                BinaryValue = "0b" + ([System.Convert]::ToString($Response.Value, 2)).PadLeft(32, "0")
            }
        }

        $Return
    } catch {
        Write-Error $_
        return
    }
}

function Set-CxadcWinRegister {
    <#
        .SYNOPSIS
            Set cxadc device register.

        .DESCRIPTION
            Set cxadc device register via WMI.

        .PARAMETER Path
            Win32 path of device.

        .PARAMETER Device
            A CxDevice object.

        .PARAMETER Address
            Memory addresses to read.

        .PARAMETER Value
            Value to write to address.
            This can be in decimal or hexadecimal.

        .INPUTS
            A CxDevice object can be piped to this command.

        .OUTPUTS
            None.
    #>
    [CmdletBinding(DefaultParameterSetName="PathSet")]
    Param(
        [Parameter(ParameterSetName = "PathSet", Mandatory)]
        [string] $Path,

        [Parameter(ParameterSetName = "DeviceSet", Mandatory, ValueFromPipeline)]
        [CxDevice] $Device,

        [Parameter(Mandatory=$true)]
        [int] $Address,

        [Parameter(Mandatory=$true)]
        [int] $Value
    )

    $SetRegisterParams = @{
        Namespace = "root/WMI"
        ClassName = "CxadcWin_Registers"
    }

    try {
        if ($PSBoundParameters.ContainsKey("Path")) {
            $Device = Get-CxadcWinDevice -Path $Path
        }

        if (-not ($Address -ge 0x200000 -and $Address -le 0x5CFFFF)) {
            Write-Error ("Address 0x{0:X8} out of range" -f $Address)
            exit
        }

        $CimInstance = Get-CimInstance @SetRegisterParams -Filter ("InstanceName='{0}'" -f $Device.InstanceName.Replace("\", "\\"))

        if ($null -eq $CimInstance) {
            throw "Error opening CxadcWin_Registers CimInstance"
        }
        
        $null = Invoke-CimMethod -CimInstance $CimInstance -MethodName Set -Arguments @{Address = $Address; Value = $Value}
    } catch {
        Write-Error $_
        return
    }
}

[Flags()]
enum CxDeviceFunction {
    Audio = 0x02
    MPEG  = 0x04
    IR    = 0x10
    All   = 0x16
}

function Get-CxadcWinDeviceFunction {
    <#
        .SYNOPSIS
            Get cxadc device function status.

        .DESCRIPTION
            Get cxadc device function status by reading the EEPROM.

        .PARAMETER Path
            Win32 path of device.

        .PARAMETER Device
            A CxDevice object.

        .INPUTS
            A CxDevice object can be piped to this command.

        .OUTPUTS
            None.
    #>
    [CmdletBinding(DefaultParameterSetName="PathSet")]
    Param(
        [Parameter(ParameterSetName = "PathSet", Mandatory)]
        [string] $Path,

        [Parameter(ParameterSetName = "DeviceSet", Mandatory, ValueFromPipeline)]
        [CxDevice] $Device
    )

    try {
        if ($PSBoundParameters.ContainsKey("Path")) {
            $Device = Get-CxadcWinDevice -Path $Path
        }

        $FunctionAddress = 0x365000
        $Current = Get-CxadcWinRegister -Device $Device -Address $FunctionAddress

        Write-Warning "This may be inaccurate."

        [PSCustomObject]@{
            "Video" = ($Current.Value -band 0x01) -ne 0 # Should always be true
            "Audio" = ($Current.Value -band [CxDeviceFunction]::Audio) -ne 0
            "MPEG" = ($Current.Value -band [CxDeviceFunction]::MPEG) -ne 0
            "IR" = ($Current.Value -band [CxDeviceFunction]::IR) -ne 0
        }
    } catch {
        Write-Error $_
        return
    }
}

function Set-CxadcWinDeviceFunction {
    <#
        .SYNOPSIS
            Enable or disable cxadc device functions.

        .DESCRIPTION
            Enable or disable cxadc device function by writing to the EEPROM. This requires a reboot to apply.

        .PARAMETER Path
            Win32 path of device.

        .PARAMETER Device
            A CxDevice object.

        .PARAMETER Function
            Device function to enable.

        .INPUTS
            A CxDevice object can be piped to this command.

        .OUTPUTS
            None.
    #>
    [CmdletBinding(DefaultParameterSetName="PathSetEnable")]
    Param(
        [Parameter(ParameterSetName = "PathSetEnable", Mandatory)]
        [Parameter(ParameterSetName = "PathSetDisable", Mandatory)]
        [string] $Path,

        [Parameter(ParameterSetName = "DeviceSetEnable", Mandatory, ValueFromPipeline)]
        [Parameter(ParameterSetName = "DeviceSetDisable", Mandatory, ValueFromPipeline)]
        [CxDevice] $Device,

        [Parameter(ParameterSetName = "PathSetEnable", Mandatory)]
        [Parameter(ParameterSetName = "DeviceSetEnable")]
        [CxDeviceFunction] $Enable,

        [Parameter(ParameterSetName = "PathSetDisable", Mandatory)]
        [Parameter(ParameterSetName = "DeviceSetDisable")]
        [CxDeviceFunction] $Disable,

        [switch] $Force
    )

    try {
        if ($PSBoundParameters.ContainsKey("Path")) {
            $Device = Get-CxadcWinDevice -Path $Path
        }

        if ($PSBoundParameters.ContainsKey("Enable")) {
            $Function = $Enable
            $IsEnable = $true
        } elseif ($PSBoundParameters.ContainsKey("Disable")) {
            $Function = $Disable
            $IsEnable = $false
        }

        $FunctionAddress = 0x365000
        $Current = Get-CxadcWinRegister -Device $Device -Address $FunctionAddress

        switch ($Function) {
            "Audio" { $FunctionValue = [CxDeviceFunction]::Audio }
            "MPEG"  { $FunctionValue = [CxDeviceFunction]::MPEG }
            "IR"    { $FunctionValue = [CxDeviceFunction]::IR }
            "All"   { $FunctionValue = [CxDeviceFunction]::All }
            default { $FunctioNValue = 0x00 }
        }

        if ($IsEnable) {
            $Value = $Current.Value -bor $FunctionValue
        } else {
            $Value = $Current.Value -band (-bnot $FunctionValue)
        }

        $Caption = "The following action will write data to the EEPROM, if something goes wrong the card may be bricked."
        $Query = "Do you take full responsbility and wish to continue?"

        if ($Force -or $PSCmdlet.ShouldContinue($Query, $Caption)) {
            Set-CxadcWinRegister -Device $Device -Address $FunctionAddress -Value $Value
            Write-Warning "A reboot is required for changes to apply."
        }
    } catch {
        Write-Error $_
        return
    }
}

Export-ModuleMember -Function Get-CxadcWinDevice
Export-ModuleMember -Function Get-CxadcWinConfig
Export-ModuleMember -Function Get-CxadcWinState
Export-ModuleMember -Function Set-CxadcWinConfig
Export-ModuleMember -Function Get-CxadcWinRegister
Export-ModuleMember -Function Set-CxadcWinRegister
Export-ModuleMember -Function Get-CxadcWinDeviceFunction
Export-ModuleMember -Function Set-CxadcWinDeviceFunction
