#!/usr/bin/env pwsh

$ErrorActionPreference = "Stop"
$projectDir = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$testDir = Join-Path ([IO.Path]::GetTempPath()) ("vivify-quest3-analysis-" + [Guid]::NewGuid().ToString("N"))
$baselineDir = Join-Path $testDir "baseline"
$candidateDir = Join-Path $testDir "candidate"

function Assert-Equal {
    Param($actual, $expected, [String] $message)
    if ($actual -ne $expected) {
        throw "$message (expected '$expected', found '$actual')"
    }
}

try {
    New-Item -ItemType Directory -Path $baselineDir, $candidateDir -Force | Out-Null
    @"
hostTime,pid,processCpuPercent,rssKb,vszKb,gpuBusyTicks,gpuTotalTicks,gpuBusyPercent,gpuClockHz,maxThermalMilliC
2026-08-11T10:00:00Z,1,12,1000,2000,40,100,40,500000000,41000
2026-08-11T10:00:01Z,1,12,1100,2000,50,100,50,510000000,42000
2026-08-11T10:00:02Z,1,12,1200,2000,60,100,60,520000000,43000
"@ | Set-Content -LiteralPath (Join-Path $baselineDir "samples.csv") -Encoding utf8
    @"
hostTime,pid,processCpuPercent,rssKb,vszKb,gpuBusyTicks,gpuTotalTicks,gpuBusyPercent,gpuClockHz,maxThermalMilliC
2026-08-11T10:00:00Z,1,8,900,2000,30,100,30,450000000,39000
2026-08-11T10:00:01Z,1,9,1000,2000,40,100,40,460000000,40000
2026-08-11T10:00:02Z,1,10,1100,2000,50,100,50,470000000,41000
"@ | Set-Content -LiteralPath (Join-Path $candidateDir "samples.csv") -Encoding utf8

    & (Join-Path $projectDir "scripts/analyze-quest3-performance.ps1") `
        -path $candidateDir -baseline $baselineDir | Out-Null

    $summary = Get-Content -LiteralPath (Join-Path $candidateDir "performance-summary.json") -Raw |
        ConvertFrom-Json
    $candidateCpu = $summary.candidate.metrics | Where-Object { $_.name -eq "processCpuPercent" }
    $comparisonCpu = $summary.comparison | Where-Object { $_.name -eq "processCpuPercent" }
    Assert-Equal $summary.candidate.rows 3 "candidate row count"
    Assert-Equal $candidateCpu.median 9 "candidate CPU median"
    Assert-Equal $candidateCpu.p95 10 "candidate CPU p95"
    Assert-Equal $comparisonCpu.baselineMedian 12 "baseline CPU median"
    Assert-Equal $comparisonCpu.deltaPercent -25 "CPU median delta percent"
} finally {
    if (Test-Path -LiteralPath $testDir) {
        Remove-Item -LiteralPath $testDir -Recurse -Force
    }
}
