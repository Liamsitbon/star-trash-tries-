#!/usr/bin/env pwsh

Param(
    [Parameter(Mandatory=$false)]
    [Switch] $all,

    [Parameter(Mandatory=$false)]
    [String] $custom="",

    [Parameter(Mandatory=$false)]
    [String] $file=""
)

& adb get-state | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Error "No ADB device is available."
    exit $LASTEXITCODE
}

$arguments = @("logcat", "-v", "color")
if (-not $all) {
    $processId = (& adb shell pidof com.beatgames.beatsaber).Trim()
    if ([String]::IsNullOrWhiteSpace($processId)) {
        Write-Error "Beat Saber is not running. Start the game or pass -all."
        exit 1
    }
    $arguments += "--pid=$processId"
}
if (-not [String]::IsNullOrWhiteSpace($custom)) {
    $arguments += $custom
}

if ([String]::IsNullOrWhiteSpace($file)) {
    & adb @arguments
} else {
    & adb @arguments | Tee-Object -FilePath $file
}
exit $LASTEXITCODE
