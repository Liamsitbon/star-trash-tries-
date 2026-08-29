#!/usr/bin/env pwsh

[CmdletBinding()]
Param(
    [Parameter(Mandatory=$true)]
    [Alias("candidate")]
    [String] $path,

    [Parameter(Mandatory=$false)]
    [String] $baseline=""
)

$ErrorActionPreference = "Stop"
$invariant = [Globalization.CultureInfo]::InvariantCulture
$numberStyle = [Globalization.NumberStyles]::Float
$metricNames = @(
    "processCpuPercent",
    "rssKb",
    "gpuBusyPercent",
    "gpuClockHz",
    "maxThermalMilliC"
)

function Resolve-SamplesPath {
    Param([String] $inputPath)
    $resolved = [IO.Path]::GetFullPath($inputPath)
    if (Test-Path -LiteralPath $resolved -PathType Container) {
        $resolved = Join-Path $resolved "samples.csv"
    }
    if (!(Test-Path -LiteralPath $resolved -PathType Leaf)) {
        throw "samples.csv was not found at '$resolved'."
    }
    return $resolved
}

function Get-MetricSummary {
    Param(
        [Object[]] $rows,
        [String] $metricName
    )

    $values = [Collections.Generic.List[Double]]::new()
    foreach ($row in $rows) {
        [Double] $parsed = 0.0
        $text = [String] $row.$metricName
        if ([Double]::TryParse($text, $numberStyle, $invariant, [ref] $parsed)) {
            $values.Add($parsed)
        }
    }
    if ($values.Count -eq 0) { return $null }

    $values.Sort()
    [Double] $sum = 0.0
    foreach ($value in $values) { $sum += $value }
    $medianIndex = [Math]::Floor(($values.Count - 1) * 0.5)
    $p95Index = [Math]::Max(0, [Math]::Ceiling($values.Count * 0.95) - 1)

    return [PSCustomObject] [Ordered] @{
        name = $metricName
        samples = $values.Count
        minimum = [Math]::Round($values[0], 3)
        mean = [Math]::Round($sum / $values.Count, 3)
        median = [Math]::Round($values[$medianIndex], 3)
        p95 = [Math]::Round($values[$p95Index], 3)
        maximum = [Math]::Round($values[$values.Count - 1], 3)
    }
}

function Get-CaptureSummary {
    Param([String] $inputPath)
    $samplesPath = Resolve-SamplesPath $inputPath
    $rows = @(Import-Csv -LiteralPath $samplesPath)
    if ($rows.Count -eq 0) {
        throw "No sample rows were found in '$samplesPath'."
    }

    $metrics = @()
    foreach ($metricName in $metricNames) {
        $summary = Get-MetricSummary $rows $metricName
        if ($null -ne $summary) { $metrics += $summary }
    }
    return [PSCustomObject] [Ordered] @{
        samplesPath = $samplesPath
        rows = $rows.Count
        metrics = $metrics
    }
}

$candidateSummary = Get-CaptureSummary $path
$payload = [Ordered] @{
    generatedAt = (Get-Date).ToString("o")
    candidate = $candidateSummary
}

if (![String]::IsNullOrWhiteSpace($baseline)) {
    $baselineSummary = Get-CaptureSummary $baseline
    $comparison = @()
    foreach ($candidateMetric in $candidateSummary.metrics) {
        $baselineMetric = $baselineSummary.metrics |
            Where-Object { $_.name -eq $candidateMetric.name } |
            Select-Object -First 1
        if ($null -eq $baselineMetric) { continue }

        $deltaMedian = $candidateMetric.median - $baselineMetric.median
        $deltaPercent = if ([Math]::Abs($baselineMetric.median) -gt 1e-9) {
            [Math]::Round(100.0 * $deltaMedian / $baselineMetric.median, 3)
        } else {
            $null
        }
        $comparison += [PSCustomObject] [Ordered] @{
            name = $candidateMetric.name
            baselineMedian = $baselineMetric.median
            candidateMedian = $candidateMetric.median
            deltaMedian = [Math]::Round($deltaMedian, 3)
            deltaPercent = $deltaPercent
        }
    }
    $payload.baseline = $baselineSummary
    $payload.comparison = $comparison
}

$summaryPath = Join-Path (Split-Path -Parent $candidateSummary.samplesPath) "performance-summary.json"
$payload | ConvertTo-Json -Depth 8 |
    Set-Content -LiteralPath $summaryPath -Encoding utf8

Write-Output "Quest performance summary: $summaryPath"
$candidateSummary.metrics | Format-Table name, samples, minimum, mean, median, p95, maximum
if ($payload.Contains("comparison")) {
    Write-Output "Candidate median compared with baseline:"
    $payload.comparison |
        Format-Table name, baselineMedian, candidateMedian, deltaMedian, deltaPercent
}
