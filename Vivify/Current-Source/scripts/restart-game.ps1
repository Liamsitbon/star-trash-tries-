#!/usr/bin/env pwsh

Param(
    [Parameter(Mandatory=$false)]
    [String] $packageId="com.beatgames.beatsaber"
)

& adb get-state | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Error "No ADB device is available."
    exit $LASTEXITCODE
}

& adb shell am force-stop $packageId
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& adb shell monkey -p $packageId -c android.intent.category.LAUNCHER 1 | Out-Null
exit $LASTEXITCODE
