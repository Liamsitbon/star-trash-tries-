Param(
    [Parameter(Mandatory=$false)]
    [Switch] $clean,

    [Parameter(Mandatory=$false)]
    [Switch] $help
)

if ($help -eq $true) {
    Write-Output '"Build" - Compiles the mod into a .so library'
    Write-Output "`n-- Arguments --`n"
    Write-Output '-Clean`t`t Deletes the build folder so the library is rebuilt'
    exit 0
}

# Source archives intentionally do not ship QPM's generated extern directory.
# Restore only when required headers are missing, so normal incremental builds stay fast.
$requiredHeaders = @(
    './extern/includes/scotland2/shared/modloader.h',
    './extern/includes/beatsaber-hook/shared/utils/hooking.hpp',
    './extern/includes/bs-cordl/include'
)

$missingDependencies = $false
foreach ($header in $requiredHeaders) {
    if (-not (Test-Path -LiteralPath $header)) {
        $missingDependencies = $true
        break
    }
}

if ($missingDependencies) {
    Write-Output 'QPM dependencies are missing. Running qpm restore...'
    & qpm restore
    if ($LASTEXITCODE -ne 0) {
        Write-Error "qpm restore failed (exit $LASTEXITCODE)."
        exit $LASTEXITCODE
    }

    foreach ($header in $requiredHeaders) {
        if (-not (Test-Path -LiteralPath $header)) {
            Write-Error "Dependency restore completed, but required header is still missing: $header"
            exit 1
        }
    }
}

if ($clean.IsPresent -and (Test-Path -LiteralPath './build')) {
    Remove-Item './build' -Recurse -Force
}

if (-not (Test-Path -LiteralPath './build')) {
    New-Item -Path './build' -ItemType Directory | Out-Null
}

& cmake -G 'Ninja' -DCMAKE_BUILD_TYPE='RelWithDebInfo' -B build
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed (exit $LASTEXITCODE)."
    exit $LASTEXITCODE
}

& cmake --build ./build
exit $LASTEXITCODE
