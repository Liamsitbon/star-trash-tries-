#!/usr/bin/env pwsh

Param(
    [Parameter(Mandatory=$false)] [Switch]$clean,
    [Parameter(Mandatory=$false)] [Switch]$release,
    [Parameter(Mandatory=$false)] [Switch]$debugBuild,
    [Parameter(Mandatory=$false)] [String]$ndk
)

$ErrorActionPreference = "Stop"
$ProjectRoot = (Resolve-Path "$PSScriptRoot/..").Path
Set-Location $ProjectRoot

if ($clean.IsPresent -and (Test-Path "build")) {
    Remove-Item "build" -Recurse -Force
}

function Test-NdkPath([String]$Path) {
    return $Path -and (Test-Path (Join-Path $Path "build/cmake/android.toolchain.cmake"))
}

$NDKPath = $ndk
if (-not (Test-NdkPath $NDKPath)) { $NDKPath = $Env:ANDROID_NDK_HOME }
if (-not (Test-NdkPath $NDKPath)) { $NDKPath = $Env:ANDROID_NDK_ROOT }
if (-not (Test-NdkPath $NDKPath)) { $NDKPath = $Env:ANDROID_NDK_LATEST_HOME }

$RootNdkFile = Join-Path $ProjectRoot "ndkpath.txt"
if ((-not (Test-NdkPath $NDKPath)) -and (Test-Path $RootNdkFile)) {
    $NDKPath = Get-Content $RootNdkFile |
        Where-Object { $_ -and -not $_.Trim().StartsWith("#") } |
        Select-Object -First 1
}

if (-not (Test-NdkPath $NDKPath)) {
    $CacheRoots = @(
        (Join-Path $HOME "Library/Application Support/QPM-RS/ndk"),
        (Join-Path $HOME ".local/share/QPM-RS/ndk"),
        $(if ($Env:APPDATA) { Join-Path $Env:APPDATA "QPM-RS/ndk" })
    ) | Where-Object { $_ -and (Test-Path $_) }

    foreach ($CacheRoot in $CacheRoots) {
        $Candidate = Get-ChildItem $CacheRoot -Directory |
            Sort-Object Name -Descending |
            Where-Object { Test-NdkPath $_.FullName } |
            Select-Object -First 1
        if ($Candidate) {
            $NDKPath = $Candidate.FullName
            break
        }
    }
}

if (-not (Test-NdkPath $NDKPath)) {
    throw "Android NDK not found. Set ANDROID_NDK_HOME, pass -ndk PATH, or create ndkpath.txt from ndkpath.example.txt."
}

$RequiredHeaders = @(
    "extern/includes/beatsaber-hook/shared/utils/hooking.hpp",
    "extern/includes/bs-cordl/include",
    "extern/includes/scotland2/shared/loader.hpp"
)
$MissingDependencies = $RequiredHeaders |
    Where-Object { -not (Test-Path -LiteralPath $_) } |
    Select-Object -First 1

if ($MissingDependencies) {
    if (Get-Command qpm-rust -ErrorAction SilentlyContinue) {
        & qpm-rust restore
        & qpm-rust cache legacy-fix
    } elseif (Get-Command qpm -ErrorAction SilentlyContinue) {
        & qpm restore
        & qpm cache legacy-fix
    } else {
        throw "Dependencies are missing and qpm-rust/qpm is not installed."
    }
}

$BuildType = if ($debugBuild.IsPresent) { "Debug" } elseif ($release.IsPresent) { "RelWithDebInfo" } else { "Debug" }

Write-Host "Building NoodleExtensions ($BuildType)"
Write-Host "Using NDK: $NDKPath"

& cmake -S . -B build -G Ninja "-DCMAKE_BUILD_TYPE=$BuildType" "-DCMAKE_ANDROID_NDK=$NDKPath"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& cmake --build build --parallel
exit $LASTEXITCODE
