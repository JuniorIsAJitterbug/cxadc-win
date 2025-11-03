<#
.SYNOPSIS
    Local capture script for capture-server.

.DESCRIPTION
    Capture video, hifi and baseband audio with optional compression and resampling.

.LINK
    http://github.com/JuniorIsAJitterbug/cxadc-win

.NOTES
    Jitterbug <jitterbug@posteo.co.uk>

.INPUTS
    None.

.PARAMETER Name
    Specifies the capture name.

.PARAMETER AddDate
    Add date of capture to the file names.
    The date format is FileDateTime (yyyyMMddTHHmmssffff).

.PARAMETER Video
    The device number used for capturing the RF video signal.

.PARAMETER VideoBaseRate
    The sample rate of the video capture device. (DEFAULT: 40000)
    This is only used when compression is enabled.

.PARAMETER CompressVideo
    Enable FLAC compression for the video data.
    This will change the file extension of the video file to ".flac".

.PARAMETER VideoCompressionLevel
    Set the FLAC compression level for the video file. (DEFAULT: 4)

.PARAMETER Hifi
    The device number used for capturing the RF hifi signal.

.PARAMETER HifiBaseRate
    The rate of the hifi capture device. (DEFAULT: 40000)
    This is only used when compression or resampling is enabled.

.PARAMETER ResampleHifi
    Enable SoX resampling of the hifi signal.

.PARAMETER HifiResampleRate
    Set the target rate for SoX resampling. (DEFAULT: 10000)

.PARAMETER CompressHifi
    Enable FLAC compression for the hifi data.
    This will change the file extension of the hifi file to ".flac".

.PARAMETER HifiCompressionLevel
    Set the FLAC compression level for the hifi file. (DEFAULT: 4)

.PARAMETER Baseband
    Enable audio capture.

.PARAMETER BasebandRate
    Set the sample rate of the audio capture. (DEFAULT: 48000)
    This has to be supported by the device.

.PARAMETER ConvertBaseband
    Convert the audio data to a 2-channel FLAC file (L+R) and a 1-channel u8 file (HSW).
    Without conversion the output file is 3-channel s24le.

.PARAMETER FlacThreadCount
    Set the number of threads each "flac.exe" instance can use. (DEFAULT: 4)

.PARAMETER FFmpegThreadCount
    Set the number of threads each "ffmpeg.exe" instance can use. (DEFAULT: 4)

.PARAMETER UseSox
    Use SoX for resampling instead of FFmpeg.

.EXAMPLE
    PS> .\LocalCapture.ps1 -Name TestCapture -Video 0 -Baseband

    - Video data is captured from \\.\cxadc0
    - Audio is captured from a clockgen device

    Files
    TestCapture-video.u8
    TestCapture-baseband.s24

.EXAMPLE
    PS> .\LocalCapture.ps1 -Name TestCapture -Video 0 -CompressVideo -Baseband -BasebandRate 46875 -ConvertBaseband

    - Video data is captured from \\.\cxadc0 and is compressed
    - Audio is captured from a clockgen device with a sample rate of 46875 and is converted and compressed

    Files
    TestCapture-video.flac
    TestCapture-baseband.flac
    TestCapture-headswitch.u8


.EXAMPLE
    PS> .\LocalCapture.ps1 -Name TestCapture -AddDate -Video 0 -CompressVideo -Hifi 1 -CompressHifi -ResampleHifi -Baseband -BasebandRate 46875 -ConvertBaseband

    - Video data is captured from \\.\cxadc0 and is compressed
    - Hifi data is captured from \\.\cxadc1 and is resampled and compressed
    - Audio is captured from a clockgen device with a sample rate of 46875 and is converted and compressed
    - A timestamp is added to the file names

    Files
    TestCapture-20250601T1217596850-video.flac
    TestCapture-20250601T1217596850-hifi.flac
    TestCapture-20250601T1217596850-baseband.flac
    TestCapture-20250601T1217596850-headswitch.u8

