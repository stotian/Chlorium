param(
    [ValidateSet(133260800,129598490,88952972)][int[]]$Levels = @(133260800,129598490,88952972),
    [string]$ReportDirectory,
    [string]$ProductionPackage,
    [switch]$VisualWatch,
    [switch]$ReuseCompleted
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (!$ReportDirectory) { $ReportDirectory = Join-Path $root ('audit-artifacts\paired-' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
$ReportDirectory = [IO.Path]::GetFullPath($ReportDirectory)
$audit = Join-Path $root 'comparison-dist\local.chlorium.audit.geode'
$production = if ($ProductionPackage) { [IO.Path]::GetFullPath($ProductionPackage) } else { Join-Path $root 'dist\local.chlorium.geode' }
$auditHash = (Get-FileHash -LiteralPath $audit).Hash
$productionHash = (Get-FileHash -LiteralPath $production).Hash
$index = 0
foreach ($level in $Levels) {
    # Alternate order between levels to reduce systematic first-run bias.
    $modes = if (($index++ % 2) -eq 0) { @($true,$false) } else { @($false,$true) }
    foreach ($native in $modes) {
        $mode = if ($native) { 'native' } else { 'chlorium' }
        $report = Join-Path $ReportDirectory "auto-$level-$mode.jsonl"
        if (Test-Path -LiteralPath $report) {
            if (!$ReuseCompleted) { throw "Refusing to overwrite $report" }
            $last = Get-Content -LiteralPath $report -Tail 1 | ConvertFrom-Json
            $receipt = Get-Content -LiteralPath ($report + '.provenance.json') -Raw | ConvertFrom-Json
            if ($last.event -ne 'finished' -or !$last.success -or !$receipt.measurement_only -or
                $receipt.measurement_sha256 -ne $auditHash -or $receipt.native -ne $native -or
                (!$native -and $receipt.production_sha256 -ne $productionHash)) { throw "Existing run cannot be reused: $report" }
            Write-Output "Reusing completed run: $level $mode"
            continue
        }
        Write-Output "Starting comparison: $level $mode"
        & (Join-Path $PSScriptRoot 'run-runtime-audit.ps1') -Native:$native -VisualWatch:$VisualWatch -Level $level -Package $audit -ProductionPackage $production -ReportDirectory $ReportDirectory
    }
    if (!$VisualWatch) {
        & 'C:\Users\stoti\scoop\apps\python\current\python.exe' (Join-Path $PSScriptRoot 'compare-auto-runs.py') (Join-Path $ReportDirectory "auto-$level-native.jsonl") (Join-Path $ReportDirectory "auto-$level-chlorium.jsonl") (Join-Path $ReportDirectory "comparaison-$level")
        if ($LASTEXITCODE -ne 0) { throw "Comparison validation failed for $level" }
    }
}
