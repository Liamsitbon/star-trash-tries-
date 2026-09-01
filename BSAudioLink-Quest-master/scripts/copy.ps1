#!/usr/bin/env pwsh

param(
    [Parameter(Mandatory=$false)]
    [Switch] $clean,

    [Parameter(Mandatory=$false)]
    [Switch] $release,

    [Parameter(Mandatory=$false)]
    [Switch] $log,

    [Parameter(Mandatory=$false)]
    [Switch] $useDebug,

    [Parameter(Mandatory=$false)]
    [Switch] $help
)

$ErrorActionPreference = "Stop"
$packageId = "com.beatgames.beatsaber"

if ($help) {
    Write-Output '"Copy" - builds AudioLink, pushes it to the Quest, and restarts Beat Saber.'
    Write-Output "`n-- Arguments --`n"
    Write-Output "-Clean       Performs a clean build"
    Write-Output "-Release     Builds RelWithDebInfo instead of Debug"
    Write-Output "-UseDebug    Pushes build/debug/<library> instead of the stripped build"
    Write-Output "-Log         Clears logcat before launch and streams logcat afterwards"
    exit 0
}

& $PSScriptRoot/build.ps1 -clean:$clean -release:$release
if ($LASTEXITCODE -ne 0) {
    Write-Error "AudioLink build failed."
    exit $LASTEXITCODE
}

# Generate mod.json from qpm.json/qpm.shared.json + mod.template.json. The old
# script called validate-modjson.ps1, but that file is not part of this source tree.
& qpm qmod manifest
if ($LASTEXITCODE -ne 0 -or -not (Test-Path "./mod.json")) {
    Write-Error "Could not generate mod.json with qpm."
    exit 1
}

$modJson = Get-Content "./mod.json" -Raw | ConvertFrom-Json

function Push-ModFiles($files, [string]$destination, [bool]$debugBuild) {
    if ($null -eq $files) { return }

    foreach ($fileName in @($files)) {
        if ([string]::IsNullOrWhiteSpace($fileName)) { continue }

        $source = if ($debugBuild) {
            Join-Path "build/debug" $fileName
        } else {
            Join-Path "build" $fileName
        }

        if (-not (Test-Path $source)) {
            throw "Expected mod file was not built: $source"
        }

        & adb push $source "$destination/$fileName"
        if ($LASTEXITCODE -ne 0) {
            throw "adb push failed for $source"
        }
    }
}

Push-ModFiles $modJson.modFiles "/sdcard/ModData/$packageId/Modloader/early_mods" $useDebug.IsPresent
Push-ModFiles $modJson.lateModFiles "/sdcard/ModData/$packageId/Modloader/mods" $useDebug.IsPresent

if ($log) {
    & adb logcat -c
}

# The old script referenced restart-game.ps1/start-logging.ps1, neither of which
# exists in this AudioLink source tree. Restart Beat Saber directly instead.
& adb shell am force-stop $packageId
if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to stop Beat Saber through adb."
    exit $LASTEXITCODE
}

& adb shell monkey -p $packageId -c android.intent.category.LAUNCHER 1 | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to launch Beat Saber through adb."
    exit $LASTEXITCODE
}

if ($log) {
    & adb logcat
}