.EXAMPLE
    PS> .\LocalCapture.ps1 -Name TestCapture -Video 0 -CompressVideo -VideoBaseRate 28636

    - Video data is captured from \\.\cxadc0 and is compressed using a sample rate of 28636
    - This is required if not using a clockgen device and the card is unmodified

    Files
    TestCapture-video.flac
#>

#Requires -Version 7.4
#Requires -PSEdition Core

param(
    [string] $Name,
    [switch] $AddDate = $false,

    [int] $Video,
    [int] $VideoBaseRate = 40000,
    [switch] $CompressVideo = $false,
    [int] $VideoCompressionLevel = 4,

    [int] $Hifi,
    [int] $HifiBaseRate = 40000,
    [switch] $ResampleHifi = $false,
    [int] $HifiResampleRate = 10000,
    [switch] $CompressHifi = $false,
    [int] $HifiCompressionLevel = 0,

    [switch] $Baseband = $false,
    [int] $BasebandRate = 48000,
    [switch] $ConvertBaseband = $false,

    [int] $FlacThreadCount = 4,
    [int] $FFmpegThreadCount = 4,
    [switch] $UseSox = $false
)

enum DeviceType {
    CxDevice
    BasebandDevice
}

enum CxCaptureType {
    Video
    Hifi
}

class DeviceData {
    [ValidateNotNullOrEmpty()][DeviceType] $DeviceType
    [ValidateNotNullOrEmpty()][string] $Name
    [ValidateNotNullOrEmpty()][string] $FilePrefix
    [ValidateNotNullOrEmpty()][int] $Rate
    [ValidateNotNullOrEmpty()][int] $FFmpegThreadCount
    [ValidateNotNullOrEmpty()][boolean] $UseSox
    [string[]] $OutputFiles
}

class CxDeviceData : DeviceData {
    [ValidateNotNullOrEmpty()][int] $Index
    [ValidateNotNullOrEmpty()][CxCaptureType] $Type
    [ValidateNotNullOrEmpty()][boolean] $EnableCompression
    [ValidateNotNullOrEmpty()][int] $CompressionLevel
    [ValidateNotNullOrEmpty()][boolean] $EnableResampling
    [ValidateNotNullOrEmpty()][int] $ResampleRate
    [ValidateNotNullOrEmpty()][int] $FlacThreadCount
}

class BasebandDeviceData : DeviceData {
    [ValidateNotNullOrEmpty()][string] $HeadSwitchFileName
    [ValidateNotNullOrEmpty()][boolean] $EnableConversion
}

class BinaryPaths {
    [ValidateNotNullOrEmpty()][string] $Server
    [ValidateNotNullOrEmpty()][string] $Curl
    [string] $Flac
    [string] $Sox
    [string] $FFmpeg
}

