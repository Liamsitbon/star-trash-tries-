#!/usr/bin/env pwsh

Param(
    [Parameter(Mandatory=$false)]
    [ValidateRange(5, 600)]
    [Int] $durationSeconds=45,

    [Parameter(Mandatory=$false)]
    [ValidateRange(250, 5000)]
    [Int] $intervalMilliseconds=1000,

    [Parameter(Mandatory=$false)]
    [String] $output="",

    [Parameter(Mandatory=$false)]
    [String] $label="quest3-vivify-performance"
)

$ErrorActionPreference = "Stop"
$packageId = "com.beatgames.beatsaber"
$adbCommand = Get-Command adb -ErrorAction SilentlyContinue
if ($null -eq $adbCommand) {
    Write-Error "adb was not found on PATH. Install Android platform-tools or add adb to PATH."
    exit 1
}
$adb = $adbCommand.Source

function Invoke-AdbText {
    Param([String[]] $arguments)
    return ((& $adb @arguments 2>&1 | Out-String).Trim())
}

& $adb get-state | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Error "No ADB device is available. Connect the Quest, enable USB debugging, and accept the headset prompt."
    exit $LASTEXITCODE
}

$safeLabel = ($label -replace '[^A-Za-z0-9._-]', '-')
$timestamp = Get-Date -Format "yyyy-MM-dd-HHmmss"
if ([String]::IsNullOrWhiteSpace($output)) {
    $output = Join-Path (Get-Location) "diagnostics/$timestamp-$safeLabel"
}
$output = [IO.Path]::GetFullPath($output)
New-Item -ItemType Directory -Path $output -Force | Out-Null

$model = Invoke-AdbText @("shell", "getprop", "ro.product.model")
$device = Invoke-AdbText @("shell", "getprop", "ro.product.device")
$hardware = Invoke-AdbText @("shell", "getprop", "ro.hardware")
$fingerprint = Invoke-AdbText @("shell", "getprop", "ro.build.fingerprint")
$androidRelease = Invoke-AdbText @("shell", "getprop", "ro.build.version.release")
$sdk = Invoke-AdbText @("shell", "getprop", "ro.build.version.sdk")
$serial = Invoke-AdbText @("get-serialno")
$requestedRefreshRate = Invoke-AdbText @("shell", "getprop", "debug.oculus.refreshRate")
$requestedCpuLevel = Invoke-AdbText @("shell", "getprop", "debug.oculus.cpuLevel")
$requestedGpuLevel = Invoke-AdbText @("shell", "getprop", "debug.oculus.gpuLevel")
$requestedFoveation = Invoke-AdbText @("shell", "getprop", "debug.oculus.foveation.level")

@"
model=$model
device=$device
hardware=$hardware
androidRelease=$androidRelease
sdk=$sdk
serial=$serial
fingerprint=$fingerprint
requestedRefreshRate=$requestedRefreshRate
requestedCpuLevel=$requestedCpuLevel
requestedGpuLevel=$requestedGpuLevel
requestedFoveation=$requestedFoveation
durationSeconds=$durationSeconds
intervalMilliseconds=$intervalMilliseconds
"@ | Set-Content -Path (Join-Path $output "device-summary.txt") -Encoding utf8

if ($model -notmatch 'Quest 3') {
    Write-Warning "Connected model reports '$model'. The collector will continue, but label the result with the actual headset."
}

$pidText = Invoke-AdbText @("shell", "pidof", $packageId)
$pidCandidates = @($pidText -split '\s+' | Where-Object { $_ -match '^\d+$' })
if ($pidCandidates.Count -eq 0) {
    Write-Error "Beat Saber is not running. Launch it, enter the target Vivify map, then run this collector."
    exit 2
}
$processId = $pidCandidates[0]

Invoke-AdbText @("shell", "dumpsys", "meminfo", $packageId) |
    Set-Content -Path (Join-Path $output "memory-before.txt") -Encoding utf8
Invoke-AdbText @("shell", "dumpsys", "thermalservice") |
    Set-Content -Path (Join-Path $output "thermal-before.txt") -Encoding utf8
Invoke-AdbText @("shell", "dumpsys", "gfxinfo", $packageId, "framestats") |
    Set-Content -Path (Join-Path $output "gfxinfo-before.txt") -Encoding utf8
Invoke-AdbText @("shell", "dumpsys", "display") |
    Set-Content -Path (Join-Path $output "display.txt") -Encoding utf8

$samplePath = Join-Path $output "samples.csv"
"hostTime,pid,processCpuPercent,rssKb,vszKb,gpuBusyTicks,gpuTotalTicks,gpuBusyPercent,gpuClockHz,maxThermalMilliC" |
    Set-Content -Path $samplePath -Encoding utf8
$rawSamplePath = Join-Path $output "samples-raw.txt"

