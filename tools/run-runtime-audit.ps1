#Requires -Version 7.0
param([switch]$Native, [switch]$VisualWatch, [switch]$WithUserMods, [switch]$ExternalFocus, [switch]$AtlasStorageRegression, [string]$GameDirectory, [string]$Package, [string]$ProductionPackage, [string]$ReportDirectory,
    [string]$UserGameDirectory = 'C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash',
    [ValidateSet(0,133260800,129598490,88952972)][int]$FetchLevel = 0,
    [ValidateSet(133260800,129598490,88952972)][int]$Level = 133260800,
    [int]$BenchmarkSeconds = 900,
    [ValidateRange(0,99.999)][double]$FromPercent = 0,
    [ValidateRange(0.001,100)][double]$ToPercent = 100,
    [ValidateSet('map','orphan')][string]$StreamMode = 'orphan',
    [ValidateRange(0,100)][double[]]$CapturePercent = @())
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if ($FromPercent -ge $ToPercent) { throw 'The target percentage range must increase' }
if ($CapturePercent.Count -gt 16 -or ($CapturePercent | Where-Object { $_ -lt $FromPercent -or $_ -ge $ToPercent })) {
    throw 'Use at most 16 capture percentages inside the measured range (end excluded)'
}
if ($CapturePercent.Count -and !$VisualWatch) { throw 'Capture percentages require VisualWatch' }
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
    $supportsTargetedMath = $binary.Contains('CHLORIUM_TARGETED_MATH_TELEMETRY_V1')
    $supportsMathPipeline = $binary.Contains('CHLORIUM_MATH_PIPELINE_AUDIT_V1')
    $supportsNativeSnapshots = $binary.Contains('CHLORIUM_NATIVE_SNAPSHOT_AUDIT_V1')
    $supportsCanonical = $binary.Contains('CHLORIUM_CANONICAL_CORE_AUDIT_V10')
    if ($VisualWatch -and (!$binary.Contains('CHLORIUM_VISUAL_READBACK_V2') -or
        !$binary.Contains('CHLORIUM_PRESENTED_CAPTURE_ONLY_V3') -or !$binary.Contains('CHLORIUM_READBACK_STATE_GUARDS_V5'))) {
        throw 'Rebuild visual diagnostics with presented-frame reads, scoped GL restoration, and complete probe/file coverage'
    }
    if ($CapturePercent.Count -and !$binary.Contains('CHLORIUM_VISUAL_PROBES_V1')) { throw 'Rebuild a harness supporting visual probes' }
    if ($WithUserMods -and (!$measurementOnly -or !$binary.Contains('CHLORIUM_AUTO_USER_MOD_PROFILE_V1'))) {
        throw 'Full-stack tests require the measurement harness with verified mod inventory'
    }
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
$requireTargetedMath = $false
$requireMathPipeline = $false
$requireNativeSnapshots = $false
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
        if ($binary.Contains('CHLORIUM_EXPERIMENTAL_AREA_MATH_V1')) {
            if (!$binary.Contains('CHLORIUM_AREA_MATH_TARGETED_CALLS_V3')) { throw 'The Area math candidate predates targeted calls; rebuild it before a new audit' }
            if (!$supportsTargetedMath) { throw 'Rebuild the independent measurement module to report optional math counters and preserve CPU summaries' }
            $requireTargetedMath = $true
        }
        if ($binary.Contains('CHLORIUM_EXPERIMENTAL_MATH_PIPELINE_V1')) {
            if (!$supportsMathPipeline) { throw 'Rebuild the measurement module to require both active math pipeline components' }
            $requireMathPipeline = $true
        }
        if ($binary.Contains('CHLORIUM_NATIVE_CANONICAL_BACKEND_V10')) {
            if (!$supportsCanonical -or !$binary.Contains('CHLORIUM_CANONICAL_AREA_QUOTIENTS_V10')) { throw 'Canonical core requires its independent V10 proof auditor' }
        } elseif ($binary.Contains('CHLORIUM_EXPERIMENTAL_NATIVE_SNAPSHOTS_V1')) {
            if (!$supportsNativeSnapshots) { throw 'Rebuild the independent measurement module to verify native snapshot activation' }
            $requireNativeSnapshots = $true
        }
        $binary = $null
    } finally { $productionZip.Dispose() }
}
# Generate reports explicitly after a completed pair. A first run in a new
# directory must not erase an existing readable report with an empty one.
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
$previousExpectedMods = $env:CHLORIUM_EXPECTED_USER_MODS
$previousProbes = $env:CHLORIUM_VISUAL_PROBES
$previousTargetedMath = $env:CHLORIUM_REQUIRE_TARGETED_MATH_V3
$previousNativeSnapshots = $env:CHLORIUM_REQUIRE_NATIVE_SNAPSHOTS
$testProcess = $null
$resourceTrace = $null
$stagedProduction = $false
$restoreProductionLoadFlag = $false
$stagedUserMods = [Collections.Generic.List[string]]::new()
$temporarySettings = [Collections.Generic.List[object]]::new()
$userModManifest = @()
$profileHash = $null
$profileSettingsHash = $null
$loaderSavedPath = Join-Path $env:LOCALAPPDATA 'GeometryDash\geode\mods\geode.loader\saved.json'
try {
    if (Test-Path -LiteralPath ($output + '.stop-request')) { Remove-Item -LiteralPath ($output + '.stop-request') }
    if (Test-Path -LiteralPath ($output + '.snapshot-failure.json')) { Remove-Item -LiteralPath ($output + '.snapshot-failure.json') }
    if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output }
    Copy-Item -LiteralPath $Package -Destination $destination
    if ($WithUserMods) {
        if ([IO.Path]::GetFullPath($game).TrimEnd('\') -eq [IO.Path]::GetFullPath($UserGameDirectory).TrimEnd('\')) {
            throw 'Full-stack tests must use a separate game fixture'
        }
        $loaderPreferences = Get-Content -LiteralPath $loaderSavedPath -Raw | ConvertFrom-Json -AsHashtable
        foreach ($source in (Get-ChildItem -LiteralPath (Join-Path $UserGameDirectory 'geode\mods') -Filter '*.geode' -File | Sort-Object Name)) {
            $zip = [IO.Compression.ZipFile]::OpenRead($source.FullName)
            try {
                $reader = [IO.StreamReader]::new($zip.GetEntry('mod.json').Open())
                try { $modInfo = $reader.ReadToEnd() | ConvertFrom-Json } finally { $reader.Dispose() }
            } finally { $zip.Dispose() }
            if ($modInfo.id -in @('local.chlorium','local.chlorium.audit','iliashdz.bismuth')) { continue }
            $target = Join-Path $game ('geode\mods\' + $source.Name)
            $hash = (Get-FileHash -LiteralPath $source.FullName).Hash
            if (Test-Path -LiteralPath $target) {
                if ((Get-FileHash -LiteralPath $target).Hash -ne $hash) { throw "Fixture has a different package: $($source.Name)" }
            } else {
                Copy-Item -LiteralPath $source.FullName -Destination $target
                $stagedUserMods.Add($target)
            }
            $key = 'should-load-' + $modInfo.id
            $requested = !$loaderPreferences.ContainsKey($key) -or [bool]$loaderPreferences[$key]
            $userModManifest += [ordered]@{ id=$modInfo.id; version=$modInfo.version; package=$source.Name; sha256=$hash; requested_enabled=$requested }
        }
        $profileBytes = [Text.Encoding]::UTF8.GetBytes(($userModManifest | ConvertTo-Json -Depth 8 -Compress))
        $profileHash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($profileBytes))
        $userModManifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath ($output + '.mods.json') -Encoding UTF8
        # Back up exact bytes before the only test overrides. Both mods stay
        # loaded; their LDM/input-assist options cannot falsify the requested run.
        $settingOverrides = @(
            @{ relative='eclipse.eclipse-menu\config.json'; values=@{
                'level.autoldm'=$false; 'player.noclip'=$false;
                'global.speedhack.toggle'=$false; 'player.autoclick'=$false;
                'player.autokill'=$false; 'player.framestepper'=$false;
                'level.showhitboxes'=$false; 'level.showhitboxes.ondeath'=$false;
                'player.showtrajectory'=$false; 'level.layoutmode'=$false;
                'level.noshader'=$false
            } },
            @{ relative='local.clickassist\saved.json'; values=@{ 'stage'=4 } },
            @{ relative='geode.loader\settings.json'; values=@{ 'file-log-level'='info' } }
        )
        $backupFolder = Join-Path $reportRoot ('settings-before-' + $Level + '-' + $mode)
        New-Item -ItemType Directory -Force -Path $backupFolder | Out-Null
        $effectiveSettings = [Collections.Generic.List[string]]::new()
        foreach ($override in $settingOverrides) {
            $path = Join-Path (Join-Path $env:LOCALAPPDATA 'GeometryDash\geode\mods') $override.relative
            if (!(Test-Path -LiteralPath $path)) { continue }
            $bytes = [IO.File]::ReadAllBytes($path)
            $settings = [Text.Encoding]::UTF8.GetString($bytes).TrimStart([char]0xfeff) | ConvertFrom-Json -AsHashtable
            foreach ($key in $override['values'].Keys) { $settings[$key] = $override['values'][$key] }
            $backup = Join-Path $backupFolder ($override.relative.Replace('\','-'))
            if (Test-Path -LiteralPath $backup) { throw 'Settings backup already exists; recover or choose a fresh report directory' }
            [IO.File]::WriteAllBytes($backup, $bytes)
            $temporarySettings.Add(@{ path=$path; bytes=$bytes; backup=$backup })
            $canonical = [ordered]@{}
            foreach ($key in ($settings.Keys | Sort-Object)) { $canonical[$key] = $settings[$key] }
            $json = $canonical | ConvertTo-Json -Depth 100
            [IO.File]::WriteAllText($path, $json, [Text.UTF8Encoding]::new($false))
            $effectiveSettings.Add($override.relative + ':' + $json)
        }
        $profileSettingsHash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData(
            [Text.Encoding]::UTF8.GetBytes(($effectiveSettings -join "`n"))))
        $env:CHLORIUM_EXPECTED_USER_MODS = (($userModManifest | Where-Object requested_enabled | ForEach-Object id) -join ',')
    } else {
        $env:CHLORIUM_EXPECTED_USER_MODS = $null
        . (Join-Path $PSScriptRoot 'ModProfile.ps1')
        $isolatedProfile = Get-ResearchModProfile -ModsDirectory (Join-Path $game 'geode/mods') -ProfileRoot (Join-Path $env:LOCALAPPDATA 'GeometryDash')
        $profileHash = $isolatedProfile.profile_sha256
        $profileSettingsHash = $isolatedProfile.settings_sha256
        $isolatedProfile.mods | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath ($output + '.mods.json') -Encoding UTF8
    }
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
        exact_transform_math_verification = $null -ne $env:CHLORIUM_TRANSFORM_VERIFY
        math_verification_stride_requested = $env:CHLORIUM_TRANSFORM_VERIFY_STRIDE
        math_diagnostics_requested = $null -ne $env:CHLORIUM_MATH_DIAGNOSTICS
        area_math_trace_requested = $null -ne $env:CHLORIUM_AREA_MATH_TRACE
        targeted_math_v3_required = $requireTargetedMath
        combined_math_pipeline_required = $requireMathPipeline
        native_snapshots_required = $requireNativeSnapshots
        target_from_percent = $FromPercent; target_to_percent = $ToPercent
        low_detail_requested = $false; disable_shake_requested = $true
        debug_overlays_disabled_requested = [bool]$WithUserMods
        visual_probe_percents = @($CapturePercent)
        mod_profile = if ($WithUserMods) { 'user-mods' } else { 'isolated' }
        mod_profile_sha256 = $profileHash; profile_settings_sha256 = $profileSettingsHash
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
    $env:CHLORIUM_VISUAL_PROBES = if ($CapturePercent.Count) { ($CapturePercent | ForEach-Object { $_.ToString([Globalization.CultureInfo]::InvariantCulture) }) -join ',' } else { $null }
    $env:CHLORIUM_ATLAS_STORAGE_REGRESSION = if ($AtlasStorageRegression) { '1' } else { $null }
    $env:CHLORIUM_TARGET_FROM = $FromPercent.ToString([Globalization.CultureInfo]::InvariantCulture)
    $env:CHLORIUM_TARGET_TO = $ToPercent.ToString([Globalization.CultureInfo]::InvariantCulture)
    $env:CHLORIUM_STREAM_MODE = $StreamMode
    $env:CHLORIUM_REQUIRE_TARGETED_MATH_V3 = if ($requireMathPipeline) { 'pipeline' } elseif ($requireTargetedMath) { '1' } else { $null }
    $env:CHLORIUM_REQUIRE_NATIVE_SNAPSHOTS = if ($requireNativeSnapshots) { '1' } else { $null }
    # This is the visible interactive game being tested, not a background
    # helper. Let GD manage its own normal focused rendering/timing.
    $testProcess = Start-Process -FilePath (Join-Path $game 'GeometryDash.exe') -WorkingDirectory $game -PassThru
    # One activation request, never a fabricated foreground metric. The
    # harness still checks the real owning PID before/after measured frames.
    if (!('ChloriumResearchFocus' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class ChloriumResearchFocus {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr window, int mode);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint pid);
    public static uint ForegroundPid() {
        uint pid; GetWindowThreadProcessId(GetForegroundWindow(), out pid); return pid;
    }
}
'@
    }
    $focusDeadline = [DateTime]::UtcNow.AddSeconds(20)
    while (!$testProcess.HasExited -and !$testProcess.MainWindowHandle -and [DateTime]::UtcNow -lt $focusDeadline) {
        Start-Sleep -Milliseconds 100
        $testProcess.Refresh()
    }
    $focusRequested = $false
    if (!$ExternalFocus -and !$testProcess.HasExited -and $testProcess.MainWindowHandle) {
        [ChloriumResearchFocus]::ShowWindow($testProcess.MainWindowHandle,9) | Out-Null
        $focusRequested = [ChloriumResearchFocus]::SetForegroundWindow($testProcess.MainWindowHandle)
    }
    [ordered]@{requested=$focusRequested;observed=([ChloriumResearchFocus]::ForegroundPid() -eq $testProcess.Id)} |
        ConvertTo-Json | Set-Content -LiteralPath ($output + '.focus.json') -Encoding UTF8
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
    $env:CHLORIUM_EXPECTED_USER_MODS = $previousExpectedMods
    $env:CHLORIUM_VISUAL_PROBES = $previousProbes
    $env:CHLORIUM_REQUIRE_TARGETED_MATH_V3 = $previousTargetedMath
    $env:CHLORIUM_REQUIRE_NATIVE_SNAPSHOTS = $previousNativeSnapshots
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
    foreach ($snapshot in $temporarySettings) {
        [IO.File]::WriteAllBytes($snapshot.path, $snapshot.bytes)
        if ((Get-FileHash -LiteralPath $snapshot.path).Hash -ne (Get-FileHash -LiteralPath $snapshot.backup).Hash) {
            throw 'User settings restoration verification failed'
        }
    }
    foreach ($path in $stagedUserMods) {
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
    }
}