Class CxCaptureServer {
    [string] $Socket
    [BinaryPaths] $BinaryPaths
    [string] $BaseUrl = "http://capture-server"
    [boolean] $IsRunning = $false

    $ServerJob
    $CaptureJobs = @()

    CxCaptureServer([string] $Socket, [BinaryPaths] $BinaryPaths) {
        $this.Socket = $Socket
        $this.BinaryPaths = $BinaryPaths
    }

    [void] StartLocalServer() {
        $JobName = "ServerJob"
        $Command = @(
            $this.BinaryPaths.Server,
            ("unix:" + $this.Socket)
        ) -join " "

        $this.ServerJob = Start-Job -Name $JobName -ScriptBlock {
           Invoke-Expression ($using:Command)
        }
    }

    [void] StopLocalServer() {
        Stop-Job $this.ServerJob
        Remove-Job $this.ServerJob
    }

    [boolean] TestConnection() {
        if ((Invoke-WebRequest -UnixSocket $this.Socket -Method Get $this.BaseUrl).StatusCode -eq 200) {
            return $true
        }

        return $false
    }

    [int] GetOverflows() {
        $Url = $this.BaseUrl + "/stats"
        $Json = Invoke-WebRequest -Method Get -UnixSocket $this.Socket $Url | ConvertFrom-Json
        return $Json.overflows
    }

    [boolean] StartCapture($Devices) {
        [string[]] $Params = @()

        ForEach ($Device in $Devices) {
            $Params += $Device.Name

            if ($Device -is [BasebandDeviceData]) {
                $Params += ("lrate=" + $Device.Rate)
            }
        }

        $Url = $this.BaseUrl + "/start?" + ($Params -Join '&')
        $Json = Invoke-WebRequest -Method Get -UnixSocket $this.Socket $Url | ConvertFrom-Json

        if ($Json.state -eq "Running") {
            $this.IsRunning = $true
            return $true
        }
        
        if ($Json | Get-Member "fail_reason") {
            throw "server responded with `"" + $Json.fail_reason + "`" when attempting to start"
        }

        return $false
    }

    [int] StopCapture() {
        if (!$this.IsRunning) {
            return 0
        }

        $Url = $this.BaseUrl + "/stop"
        $Json = Invoke-WebRequest -Method Get -UnixSocket $this.Socket $Url | ConvertFrom-Json

        if ($Json.state -ne "Idle") {
            throw "unable to stop server"
        }

        Wait-Job $this.CaptureJobs -Timeout 10

        return $Json.overflows
    }

    [void] ForceStopCapture() {
        Stop-Job $this.CaptureJobs
        Remove-Job $this.CaptureJobs
    }

    [void] CaptureCx([CxDeviceData] $Device) {
        $JobName = "CaptureCxJob" + $Device.Index
        $CurlCommand = @(
            "&", $this.BinaryPaths.Curl, "-s",
            "-X", "GET",
            "--unix-socket", $this.Socket,
            "--url", ($this.BaseUrl + "/cxadc?" + $Device.Index),
            "-o"
        )
        $CurlOut = ($Device.FilePrefix + ".u8")
        $ResamplerCommand = ""
        $FlacCommand = ""
        $OutputFiles = @()

        if ($Device.EnableCompression) {
            $CurlOut = "-"
            $FlacOut = ($Device.FilePrefix + ".flac")
            $FlacRate = ($Device.EnableResampling) ? $Device.ResampleRate : $Device.Rate
            $FlacCommand = @(
                "|", "&", $this.BinaryPaths.Flac, "-s", "-f",
                "-j", $Device.FlacThreadCount
                ("-" + $Device.CompressionLevel), "-b", 65535, "--lax",
                ("--sample-rate=" + $FlacRate), "--channels=1", "--bps=8", "--sign=unsigned", "--endian=little",
                "-",
                "-o", $FlacOut
            )
            $OutputFiles += $FlacOut
        }

        if ($Device.EnableResampling) {
            $CurlOut = "-"
            $SoxOut = $FlacCommand -ne "" ? "-" : ($Device.FilePrefix + ".u8")
            $OutputFiles += $SoxOut

            if ($Device.UseSox) {
                $ResamplerCommand = @(
                    "|", "&", $this.BinaryPaths.Sox,
                    "-D",
                    "-t", "raw", "-r", $Device.Rate, "-b", 8, "-c", 1, "-L", "-e", "unsigned-integer",
                    "-",
                    "-t", "raw", "-b", 8, "-c", 1, "-L", "-e", "unsigned-integer",
                    $SoxOut, "rate", "-l", $Device.ResampleRate
                )
            } else {
                $ResamplerCommand = @(
                    "|", "&", $this.BinaryPaths.FFmpeg,
                    "-hide_banner", "-y", "-loglevel", "error",
                    "-threads", $Device.FFmpegThreadCount,
                    "-thread_queue_size", 1024,
                    "-ar", $Device.Rate, "-f", "u8",
                    "-i", "-",
                    "-filter_complex", ("aresample=resampler=soxr:precision=15,aformat=sample_fmts=u8:sample_rates=" + $Device.ResampleRate),
                    "-f", "u8", $SoxOut
                )
            }
        }

        $OutputFiles += $CurlOut
        $Device.OutputFiles = ($OutputFiles | Where-Object { $_ -ne "-" })
        $Command = ($CurlCommand + $CurlOut + $ResamplerCommand + $FlacCommand) -join " "

        $Job = Start-Job -Name $JobName -ScriptBlock {
           Invoke-Expression ($using:Command)
           
            if ($LastErrorCode -ne 0) {
                throw
            }
        }
        
        $this.CaptureJobs += $Job
    }

    [void] CaptureBaseband([BasebandDeviceData] $Device) {
        $JobName = "CaptureBasebandJob"
        $CurlOut = ($Device.FilePrefix + ".s24")
        $CurlCommand = @(
            "&", $this.BinaryPaths.Curl, "-s",
            "-X", "GET",
            "--unix-socket", $this.Socket,
            "--url", ($this.BaseUrl + "/baseband"),
            "-o"
        )
        $FFmpegCommand = ""
        $OutputFiles = @()

        if ($Device.EnableConversion) {
            $CurlOut = "-"
            $FFmpegOutBaseband = ($Device.FilePrefix + ".flac")
            $FFmpegOutHeadSwitch = ($Device.HeadSwitchFileName + ".u8")
            $FFmpegCommand = @(
                "|", "&", $this.BinaryPaths.FFmpeg,
                "-hide_banner", "-y", "-loglevel", "error",
                "-threads", $Device.FFmpegThreadCount,
                "-ar", $Device.Rate, "-ac", 3, "-f", "s24le",
                "-i", "-",
                "-filter_complex", "`"[0:a]channelsplit=channel_layout=2.1[FL][FR][headswitch],[FL][FR]amerge=inputs=2[baseband]`"",
                "-map", "`"[baseband]`"", "-compression_level", 0, $FFmpegOutBaseband,
                "-map", "`"[headswitch]`"", "-f", "u8", $FFmpegOutHeadSwitch
            )

            $OutputFiles += $FFmpegOutBaseband
            $OutputFiles += $FFmpegOutHeadSwitch
        }

        $OutputFiles += $CurlOut
        $Device.OutputFiles = ($OutputFiles | Where-Object { $_ -ne "-" })
        $Command = ($CurlCommand + $CurlOut + $FFmpegCommand) -join " "

        $Job = Start-Job -Name $JobName {
            Invoke-Expression ($using:Command)

            if ($LastErrorCode -ne 0) {
                throw
            }
        }
        
        $this.CaptureJobs += $Job
    }
}

