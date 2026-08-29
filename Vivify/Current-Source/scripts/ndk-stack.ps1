#!/usr/bin/env pwsh

Param(
    [Parameter(Mandatory=$false)]
    [String] $input="./tombstone.txt",

    [Parameter(Mandatory=$false)]
    [String] $symbols="./build/debug"
)

if (-not (Test-Path $input)) {
    Write-Error "Tombstone file '$input' does not exist. Run 'qpm s tomb' first."
    exit 1
}
if (-not (Test-Path $symbols)) {
    Write-Error "Symbol directory '$symbols' does not exist. Run a RelWithDebInfo build first."
    exit 1
}

$ndkPath = $env:ANDROID_NDK_HOME
if ([String]::IsNullOrWhiteSpace($ndkPath) -or -not (Test-Path $ndkPath)) {
    $installed = & qpm ndk list -q
    $candidate = $installed |
        Select-String -Pattern " -> " |
        Select-Object -Last 1
    if ($null -ne $candidate) {
        $ndkPath = ($candidate.Line -split " -> ", 2)[1].Trim()
    }
}

$ndkStack = if ([String]::IsNullOrWhiteSpace($ndkPath)) {
    ""
} else {
    Join-Path $ndkPath "ndk-stack"
}
if ([String]::IsNullOrWhiteSpace($ndkStack) -or -not (Test-Path $ndkStack)) {
    Write-Error "Could not locate ndk-stack. Run 'qpm ndk resolve --download' first."
    exit 1
}

Get-Content $input -Raw | & $ndkStack -sym $symbols
exit $LASTEXITCODE
