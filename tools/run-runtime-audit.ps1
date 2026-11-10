#Requires -Version 7.0
param([switch]$Native, [switch]$VisualWatch, [switch]$WithUserMods, [switch]$ExternalFocus, [switch]$ExternalSampling, [switch]$OpacityShadow, [switch]$PassCapture, [switch]$FieldWatch, [switch]$RotationTrace, [switch]$AtlasStorageRegression, [string]$GameDirectory, [string]$Package, [string]$ProductionPackage, [string]$ReportDirectory,
    [string]$UserGameDirectory = 'C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash',
    [ValidateSet(0,133260800,129598490,88952972,139648964)][int]$FetchLevel = 0,
    [ValidateSet(133260800,129598490,88952972,139648964)][int]$Level = 133260800,
    [int]$BenchmarkSeconds = 900,
    [ValidateRange(0,99.999)][double]$FromPercent = 0,
    [ValidateRange(0.001,100)][double]$ToPercent = 100,
    [ValidateSet('map','orphan')][string]$StreamMode = 'orphan',
    [ValidateRange(0,100)][double[]]$CapturePercent = @(),[switch]$StartupDiagnostic)
$ErrorActionPreference = 'Stop'
$groupTraceDiagnostic = $env:CHLORIUM_GROUP_TRACE_R63 -eq '1'
$mixedTraceR74=$false
$wideRootR75=$false
if($env:CHLORIUM_ROTATION_TRACE_R72 -and !$RotationTrace){throw 'Inherited R72 rotation trace requires explicit diagnostic selection'}
if($RotationTrace -and ($Native -or $VisualWatch -or $WithUserMods -or $ExternalSampling -or $PassCapture -or $OpacityShadow -or $FieldWatch -or $groupTraceDiagnostic -or $AtlasStorageRegression -or $StartupDiagnostic -or !$ProductionPackage)){throw 'R72 rotation trace requires Chlorium active, isolated mods and no combined diagnostic'}
if($RotationTrace -and @('CHLORIUM_QUAD_READ_WATCH','CHLORIUM_OPACITY_INVOCATIONS','CHLORIUM_MOVE_COMMAND_CENSUS','CHLORIUM_NATIVE_PRODUCER_CAPTURE','CHLORIUM_NATIVE_BATCH_REPLAY','CHLORIUM_RESIDENT_NATIVE_REPLAY','CHLORIUM_OPAQUE_INPUT_CENSUS','CHLORIUM_OWNED_REGISTER_ORACLE','CHLORIUM_STARTUP_DEATH_WATCH','CHLORIUM_FIELD_WATCH_R69' | Where-Object {[Environment]::GetEnvironmentVariable($_,'Process') -eq '1'}).Count){throw 'R72 rotation trace cannot combine capture diagnostics'}
if($env:CHLORIUM_FIELD_WATCH_R69 -and !$FieldWatch){throw 'Inherited R69 field watch requires explicit diagnostic selection'}
if($FieldWatch -and ($Native -or $VisualWatch -or $WithUserMods -or $ExternalSampling -or $PassCapture -or $OpacityShadow -or $groupTraceDiagnostic -or $AtlasStorageRegression -or $StartupDiagnostic -or !$ProductionPackage)){throw 'R69 field watch requires Chlorium active, isolated mods and no combined diagnostic'}
if($FieldWatch -and @('CHLORIUM_QUAD_READ_WATCH','CHLORIUM_OPACITY_INVOCATIONS','CHLORIUM_MOVE_COMMAND_CENSUS','CHLORIUM_NATIVE_PRODUCER_CAPTURE','CHLORIUM_NATIVE_BATCH_REPLAY','CHLORIUM_RESIDENT_NATIVE_REPLAY','CHLORIUM_OPAQUE_INPUT_CENSUS','CHLORIUM_OWNED_REGISTER_ORACLE','CHLORIUM_STARTUP_DEATH_WATCH' | Where-Object {[Environment]::GetEnvironmentVariable($_,'Process') -eq '1'}).Count){throw 'R69 field watch cannot combine capture diagnostics'}
if($groupTraceDiagnostic -and ($Native -or $PassCapture -or $WithUserMods -or $ExternalSampling -or $OpacityShadow -or $VisualWatch -or $AtlasStorageRegression -or !$ProductionPackage)){
    throw 'R63 group trace requires Chlorium active with isolated mods and no combined renderer diagnostic'
}
if($groupTraceDiagnostic -and @('CHLORIUM_QUAD_READ_WATCH','CHLORIUM_OPACITY_INVOCATIONS','CHLORIUM_MOVE_COMMAND_CENSUS','CHLORIUM_NATIVE_PRODUCER_CAPTURE','CHLORIUM_NATIVE_BATCH_REPLAY','CHLORIUM_RESIDENT_NATIVE_REPLAY','CHLORIUM_OPAQUE_INPUT_CENSUS','CHLORIUM_OWNED_REGISTER_ORACLE','CHLORIUM_STARTUP_DEATH_WATCH' | Where-Object {[Environment]::GetEnvironmentVariable($_,'Process') -eq '1'}).Count){
    throw 'R63 group trace cannot combine other capture diagnostics'
}
if($env:CHLORIUM_NATIVE_PRODUCER_CAPTURE -eq '1'){
    if(!$Native -or !$PassCapture -or $WithUserMods){throw 'Producer observer requires the isolated native pass-capture profile'}
    if(@('CHLORIUM_QUAD_READ_WATCH','CHLORIUM_OPACITY_INVOCATIONS','CHLORIUM_MOVE_COMMAND_CENSUS','CHLORIUM_OWNED_REGISTER_ORACLE','CHLORIUM_STARTUP_DEATH_WATCH' | Where-Object {[Environment]::GetEnvironmentVariable($_,'Process') -eq '1'}).Count){throw 'Producer observer cannot combine hardware or invocation diagnostics'}
}
if($env:CHLORIUM_RESIDENT_NATIVE_STREAM -eq '1' -and $env:CHLORIUM_RESIDENT_NATIVE_REPLAY -ne '1'){throw 'Native vertex stream requires its explicit resident replay diagnostic'}
if($env:CHLORIUM_RESIDENT_NATIVE_REPLAY -eq '1'){
    if(!$Native -or !$PassCapture -or $WithUserMods){throw 'Resident replay requires the isolated native pass-capture profile'}
    if($env:CHLORIUM_NATIVE_BATCH_REPLAY -ne '1' -or $env:CHLORIUM_OPAQUE_INPUT_CENSUS -ne '1' -or $env:CHLORIUM_OPAQUE_PIXEL_PAIR -ne '1'){throw 'Resident replay requires its explicit native pixel replay chain'}
    if(@('CHLORIUM_ZERO_FRAGMENT_DISCARD','CHLORIUM_ZERO_NATIVE_HANDOFF','CHLORIUM_STARTUP_DEATH_WATCH','CHLORIUM_QUAD_READ_WATCH','CHLORIUM_OPACITY_INVOCATIONS','CHLORIUM_MOVE_COMMAND_CENSUS','CHLORIUM_OWNED_REGISTER_ORACLE' | Where-Object {[Environment]::GetEnvironmentVariable($_,'Process') -eq '1'}).Count){throw 'Resident replay cannot combine renderer or hardware diagnostics'}
    $taskResidentSource=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../research/R4-33/resident.vert'))
    if(!$env:CHLORIUM_RESIDENT_VERTEX_FILE -or [IO.Path]::GetFullPath($env:CHLORIUM_RESIDENT_VERTEX_FILE) -ne $taskResidentSource){throw 'Resident replay requires the pinned local vertex source'}
    if((Get-FileHash -LiteralPath $taskResidentSource -Algorithm SHA256).Hash.ToLowerInvariant() -ne 'bc8f9c88d908d769b440d86f10d494520a582b655f46de0c4348b7911cca9ad0'){throw 'Resident vertex source changed; rebuild and review before launch'}
}
if($env:CHLORIUM_ZERO_NATIVE_HANDOFF -eq '1' -and $env:CHLORIUM_ZERO_FRAGMENT_DISCARD -ne '1'){throw 'Native shader handoff requires its explicit zero fragment diagnostic'}
if($env:CHLORIUM_STARTUP_DEATH_WATCH -eq '1' -and !$PassCapture){throw 'Startup death observation requires the isolated pass-capture diagnostic'}
if($env:CHLORIUM_STARTUP_DEATH_WATCH -eq '1' -and @('CHLORIUM_QUAD_READ_WATCH','CHLORIUM_OPACITY_INVOCATIONS','CHLORIUM_OPAQUE_INPUT_CENSUS','CHLORIUM_MOVE_COMMAND_CENSUS','CHLORIUM_OWNED_REGISTER_ORACLE' | Where-Object {[Environment]::GetEnvironmentVariable($_,'Process') -eq '1'}).Count){throw 'Use only one startup death diagnostic per run'}
if($env:CHLORIUM_ZERO_FRAGMENT_DISCARD -eq '1' -and $env:CHLORIUM_NATIVE_BATCH_REPLAY -ne '1'){throw 'Zero fragment discard requires its explicit native batch replay'}
if($env:CHLORIUM_QUAD_READ_WATCH -eq '1' -and !$PassCapture){throw 'Native quad read observation requires the isolated pass-capture diagnostic'}
if($env:CHLORIUM_OPACITY_INVOCATIONS -eq '1' -and !$PassCapture){throw 'Native opacity invocation census requires the isolated pass-capture diagnostic'}
if($env:CHLORIUM_OPAQUE_INPUT_CENSUS -eq '1' -and !$PassCapture){throw 'Opaque input census requires the isolated pass-capture diagnostic'}
if($env:CHLORIUM_MOVE_COMMAND_CENSUS -eq '1' -and !$PassCapture){throw 'Movement command census requires the isolated pass-capture diagnostic'}
if($env:CHLORIUM_NATIVE_BATCH_REPLAY -eq '1' -and !$PassCapture){throw 'Native batch replay requires the isolated pass-capture diagnostic'}
if($env:CHLORIUM_NATIVE_BATCH_REPLAY -eq '1' -and $env:CHLORIUM_OPAQUE_PIXEL_PAIR -ne '1'){throw 'Native batch replay requires its explicit native pixel pair'}
if($env:CHLORIUM_MOVE_COMMAND_CENSUS -eq '1' -and @('CHLORIUM_OPAQUE_INPUT_CENSUS','CHLORIUM_OPACITY_INVOCATIONS','CHLORIUM_QUAD_READ_WATCH','CHLORIUM_OWNED_REGISTER_ORACLE' | Where-Object {[Environment]::GetEnvironmentVariable($_,'Process') -eq '1'}).Count){throw 'Use only one explicit native movement diagnostic per run'}
if($env:CHLORIUM_OPAQUE_PIXEL_PAIR -eq '1' -and $env:CHLORIUM_OPAQUE_INPUT_CENSUS -ne '1'){throw 'Native batch pixel pair requires its explicit opaque input census'}
if($env:CHLORIUM_OPAQUE_INPUT_CENSUS -eq '1' -and ($env:CHLORIUM_OPACITY_INVOCATIONS -eq '1' -or $env:CHLORIUM_QUAD_READ_WATCH -eq '1' -or $env:CHLORIUM_OWNED_REGISTER_ORACLE -eq '1')){throw 'Use only one explicit native input diagnostic per run'}
if($env:CHLORIUM_OPACITY_INVOCATIONS -eq '1' -and $env:CHLORIUM_QUAD_READ_WATCH -eq '1'){throw 'Use only one hardware breakpoint diagnostic per run'}
if($env:CHLORIUM_OWNED_REGISTER_ORACLE -eq '1' -and !$PassCapture){throw 'Owned register oracle requires the isolated pass-capture diagnostic'}
$root = Split-Path -Parent $PSScriptRoot
if ($FromPercent -ge $ToPercent) { throw 'The target percentage range must increase' }
if ($CapturePercent.Count -gt 16 -or ($CapturePercent | Where-Object { $_ -lt $FromPercent -or $_ -ge $ToPercent })) {
    throw 'Use at most 16 capture percentages inside the measured range (end excluded)'
}
if ($CapturePercent.Count -and !$VisualWatch) { throw 'Capture percentages require VisualWatch' }
$game = if ($GameDirectory) { $GameDirectory } else { Join-Path $root 'test-game' }
$destination = Join-Path $game 'geode\mods\local.chlorium.audit.geode'
$productionDestination = Join-Path $game 'geode\mods\local.chlorium.geode'
. (Join-Path $PSScriptRoot 'ModProfile.ps1')
if(!$WithUserMods) {
    $isolatedProfile=Get-ResearchModProfile -ModsDirectory (Join-Path $game 'geode/mods') -ProfileRoot (Join-Path $env:LOCALAPPDATA 'GeometryDash') -AllowedIds @('geode.node-ids')
    if(@($isolatedProfile.mods).Count -ne 1){throw 'Isolated profile requires the node-ids dependency'}
}
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
    $supportsContinuous = $binary.Contains('CHLORIUM_RESIDENT_CONTINUOUS_AUDIT_R57')
    $supportsDirectPagesR65 = $binary.Contains('CHLORIUM_DIRECT_VERTEX_AUDIT_R65')
    $supportsAdaptivePagesR66 = $binary.Contains('CHLORIUM_ADAPTIVE_VERTEX_AUDIT_R66')
    $supportsDirectGuardR67 = $binary.Contains('CHLORIUM_DIRECT_VERTEX_AUDIT_R67')
    $supportsNativeAuthorityR67 = $binary.Contains('CHLORIUM_NATIVE_AUTHORITY_AUDIT_R67')
    $supportsCanonicalCopyR73 = $binary.Contains('CHLORIUM_CANONICAL_GPU_COPY_AUDIT_R73')
    $supportsMixedTraceR74 = $binary.Contains('CHLORIUM_MIXED_TRACE_AUDIT_R74')
    $supportsWideRootR75 = $binary.Contains('CHLORIUM_WIDE_ROOT_AUDIT_R75')
    if([bool]$FieldWatch -ne $binary.Contains('CHLORIUM_FIELD_WATCH_AUDIT_R69')){throw 'R69 field watch requires its matching independent auditor'}
    if([bool]$RotationTrace -ne $binary.Contains('CHLORIUM_ROTATION_TRACE_AUDIT_R72')){throw 'R72 rotation trace requires its matching independent auditor'}
    if($supportsDirectPagesR65 -and ($Native -or $WithUserMods -or $groupTraceDiagnostic)){throw 'R65 campaign requires Chlorium active, isolated mods and no movement trace'}
    if($groupTraceDiagnostic -and !$binary.Contains('CHLORIUM_GROUP_TRACE_AUDIT_R63')){throw 'R63 trace requires its identified independent auditor'}
    $opacityHashAuditBinary = $binary.Contains('CHLORIUM_OPACITY_HASH_AUDIT_R36')
    if (!$binary.Contains('CHLORIUM_AUDIT_120FPS_PACING_R4')) { throw 'Rebuild the auditor with verified 120 FPS pacing before launching this campaign' }
    $opacityShadowBinary = $binary.Contains('CHLORIUM_OPACITY_PREFIX_SHADOW_V1')
    $passCaptureBinary = $binary.Contains('CHLORIUM_NATIVE_PASS_CAPTURE_V3')
    if($env:CHLORIUM_ZERO_NATIVE_HANDOFF -eq '1' -and !$binary.Contains('CHLORIUM_ZERO_NATIVE_HANDOFF_R46')){throw 'Rebuild the native shader handoff before this diagnostic'}
    if($env:CHLORIUM_RESIDENT_NATIVE_REPLAY -eq '1' -and !$binary.Contains('CHLORIUM_RESIDENT_NATIVE_REPLAY_R49')){throw 'Rebuild the resident native replay before this diagnostic'}
    if($env:CHLORIUM_RESIDENT_NATIVE_REPLAY -eq '1' -and !$binary.Contains('CHLORIUM_RESIDENT_NATIVE_CACHE_R50')){throw 'Rebuild the persistent resident cache before this diagnostic'}
    if($env:CHLORIUM_RESIDENT_NATIVE_STREAM -eq '1' -and !$binary.Contains('CHLORIUM_RESIDENT_NATIVE_STREAM_R51')){throw 'Rebuild the native vertex stream before this diagnostic'}
    if($env:CHLORIUM_RESIDENT_NATIVE_STREAM -eq '1' -and !$binary.Contains('CHLORIUM_RESIDENT_NATIVE_SLOT_CACHE_R52')){throw 'Rebuild the native slot cache before this diagnostic'}
    if($env:CHLORIUM_RESIDENT_NATIVE_STREAM -eq '1' -and !$binary.Contains('CHLORIUM_RESIDENT_EXACT_GPU_UPDATES_R53')){throw 'Rebuild the exact GPU updates before this diagnostic'}
    if($env:CHLORIUM_RESIDENT_NATIVE_STREAM -eq '1' -and !$binary.Contains('CHLORIUM_RESIDENT_OVERWRITE_BARRIER_R56')){throw 'Rebuild the GPU overwrite barrier before this diagnostic'}
    if($env:CHLORIUM_NATIVE_PRODUCER_CAPTURE -eq '1' -and !$binary.Contains('CHLORIUM_NATIVE_PRODUCER_SHADOW_R54')){throw 'Rebuild the native producer observer before this diagnostic'}
    if($env:CHLORIUM_NATIVE_PRODUCER_CAPTURE -eq '1' -and !$binary.Contains('CHLORIUM_NATIVE_PRODUCER_REPLAY_R55')){throw 'Rebuild the event packet replay before this diagnostic'}
    if($env:CHLORIUM_RESEARCH_SOURCE_LEVEL_ID -eq '1' -and !$binary.Contains('CHLORIUM_RESEARCH_SOURCE_LEVEL_ID_R45')){throw 'Rebuild the source level identity experiment before this diagnostic'}
    if($env:CHLORIUM_STARTUP_DEATH_WATCH -eq '1' -and !$binary.Contains('CHLORIUM_STARTUP_DEATH_WATCH_R45')){throw 'Rebuild the startup death observer before this diagnostic'}
    if($env:CHLORIUM_ZERO_FRAGMENT_DISCARD -eq '1' -and !$binary.Contains('CHLORIUM_ZERO_FRAGMENT_DISCARD_R44')){throw 'Rebuild the zero fragment discard before this diagnostic'}
    if($env:CHLORIUM_NATIVE_BATCH_REPLAY -eq '1' -and !$binary.Contains('CHLORIUM_NATIVE_BATCH_REPLAY_R43')){throw 'Rebuild the native batch replay before this diagnostic'}
    if($env:CHLORIUM_OWNED_REGISTER_ORACLE -eq '1' -and !$binary.Contains('CHLORIUM_OWNED_REGISTER_ORACLE_V1')){throw 'Rebuild the owned register oracle before this diagnostic'}
    if($env:CHLORIUM_QUAD_READ_WATCH -eq '1' -and !$binary.Contains('CHLORIUM_NATIVE_QUAD_READ_WATCH_DIAGNOSTIC_V1')){throw 'Rebuild the native quad read observer before this diagnostic'}
    if($env:CHLORIUM_OPACITY_INVOCATIONS -eq '1' -and !$binary.Contains('CHLORIUM_NATIVE_OPACITY_INVOCATIONS_R37')){throw 'Rebuild the native opacity invocation census before this diagnostic'}
    if($env:CHLORIUM_OPAQUE_INPUT_CENSUS -eq '1' -and !$binary.Contains('CHLORIUM_OPAQUE_INPUT_CENSUS_R41')){throw 'Rebuild the opaque input census before this diagnostic'}
    if($env:CHLORIUM_OPAQUE_PIXEL_PAIR -eq '1' -and !$binary.Contains('CHLORIUM_NATIVE_BATCH_PIXEL_PAIR_R41')){throw 'Rebuild the native batch pixel pair before this diagnostic'}
    if($env:CHLORIUM_MOVE_COMMAND_CENSUS -eq '1' -and !$binary.Contains('CHLORIUM_NATIVE_MOVEMENT_CENSUS_R42')){throw 'Rebuild the native movement census before this diagnostic'}
    if($binary.Contains('CHLORIUM_NATIVE_PASS_CAPTURE_V') -and !$passCaptureBinary){throw 'Unsupported native pass capture version; rebuild the diagnostic'}
    if($passCaptureBinary -ne [bool]$PassCapture){throw 'Native pass capture requires its explicit diagnostic flag and matching auditor'}
    if($PassCapture -and (!$Native -or $VisualWatch -or $WithUserMods -or $ExternalSampling -or $OpacityShadow -or !$measurementOnly -or !$supportsCanonical)){throw 'Native pass capture is an isolated native diagnostic, never a performance comparison'}
    if($opacityShadowBinary -ne [bool]$OpacityShadow){throw 'Opacity shadow requires its explicit diagnostic flag and matching auditor'}
    if($OpacityShadow -and (!$Native -or $VisualWatch -or $WithUserMods -or $ExternalSampling -or !$measurementOnly -or !$supportsCanonical)){throw 'Opacity shadow requires its isolated native diagnostic profile without image capture or sampling'}
    if($ExternalSampling -and ((!$Native -and (!$supportsAdaptivePagesR66 -or !$ProductionPackage)) -or $VisualWatch -or $WithUserMods -or !$measurementOnly -or !$binary.Contains('CHLORIUM_EXTERNAL_SAMPLING_AUDIT_V2'))){throw 'External sampling requires its isolated native or identified R66 diagnostic; never use it as a performance run'}
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
$requireOpacityHash = $false
$rendererTimingDiagnostic = $false
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
        if ($binary.Contains('CHLORIUM_RESIDENT_CONTINUOUS_R57')) {
            if(!$supportsContinuous -or !$supportsCanonical){throw 'Continuous renderer requires its independent R57 activation and native-body auditor'}
            if($binary.Contains('CHLORIUM_NATIVE_CANONICAL_BACKEND_V10')){throw 'Continuous renderer must not contain the old canonical core'}
        } elseif ($binary.Contains('CHLORIUM_NATIVE_CANONICAL_BACKEND_V10')) {
            if (!$supportsCanonical -or !$binary.Contains('CHLORIUM_CANONICAL_AREA_QUOTIENTS_V10')) { throw 'Canonical core requires its independent V10 proof auditor' }
            if($binary.Contains('CHLORIUM_NATIVE_OPACITY_HASH_R36')){
                if(!$opacityHashAuditBinary){throw 'Rebuild the independent R36 opacity hash auditor'}
                $requireOpacityHash=$true
            }
        } elseif ($binary.Contains('CHLORIUM_EXPERIMENTAL_NATIVE_SNAPSHOTS_V1')) {
            if (!$supportsNativeSnapshots) { throw 'Rebuild the independent measurement module to verify native snapshot activation' }
            $requireNativeSnapshots = $true
        }
        if($groupTraceDiagnostic -ne $binary.Contains('CHLORIUM_GROUP_STATE_TRACE_R63')){throw 'R63 trace candidate and explicit diagnostic selection must match'}
        if([bool]$FieldWatch -ne $binary.Contains('CHLORIUM_NATIVE_FIELD_WATCH_R69')){throw 'R69 field watch candidate and diagnostic selection must match'}
        if([bool]$RotationTrace -ne $binary.Contains('CHLORIUM_NATIVE_ROTATION_TRACE_R72')){throw 'R72 rotation trace candidate and diagnostic selection must match'}
        $mixedTraceR74=$binary.Contains('CHLORIUM_MIXED_NATIVE_TRACE_R74')
        if($mixedTraceR74 -ne $supportsMixedTraceR74){throw 'R74 joint trace requires its matching independent auditor'}
        $wideRootR75=$binary.Contains('CHLORIUM_WIDE_ROOT_SAMPLE_R75')
        if($wideRootR75 -ne $supportsWideRootR75){throw 'R75 root/geometry sampling requires its matching independent auditor'}
        $rendererTimingDiagnostic = $binary.Contains('CHLORIUM_RENDERER_TIMING_R60')
        $rendererVariant = if($binary.Contains('CHLORIUM_CANONICAL_GPU_COPY_R73')){'canonical-gpu-copy-r73'}elseif($binary.Contains('CHLORIUM_DIRECT_CONTEXT_GUARD_R67')){'direct-pages-r67'}elseif($binary.Contains('CHLORIUM_ADAPTIVE_VERTEX_PAGES_R66')){'adaptive-pages-r66'}elseif($binary.Contains('CHLORIUM_DIRECT_VERTEX_PAGES_R65')){'direct-pages-r65'}elseif($binary.Contains('CHLORIUM_R61_UNTIMED_CONTROL_R65')){'stream-control-r65'}else{'previous-profile'}
        if($rendererVariant -eq 'canonical-gpu-copy-r73' -and !$supportsCanonicalCopyR73){throw 'R73 requires its matching GPU-copy auditor and mode label'}
        if($rendererVariant -in @('direct-pages-r67','canonical-gpu-copy-r73') -and !$supportsDirectGuardR67){throw 'R67 requires its matching context-recovery auditor and mode label'}
        if($rendererVariant -in @('direct-pages-r67','canonical-gpu-copy-r73') -and (!$supportsNativeAuthorityR67 -or !$binary.Contains('CHLORIUM_NATIVE_VERTEX_AUTHORITY_R67'))){throw 'R67 must keep native vertex storage authoritative and report its copies'}
        if($rendererVariant -eq 'adaptive-pages-r66' -and !$supportsAdaptivePagesR66){throw 'R66 requires its matching identified auditor and mode label'}
        if($ExternalSampling -and $rendererVariant -ne 'adaptive-pages-r66'){throw 'Candidate sampling is restricted to the identified R66 renderer'}
        if($rendererVariant -ne 'previous-profile' -and (!$supportsDirectPagesR65 -or $rendererTimingDiagnostic)){throw 'R65 performance profiles require their identified auditor without renderer timing'}
        if($supportsDirectPagesR65 -and $rendererVariant -eq 'previous-profile'){throw 'R65 auditor cannot mislabel another renderer'}
        $binary = $null
    } finally { $productionZip.Dispose() }
}
# Generate reports explicitly after a completed pair. A first run in a new
# directory must not erase an existing readable report with an empty one.
$previous = $env:CHLORIUM_AUDIT_OUTPUT
$previousNative = $env:CHLORIUM_AUDIT_NATIVE
$previousOpacityHash = $env:CHLORIUM_REQUIRE_OPACITY_HASH_R36
$previousFieldWatch = $env:CHLORIUM_FIELD_WATCH_R69
$previousRotationTrace = $env:CHLORIUM_ROTATION_TRACE_R72
$previousSampling = $env:CHLORIUM_EXTERNAL_SAMPLING
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
        $isolatedProfile = Get-ResearchModProfile -ModsDirectory (Join-Path $game 'geode/mods') -ProfileRoot (Join-Path $env:LOCALAPPDATA 'GeometryDash') -AllowedIds @('geode.node-ids','local.chlorium.audit')
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
        target_fps = 120
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
        external_sampling_requested = [bool]$ExternalSampling
        opacity_shadow_diagnostic = [bool]$OpacityShadow
        native_pass_capture_diagnostic = [bool]$PassCapture
        native_field_watch_diagnostic = [bool]$FieldWatch
        native_rotation_trace_diagnostic = [bool]$RotationTrace
        mixed_native_trace_diagnostic = [bool]$mixedTraceR74
        wide_root_geometry_diagnostic = [bool]$wideRootR75
        movement_command_census_diagnostic = ($env:CHLORIUM_MOVE_COMMAND_CENSUS -eq '1')
        native_batch_replay_diagnostic = ($env:CHLORIUM_NATIVE_BATCH_REPLAY -eq '1')
        zero_fragment_discard_diagnostic = ($env:CHLORIUM_ZERO_FRAGMENT_DISCARD -eq '1')
        startup_death_diagnostic = ($env:CHLORIUM_STARTUP_DEATH_WATCH -eq '1')
        research_source_level_id = ($env:CHLORIUM_RESEARCH_SOURCE_LEVEL_ID -eq '1')
        zero_native_handoff_diagnostic = ($env:CHLORIUM_ZERO_NATIVE_HANDOFF -eq '1')
        resident_native_replay_diagnostic = ($env:CHLORIUM_RESIDENT_NATIVE_REPLAY -eq '1')
        resident_native_cache_diagnostic = ($env:CHLORIUM_RESIDENT_NATIVE_REPLAY -eq '1')
        resident_native_stream_diagnostic = ($env:CHLORIUM_RESIDENT_NATIVE_STREAM -eq '1')
        resident_exact_gpu_updates_diagnostic = ($env:CHLORIUM_RESIDENT_NATIVE_STREAM -eq '1')
        renderer_timing_diagnostic = $rendererTimingDiagnostic
        renderer_variant = $rendererVariant
        group_state_trace_diagnostic = $groupTraceDiagnostic
        startup_diagnostic = [bool]$StartupDiagnostic
        native_producer_capture_diagnostic = ($env:CHLORIUM_NATIVE_PRODUCER_CAPTURE -eq '1')
        resident_vertex_sha256 = $(if($env:CHLORIUM_RESIDENT_NATIVE_REPLAY -eq '1'){(Get-FileHash -LiteralPath $taskResidentSource -Algorithm SHA256).Hash.ToLowerInvariant()}else{$null})
        native_quad_reads_diagnostic = $env:CHLORIUM_QUAD_READ_WATCH -eq '1'
        native_opacity_invocations_diagnostic = $env:CHLORIUM_OPACITY_INVOCATIONS -eq '1'
        opaque_input_census_diagnostic = $env:CHLORIUM_OPAQUE_INPUT_CENSUS -eq '1'
        native_batch_pixel_pair_diagnostic = $env:CHLORIUM_OPAQUE_PIXEL_PAIR -eq '1'
        owned_register_oracle_diagnostic = $env:CHLORIUM_OWNED_REGISTER_ORACLE -eq '1'
        native_opacity_hash_r36_required = $requireOpacityHash
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
    $env:CHLORIUM_EXTERNAL_SAMPLING = if ($ExternalSampling) { '1' } else { $null }
    $env:CHLORIUM_FIELD_WATCH_R69 = if ($FieldWatch) { '1' } else { $null }
    $env:CHLORIUM_ROTATION_TRACE_R72 = if ($RotationTrace) { '1' } else { $null }
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
    $env:CHLORIUM_REQUIRE_OPACITY_HASH_R36 = if($requireOpacityHash){'1'}else{$null}
    # This is the visible interactive game being tested, not a background
    # helper. Let GD manage its own normal focused rendering/timing.
    $testProcess = Start-Process -FilePath (Join-Path $game 'GeometryDash.exe') -WorkingDirectory $game -WindowStyle Normal -PassThru
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
    $env:CHLORIUM_EXTERNAL_SAMPLING = $previousSampling
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
    $env:CHLORIUM_REQUIRE_OPACITY_HASH_R36 = $previousOpacityHash
    $env:CHLORIUM_FIELD_WATCH_R69 = $previousFieldWatch
    $env:CHLORIUM_ROTATION_TRACE_R72 = $previousRotationTrace
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