if (!$PSBoundParameters.ContainsKey("Name")) {
    Write-Error "no name provided"
    exit
}

[string] $BasePath = $Name

if ($PSBoundParameters.ContainsKey("AddDate")) {
    $BasePath += ("-" + (Get-Date -Format FileDateTime))
}

# Create devices
[DeviceData[]] $Devices = @()

if ($PSBoundParameters.ContainsKey("Video")) {
    $Devices += [CxDeviceData]@{
        Index = $Video
        Type = [CxCaptureType]::Video
        Name = ("cxadc" + $Video)
        FilePrefix = ($BasePath + "-video")
        Rate = $VideoBaseRate
        EnableCompression = $CompressVideo
        CompressionLevel = $VideoCompressionLevel
        FlacThreadCount = $FlacThreadCount
        FFmpegThreadCount = $FFmpegThreadCount
        EnableResampling = $false
    }
}

if ($PSBoundParameters.ContainsKey("Hifi")) {
    $Devices += [CxDeviceData]@{
        Index = $Hifi
        Type = [CxCaptureType]::Hifi
        Name = ("cxadc" + $Hifi)
        FilePrefix = ($BasePath + "-hifi")
        Rate = $HifiBaseRate
        EnableCompression = $CompressHifi
        CompressionLevel = $HifiCompressionLevel
        EnableResampling = $ResampleHifi
        ResampleRate = $HifiResampleRate
        FlacThreadCount = $FlacThreadCount
        FFmpegThreadCount = $FFmpegThreadCount
        UseSox = $UseSox
    }
}

