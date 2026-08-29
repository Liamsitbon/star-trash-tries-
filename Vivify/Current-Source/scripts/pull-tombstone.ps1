#!/usr/bin/env pwsh

Param(
    [Parameter(Mandatory=$false)]
    [String] $output="./tombstone.txt"
)

& adb get-state | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Error "No ADB device is available."
    exit $LASTEXITCODE
}

$remote = (& adb shell "ls -t /data/tombstones/tombstone_* 2>/dev/null | head -n 1").Trim()
if ([String]::IsNullOrWhiteSpace($remote)) {
    Write-Error "No readable Android tombstone was found. Reproduce the crash and try again."
    exit 1
}

& adb pull $remote $output
exit $LASTEXITCODE
