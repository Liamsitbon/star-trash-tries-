#!/usr/bin/env pwsh

Param(
    [Parameter(Mandatory=$false)]
    [Switch] $clean,

    [Parameter(Mandatory=$false)]
    [Switch] $useDebug,

    [Parameter(Mandatory=$false)]
    [Switch] $log,

    [Parameter(Mandatory=$false)]
    [String] $file="",

    [Parameter(ValueFromRemainingArguments=$true)]
    [String[]] $remainingArguments
)

& $PSScriptRoot/build.ps1 -clean:$clean
if ($LASTEXITCODE -ne 0) {
    Write-Error "Vivify build failed; nothing was copied."
    exit $LASTEXITCODE
}

& adb get-state | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Error "No ADB device is available."
    exit $LASTEXITCODE
}

& $PSScriptRoot/validate-modjson.ps1
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$modJson = Get-Content "./mod.json" -Raw | ConvertFrom-Json
$earlyDirectory = "/sdcard/ModData/$($modJson.packageId)/Modloader/early_mods"
$lateDirectory = "/sdcard/ModData/$($modJson.packageId)/Modloader/mods"
& adb shell mkdir -p $earlyDirectory $lateDirectory
if ($LASTEXITCODE -ne 0) {
    Write-Error "Could not create the mod directories on the Quest."
    exit $LASTEXITCODE
}

foreach ($fileName in $modJson.modFiles) {
    $source = if ($useDebug) { "./build/debug/$fileName" } else { "./build/$fileName" }
    & adb push $source "$earlyDirectory/$fileName"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

foreach ($fileName in $modJson.lateModFiles) {
    $source = if ($useDebug) { "./build/debug/$fileName" } else { "./build/$fileName" }
    & adb push $source "$lateDirectory/$fileName"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

& $PSScriptRoot/restart-game.ps1
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($log) {
    & $PSScriptRoot/start-logging.ps1 -file $file
}