if ($Baseband) {
    $Devices += [BasebandDeviceData]@{
        Name = "baseband"
        FilePrefix = ($BasePath + "-baseband")
        HeadSwitchFileName = ($BasePath + "-headswitch")
        Rate = $BasebandRate
        EnableConversion = $ConvertBaseband
        FFmpegThreadCount = $FFmpegThreadCount
    }
}

if ($Devices.Count -eq 0) {
    Write-Error "No devices selected"
    exit
}

$FindBinary = {
    param([string] $Name)

    $Fullname = ($Name + ".exe")
    $TCResult = (Test-Path (".\" + $Fullname))

    if ($TCResult -eq $true) {
        return (".\`"" + $Fullname + "`"")
    }

    $GCResult = (Get-Command $Fullname -ErrorAction SilentlyContinue)

    if ($GCResult -ne $null) {
        return ("`"" + $GCResult.Path + "`"")
    }

    throw ($Name + " not found in path")
}

# check path for binaries
try {
    $BinaryPaths = [BinaryPaths]@{
        Server = $FindBinary.Invoke("capture-server")
        Curl = $FindBinary.Invoke("curl")
    }

    if ($CompressVideo -or $CompressHifi) {
        $BinaryPaths.Flac = $FindBinary.Invoke("flac")
    }

    if ($ResampleHifi -and $UseSox) {
        $BinaryPaths.Sox = $FindBinary.Invoke("sox")
    }

    if ($Baseband -or ($ResampleHifi -and !$UseSox)) {
        $BinaryPaths.FFmpeg = $FindBinary.Invoke("ffmpeg")
    }
} catch {
    Write-Error $_
    exit
}

$SocketPath = ((Get-Location).Path + "\.capture-server.sock")
$CxCaptureServer = [CxCaptureServer]::new($SocketPath, $BinaryPaths)

try {
    $CxCaptureServer.StartLocalServer()
    Start-Sleep -Milliseconds 1000
    Receive-Job $CxCaptureServer.ServerJob

    if (!$CxCaptureServer.TestConnection()) {
        Write-Error "capture server not responding"
        return
    }

    if (!$CxCaptureServer.StartCapture($Devices)) {
        Write-Error "capture server failed to start"
        return
    }
    
    ForEach ($Device in $Devices) {
        if ($Device -is [CxDeviceData]) {
            $CxCaptureServer.CaptureCx($Device)
        }
        
        if ($Device -is [BasebandDeviceData]) {
            $CxCaptureServer.CaptureBaseband($Device)
        }
    }

    $RunTimer = [Diagnostics.Stopwatch]::StartNew()
    $IsStopping = $false
    $ProgressWidth = 120
    $PSStyle.Progress.View = "Minimal"
    $PSStyle.Progress.Style = "`e[38;5;97m"
    $PSStyle.Progress.MaxWidth = $ProgressWidth
    $ProgressLoopLimitCount = 3

    if ($ResampleHifi) {
        Write-Warning "resampling may result in dropped samples."
    }

    Write-Host ("Starting `"" + $Name + "`" capture, press 'q' to stop")

    while ($True) {
        ForEach ($Job in $CxCaptureServer.CaptureJobs) {
            if ($Job.State -ne "Running" -and $IsStopping -eq $false) {
                $null = Receive-Job $Job
                throw ($Job.Name + " stopped unexpectedly")
            }
        }

        if ([Console]::KeyAvailable -and [Console]::ReadKey($true).Key -eq "q") {
            $IsStopping = $true
            break
        }

        Start-Sleep -Milliseconds 250

        if ($ProgressLoopLimitCount++ -ne 3) {
            continue
        }

        # print capture "progress"
        $Overflows = ("{0} overflows" -f $CxCaptureServer.GetOverflows()).PadRight(20)
        $Duration = ("{0,2} hour(s) {1,2} minute(s) {2,2} second(s)" -f $RunTimer.Elapsed.Hours, $RunTimer.Elapsed.Minutes, $RunTimer.Elapsed.Seconds)
        $StatusString = "{0}{1}" -f $Overflows, $Duration
        $OutputFileParentProgressParams = @{
                Id = 0
                Activity = "Capturing...".PadRight(57)
                Status = $StatusString.PadLeft(57)
        }
        Write-Progress @OutputFileParentProgressParams

        $ProgressId = 1

        ForEach ($Device in $Devices) {
            ForEach ($OutputFile in $Device.OutputFiles) {
                $CompressedString = ""
                $RateString = ""
                if ($Device -is [CxDeviceData]) {
                    $ActivityString = "{0} (\\.\cxadc{1})" -f `
                        (($Device.Type).ToString()), `
                        $Device.Index

                    if ($Device.EnableCompression) {
                        $CompressedString += "Compressed"
                    }

                    if ($Device.EnableCompression -or $Device.EnableResampling) {
                        $RateString += ("{0}Hz{1,11}" -f `
                            $Device.Rate, `
                            ($Device.EnableResampling ? ("-> {0}Hz" -f $Device.ResampleRate) : ""))
                    }
                } elseif ($Device -is [BasebandDeviceData]) { 
                    if ($OutputFile -match $Device.HeadSwitchFileName) {
                        $ActivityString = "Head Switch (ClockGen)"
                    } else {
                        $ActivityString = "Baseband (ClockGen)"
                        $CompressedString += "Compressed"
                    }
                }

                $OutputFileSizeBytes = (Get-Item $OutputFile -ErrorAction SilentlyContinue).Length
                $SizeString = "Size:{0,7:N2}{1}" -f `
                    ($OutputFileSizeBytes -ge 1TB ? ($OutputFileSizeBytes / 1TB) : `
                        ($OutputFileSizeBytes -ge 1GB ? ($OutputFileSizeBytes / 1GB) : ($OutputFileSizeBytes / 1MB))), `
                    ($OutputFileSizeBytes -ge 1TB ? "TB" : `
                        ($OutputFileSizeBytes -ge 1GB ? "GB" : "MB"))

                $StatusString = " {0,-12}{1,-30}{2,-13} " -f $CompressedString, $RateString, $SizeString

                $OutputFileProgressParams = @{
                    ParentId = 0
                    Id = $ProgressId
                    Activity = $ActivityString.PadRight(55)
                    Status = $StatusString
                }

                Write-Progress @OutputFileProgressParams
                $ProgressId += 1
            }
        }

        $ProgressLoopLimitCount = 0
    }
}
catch [System.Net.Sockets.SocketException] {
    Write-Error "server not responding"
}
catch [Microsoft.PowerShell.Commands.HttpResponseException] {
    Write-Error "server responded with error"
}
catch {
    Write-Error $_
}
finally {
    0..5 | ForEach-Object { Write-Progress -Id $_ -Completed } # shouldn't be more than 5
    Write-Host "Waiting for writes to finish..."
    
    try {
        $Overflows = $CxCaptureServer.StopCapture()

        if ($Overflows -gt 0) {
            Write-Warning "capture stopped with " + $Overflows + " overflows"
        }
    }
    catch {
        $CxCaptureServer.ForceStopCapture()
    }

    Write-Host "Files created:"
    ForEach ($Device in $Devices) {
        ForEach ($OutputFile in $Device.OutputFiles) {
            Write-Host ("  " + $OutputFile)
        }
    }

    Write-Host "Killing server"
    $CxCaptureServer.StopLocalServer()
}

Write-Host "Finished!"
