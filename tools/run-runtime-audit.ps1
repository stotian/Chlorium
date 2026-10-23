param([switch]$Native, [switch]$VisualWatch, [switch]$AtlasStorageRegression, [string]$GameDirectory, [string]$Package, [string]$ProductionPackage, [string]$ReportDirectory,
    [ValidateSet(0,133260800,129598490,88952972)][int]$FetchLevel = 0,
    [ValidateSet(133260800,129598490,88952972)][int]$Level = 133260800,
    [int]$BenchmarkSeconds = 900,
    [ValidateRange(0,99.999)][double]$FromPercent = 0,
    [ValidateRange(0.001,100)][double]$ToPercent = 100,
    [ValidateSet('map','orphan')][string]$StreamMode = 'orphan')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if ($FromPercent -ge $ToPercent) { throw 'The target percentage range must increase' }
$game = if ($GameDirectory) { $GameDirectory } else { Join-Path $root 'test-game' }
$destination = Join-Path $game 'geode\mods\local.chlorium.audit.geode'
$productionDestination = Join-Path $game 'geode\mods\local.chlorium.geode'
if (Get-Process GeometryDash -ErrorAction SilentlyContinue) { throw 'A game session is already running; refusing to interfere' }
if (!$Package) {
    $cache = Get-Content -LiteralPath (Join-Path $root 'build\CMakeCache.txt') -Raw
    if ($cache -notmatch '(?m)^CHLORIUM_ORBIT_BENCHMARK:BOOL=ON\r?$' -or
        $cache -match '(?m)^CHLORIUM_AUDIT_HARNESS_ONLY:BOOL=ON\r?$') {
        throw 'Only the collision-preserving auto-level harness is authorized'
    }
    & (Join-Path $root 'package-local.ps1') -OutputDirectory 'audit-dist' -Audit
    $Package = Join-Path $root 'audit-dist\local.chlorium.audit.geode'
}
# Inspect the package actually launched, not an unrelated current build DLL.
Add-Type -AssemblyName System.IO.Compression.FileSystem
$auditZip = [IO.Compression.ZipFile]::OpenRead($Package)
try {
    $auditEntry = $auditZip.GetEntry('local.chlorium.audit.dll')
    if (!$auditEntry) { throw 'Expected an audit package' }
    $stream = $auditEntry.Open(); $memory = [IO.MemoryStream]::new()
    try { $stream.CopyTo($memory); $binary = [Text.Encoding]::ASCII.GetString($memory.ToArray()) }
    finally { $stream.Dispose(); $memory.Dispose() }
    if (!$binary.Contains('CHLORIUM_AUTO_COLLISIONS_ALLOWLIST_V4') -or
        !$binary.Contains('CHLORIUM_RUNTIME_AUDIT_BINARY_V1')) { throw 'Only the collision-preserving auto-level harness is authorized' }
    $measurementOnly = $binary.Contains('CHLORIUM_MEASUREMENT_ONLY_V1')
    $binary = $null
} finally { $auditZip.Dispose() }
if ($ProductionPackage -and !$measurementOnly) { throw 'A production package requires the independent measurement module' }
if ($FetchLevel -gt 0) {
    if ($PSBoundParameters.ContainsKey('Level') -and $Level -ne $FetchLevel) { throw 'Conflicting selected and fetch level IDs' }
    $Level = $FetchLevel
}
$mode = if ($Native) { 'native' } else { 'chlorium' }
$reportRoot = if ($ReportDirectory) { [IO.Path]::GetFullPath($ReportDirectory) } else { $root }
New-Item -ItemType Directory -Force -Path $reportRoot | Out-Null
$output = Join-Path $reportRoot "auto-$Level-$mode.jsonl"
if (Get-Process GeometryDash -ErrorAction SilentlyContinue) { throw 'A game session is already running; refusing to interfere' }
if (Test-Path -LiteralPath $destination) { throw 'An audit package already exists in mods' }
if ($measurementOnly -and (Test-Path -LiteralPath $productionDestination)) { throw 'The fixture already contains Chlorium; refusing to replace it' }
if ($measurementOnly -and !$Native) {
    if (!$ProductionPackage) { $ProductionPackage = Join-Path $root 'dist\local.chlorium.geode' }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $productionZip = [IO.Compression.ZipFile]::OpenRead($ProductionPackage)
    try {
        $reader = [IO.StreamReader]::new($productionZip.GetEntry('mod.json').Open())
        try { $manifest = $reader.ReadToEnd() | ConvertFrom-Json } finally { $reader.Dispose() }
        $entry = $productionZip.GetEntry('local.chlorium.dll')
        if ($manifest.id -ne 'local.chlorium' -or !$entry) { throw 'Expected a normal Chlorium package' }
        $stream = $entry.Open(); $memory = [IO.MemoryStream]::new()
        try { $stream.CopyTo($memory); $binary = [Text.Encoding]::ASCII.GetString($memory.ToArray()) }
        finally { $stream.Dispose(); $memory.Dispose() }
        if (!$binary.Contains('CHLORIUM_NORMAL_BINARY_V1') -or $binary.Contains('CHLORIUM_RUNTIME_AUDIT_BINARY_V1')) { throw 'Refusing an experimental production package' }
        $binary = $null
    } finally { $productionZip.Dispose() }
}
if ($VisualWatch -and (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'diagnostic-report.py'))) {
    # Update the readable report only between games, never during measurement.
    $diagnosticOutput = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'chlorium\diagnostic'
    & 'C:\Users\stoti\scoop\apps\python\current\python.exe' (Join-Path $PSScriptRoot 'diagnostic-report.py') $reportRoot $diagnosticOutput
    if ($LASTEXITCODE -ne 0) { throw 'Diagnostic report generation failed before the next run' }
}
$previous = $env:CHLORIUM_AUDIT_OUTPUT
$previousNative = $env:CHLORIUM_AUDIT_NATIVE
$previousFetch = $env:CHLORIUM_AUDIT_FETCH_ID
$previousBenchmark = $env:CHLORIUM_BENCHMARK_SECONDS
$previousLevel = $env:CHLORIUM_BENCHMARK_LEVEL
$previousVisual = $env:CHLORIUM_VISUAL_WATCH
$previousStorageTest = $env:CHLORIUM_ATLAS_STORAGE_REGRESSION
$previousFrom = $env:CHLORIUM_TARGET_FROM
$previousTo = $env:CHLORIUM_TARGET_TO
$previousStreamMode = $env:CHLORIUM_STREAM_MODE
$testProcess = $null
$resourceTrace = $null
$stagedProduction = $false
$restoreProductionLoadFlag = $false
$loaderSavedPath = Join-Path $env:LOCALAPPDATA 'GeometryDash\geode\mods\geode.loader\saved.json'
try {
    if (Test-Path -LiteralPath ($output + '.stop-request')) { Remove-Item -LiteralPath ($output + '.stop-request') }
    if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output }
    Copy-Item -LiteralPath $Package -Destination $destination
    if ($measurementOnly -and !$Native) {
        Copy-Item -LiteralPath $ProductionPackage -Destination $productionDestination
        $stagedProduction = $true
        # Geode shares preferences between the fixture and main game. Enable
        # only this mod for this run, then restore the user's original choice.
        $loaderSaved = Get-Content -LiteralPath $loaderSavedPath -Raw | ConvertFrom-Json -AsHashtable
        $hadProductionLoadFlag = $loaderSaved.ContainsKey('should-load-local.chlorium')
        $previousProductionLoadFlag = $loaderSaved['should-load-local.chlorium']
        if (!$hadProductionLoadFlag -or $previousProductionLoadFlag -ne $true) {
            $restoreProductionLoadFlag = $true
            $loaderSaved['should-load-local.chlorium'] = $true
            [IO.File]::WriteAllText($loaderSavedPath, ($loaderSaved | ConvertTo-Json -Depth 100), [Text.UTF8Encoding]::new($false))
        }
    }
    @{
        level = $Level; native = [bool]$Native; measurement_only = $measurementOnly
        measurement_sha256 = (Get-FileHash -LiteralPath $Package).Hash
        production_sha256 = if ($stagedProduction) { (Get-FileHash -LiteralPath $productionDestination).Hash } else { $null }
        game_sha256 = (Get-FileHash -LiteralPath (Join-Path $game 'GeometryDash.exe')).Hash
        started_utc = [DateTime]::UtcNow.ToString('o')
        visual_diagnostic = [bool]$VisualWatch
        atlas_storage_regression = [bool]$AtlasStorageRegression
        exact_transform_math_verification = [bool]$env:CHLORIUM_TRANSFORM_VERIFY
        target_from_percent = $FromPercent; target_to_percent = $ToPercent
        low_detail_requested = $false; disable_shake_requested = $true
        experimental_stream_mode = $StreamMode
    } | ConvertTo-Json | Set-Content -LiteralPath ($output + '.provenance.json') -Encoding UTF8
    $resourceTrace = [IO.StreamWriter]::new($output + '.process.csv', $false)
    $resourceTrace.AutoFlush = $true
    $resourceTrace.WriteLine('elapsed_wall_s,working_set_bytes,private_bytes,cpu_seconds')
    $env:CHLORIUM_AUDIT_OUTPUT = $output
    $env:CHLORIUM_AUDIT_NATIVE = if ($Native) { '1' } else { $null }
    $env:CHLORIUM_AUDIT_FETCH_ID = if ($FetchLevel -gt 0) { [string]$FetchLevel } else { $null }
    $env:CHLORIUM_BENCHMARK_SECONDS = if ($BenchmarkSeconds -gt 0) { [string]$BenchmarkSeconds } else { $null }
    $env:CHLORIUM_BENCHMARK_LEVEL = [string]$Level
    $env:CHLORIUM_VISUAL_WATCH = if ($VisualWatch) { '1' } else { $null }
    $env:CHLORIUM_ATLAS_STORAGE_REGRESSION = if ($AtlasStorageRegression) { '1' } else { $null }
    $env:CHLORIUM_TARGET_FROM = $FromPercent.ToString([Globalization.CultureInfo]::InvariantCulture)
    $env:CHLORIUM_TARGET_TO = $ToPercent.ToString([Globalization.CultureInfo]::InvariantCulture)
    $env:CHLORIUM_STREAM_MODE = $StreamMode
    # This is the visible interactive game being tested, not a background
    # helper. Let GD manage its own normal focused rendering/timing.
    $testProcess = Start-Process -FilePath (Join-Path $game 'GeometryDash.exe') -WorkingDirectory $game -PassThru
    $processClock = [Diagnostics.Stopwatch]::StartNew()
    Write-Output "Dedicated test process: $($testProcess.Id)"
    $deadline = [DateTime]::UtcNow.AddSeconds([Math]::Max(600, $BenchmarkSeconds + 300))
    while (!$testProcess.HasExited -and [DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Seconds 2
        $testProcess.Refresh()
        if (!$testProcess.HasExited) {
            $resourceTrace.WriteLine([string]::Format([Globalization.CultureInfo]::InvariantCulture,
                '{0:F3},{1},{2},{3:F3}', $processClock.Elapsed.TotalSeconds,
                $testProcess.WorkingSet64, $testProcess.PrivateMemorySize64, $testProcess.TotalProcessorTime.TotalSeconds))
        }
    }
    if (!$testProcess.HasExited) { Stop-Process -Id $testProcess.Id; throw 'Runtime audit timed out' }
    Write-Output "Test process exit: $($testProcess.ExitCode)"
    if (!(Test-Path -LiteralPath $output)) { throw 'The game did not reach the audit harness' }
    $provenancePath = $output + '.provenance.json'
    $provenance = Get-Content -LiteralPath $provenancePath -Raw | ConvertFrom-Json
    $levelData = Join-Path $reportRoot "references\level-$Level.level"
    if (Test-Path -LiteralPath $levelData) {
        $provenance | Add-Member -NotePropertyName level_sha256 -NotePropertyValue (Get-FileHash -LiteralPath $levelData).Hash -Force
    }
    $provenance | ConvertTo-Json | Set-Content -LiteralPath $provenancePath -Encoding UTF8
    $last = Get-Content -LiteralPath $output -Tail 1 | ConvertFrom-Json
    $last | ConvertTo-Json -Compress
    if ($last.event -ne 'finished' -or !$last.success) { throw 'Runtime audit did not complete successfully' }
} finally {
    $env:CHLORIUM_AUDIT_OUTPUT = $previous
    $env:CHLORIUM_AUDIT_NATIVE = $previousNative
    $env:CHLORIUM_AUDIT_FETCH_ID = $previousFetch
    $env:CHLORIUM_BENCHMARK_SECONDS = $previousBenchmark
    $env:CHLORIUM_BENCHMARK_LEVEL = $previousLevel
    $env:CHLORIUM_VISUAL_WATCH = $previousVisual
    $env:CHLORIUM_ATLAS_STORAGE_REGRESSION = $previousStorageTest
    $env:CHLORIUM_TARGET_FROM = $previousFrom
    $env:CHLORIUM_TARGET_TO = $previousTo
    $env:CHLORIUM_STREAM_MODE = $previousStreamMode
    if ($resourceTrace) { $resourceTrace.Dispose() }
    if ($testProcess -and !$testProcess.HasExited) { Stop-Process -Id $testProcess.Id -ErrorAction SilentlyContinue }
    if ($testProcess) { $testProcess.WaitForExit(5000) | Out-Null; $testProcess.Dispose() }
    if (Test-Path -LiteralPath $destination) { Remove-Item -LiteralPath $destination }
    if ($stagedProduction -and (Test-Path -LiteralPath $productionDestination)) { Remove-Item -LiteralPath $productionDestination }
    if ($restoreProductionLoadFlag) {
        $loaderSaved = Get-Content -LiteralPath $loaderSavedPath -Raw | ConvertFrom-Json -AsHashtable
        if ($hadProductionLoadFlag) { $loaderSaved['should-load-local.chlorium'] = $previousProductionLoadFlag }
        else { $loaderSaved.Remove('should-load-local.chlorium') }
        [IO.File]::WriteAllText($loaderSavedPath, ($loaderSaved | ConvertTo-Json -Depth 100), [Text.UTF8Encoding]::new($false))
    }
}