$deadline = (Get-Date).AddSeconds($durationSeconds)
while ((Get-Date) -lt $deadline) {
    $livePid = Invoke-AdbText @("shell", "pidof", $packageId)
    if ($livePid -notmatch "(^|\s)$processId(\s|$)") {
        "Beat Saber process $processId exited during collection at $((Get-Date).ToString('o'))." |
            Set-Content -Path (Join-Path $output "process-exited.txt") -Encoding utf8
        break
    }

    $processSample = Invoke-AdbText @(
        "shell", "sh", "-c",
        "toybox ps -p $processId -o PID,CPU,RSS,VSZ,NAME 2>/dev/null | tail -n 1"
    )
    $gpuBusy = Invoke-AdbText @(
        "shell", "sh", "-c",
        "cat /sys/class/kgsl/kgsl-3d0/gpubusy 2>/dev/null || true"
    )
    $gpuClock = Invoke-AdbText @(
        "shell", "sh", "-c",
        "cat /sys/class/kgsl/kgsl-3d0/devfreq/cur_freq 2>/dev/null || cat /sys/class/kgsl/kgsl-3d0/gpuclk 2>/dev/null || true"
    )
    $thermalZones = Invoke-AdbText @(
        "shell", "sh", "-c",
        'for z in /sys/class/thermal/thermal_zone*/temp; do cat "$z" 2>/dev/null; done | tr "\n" ":"'
    )

    $processFields = @($processSample -split '\s+' | Where-Object { $_ -ne '' })
    $processCpuPercent = ""
    $rssKb = ""
    $vszKb = ""
    if ($processFields.Count -ge 4 -and $processFields[0] -match '^\d+$') {
        $processCpuPercent = $processFields[1]
        $rssKb = $processFields[2]
        $vszKb = $processFields[3]
    }

    $gpuBusyTicks = ""
    $gpuTotalTicks = ""
    $gpuBusyPercent = ""
    $gpuFields = @($gpuBusy -split '\s+' | Where-Object { $_ -match '^\d+$' })
    [Long] $parsedGpuBusyTicks = 0
    [Long] $parsedGpuTotalTicks = 0
    if ($gpuFields.Count -ge 2 -and
        [Long]::TryParse($gpuFields[0], [ref] $parsedGpuBusyTicks) -and
        [Long]::TryParse($gpuFields[1], [ref] $parsedGpuTotalTicks)) {
        $gpuBusyTicks = $parsedGpuBusyTicks
        $gpuTotalTicks = $parsedGpuTotalTicks
        if ($parsedGpuTotalTicks -gt 0) {
            $gpuBusyPercent = [String]::Format(
                [Globalization.CultureInfo]::InvariantCulture,
                "{0:F3}", (100.0 * $parsedGpuBusyTicks / $parsedGpuTotalTicks))
        }
    }

    $gpuClockHz = ""
    [Long] $parsedGpuClockHz = 0
    $gpuClockFields = @($gpuClock -split '\s+' | Where-Object { $_ -match '^\d+$' })
    if ($gpuClockFields.Count -gt 0 -and
        [Long]::TryParse($gpuClockFields[0], [ref] $parsedGpuClockHz)) {
        $gpuClockHz = $parsedGpuClockHz
    }
    $thermalValues = @(
        $thermalZones -split ':' |
            Where-Object { $_ -match '^-?\d+$' } |
            ForEach-Object { [Long] $_ }
    )
    $maxThermalMilliC = if ($thermalValues.Count -gt 0) {
        ($thermalValues | Measure-Object -Maximum).Maximum
    } else {
        ""
    }

    $csvFields = @(
        (Get-Date).ToString("o"),
        $processId,
        $processCpuPercent,
        $rssKb,
        $vszKb,
        $gpuBusyTicks,
        $gpuTotalTicks,
        $gpuBusyPercent,
        $gpuClockHz,
        $maxThermalMilliC
    )
    ($csvFields -join ',') | Add-Content -Path $samplePath -Encoding utf8
    "[$((Get-Date).ToString('o'))] ps=[$processSample] gpubusy=[$gpuBusy] gpuclock=[$gpuClock] thermal=[$thermalZones]" |
        Add-Content -Path $rawSamplePath -Encoding utf8
    Start-Sleep -Milliseconds $intervalMilliseconds
}

Invoke-AdbText @("shell", "dumpsys", "meminfo", $packageId) |
    Set-Content -Path (Join-Path $output "memory-after.txt") -Encoding utf8
Invoke-AdbText @("shell", "dumpsys", "thermalservice") |
    Set-Content -Path (Join-Path $output "thermal-after.txt") -Encoding utf8
Invoke-AdbText @("shell", "dumpsys", "gfxinfo", $packageId, "framestats") |
    Set-Content -Path (Join-Path $output "gfxinfo-after.txt") -Encoding utf8
Invoke-AdbText @("shell", "dumpsys", "SurfaceFlinger") |
    Set-Content -Path (Join-Path $output "surfaceflinger-after.txt") -Encoding utf8
Invoke-AdbText @("shell", "dumpsys", "activity", "processes", $packageId) |
    Set-Content -Path (Join-Path $output "activity-process-after.txt") -Encoding utf8
Invoke-AdbText @("logcat", "-d", "-v", "threadtime", "Vivify:I", "Unity:I", "AndroidRuntime:E", "DEBUG:E", "*:S") |
    Set-Content -Path (Join-Path $output "logcat-filtered.txt") -Encoding utf8

$vivifyLog = "/sdcard/ModData/$packageId/Logs/Vivify.log"
& $adb shell test -f $vivifyLog
if ($LASTEXITCODE -eq 0) {
    & $adb pull $vivifyLog (Join-Path $output "Vivify.log") | Out-Null
}

$hashPaths = @(
    "/sdcard/ModData/$packageId/Modloader/mods/libVivify.so",
    "/sdcard/ModData/$packageId/Packages/1.40.8_7379/vivify_v0.6.8/libVivify.so"
)
$hashPath = Join-Path $output "installed-vivify-hashes.txt"
foreach ($remote in $hashPaths) {
    Invoke-AdbText @("shell", "sha256sum", $remote) | Add-Content -Path $hashPath -Encoding utf8
}

$zipPath = "$output.zip"
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
Compress-Archive -Path (Join-Path $output "*") -DestinationPath $zipPath -CompressionLevel Optimal

Write-Output "Quest performance capture: $output"
Write-Output "Share this ZIP: $zipPath"
