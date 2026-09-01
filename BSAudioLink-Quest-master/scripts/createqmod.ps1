#!/usr/bin/env pwsh

param(
    [Parameter(Mandatory=$false)]
    [String] $qmodName = "bs-audiolink",

    [Parameter(Mandatory=$false)]
    [Switch] $clean
)

$ErrorActionPreference = "Stop"
$defaultQmod = "bs-audiolink.qmod"
$requestedQmod = "$qmodName.qmod"

if ([string]::IsNullOrWhiteSpace($qmodName)) {
    Write-Error "qmodName cannot be empty."
    exit 1
}

if ($clean) {
    Remove-Item "./mod.json" -Force -ErrorAction SilentlyContinue
    Remove-Item "./$defaultQmod" -Force -ErrorAction SilentlyContinue
    if ($requestedQmod -ne $defaultQmod) {
        Remove-Item "./$requestedQmod" -Force -ErrorAction SilentlyContinue
    }
}

# Let QPM build the manifest from qpm.shared.json + mod.template.json and then
# construct a fresh archive. This avoids Compress-Archive -Update retaining stale
# files from an older QMOD.
& qpm qmod manifest
if ($LASTEXITCODE -ne 0) {
    Write-Error "qpm qmod manifest failed."
    exit $LASTEXITCODE
}

& qpm qmod zip
if ($LASTEXITCODE -ne 0) {
    Write-Error "qpm qmod zip failed."
    exit $LASTEXITCODE
}

if (-not (Test-Path "./$defaultQmod")) {
    Write-Error "QPM did not create $defaultQmod."
    exit 1
}

if ($requestedQmod -ne $defaultQmod) {
    Move-Item "./$defaultQmod" "./$requestedQmod" -Force
}

Write-Output "Created $requestedQmod"
