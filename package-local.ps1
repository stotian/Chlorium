param([string]$OutputDirectory = 'dist', [switch]$Audit, [switch]$Experimental)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root = $PSScriptRoot
$cachePath = Join-Path $root 'build\CMakeCache.txt'
if (!(Test-Path -LiteralPath $cachePath)) { throw 'Configure and compile before packaging' }
$cacheText = Get-Content -LiteralPath $cachePath -Raw
$auditBuild = $cacheText -match '(?m)^CHLORIUM_RUNTIME_AUDIT:BOOL=ON\r?$'
$harnessOnly = $cacheText -match '(?m)^CHLORIUM_AUDIT_HARNESS_ONLY:BOOL=ON\r?$'
$orbitProbe = $cacheText -match '(?m)^CHLORIUM_ORBIT_BENCHMARK:BOOL=ON\r?$'
$nativeQuads = $cacheText -match '(?m)^CHLORIUM_EXPERIMENTAL_NATIVE_QUADS:BOOL=ON\r?$'
$nativeVertex = $cacheText -match '(?m)^CHLORIUM_NATIVE_VERTEX_PARITY:BOOL=ON\r?$'
$baseRenderer = $nativeQuads -and $nativeVertex
$snapshots = $cacheText -match '(?m)^CHLORIUM_EXPERIMENTAL_NATIVE_SNAPSHOTS:BOOL=ON\r?$'
$staticRenderer = $cacheText -match '(?m)^CHLORIUM_EXPERIMENTAL_STATIC_RENDERER:BOOL=ON\r?$'
$intrusiveProfiler = $cacheText -match '(?m)^CHLORIUM_ENABLE_PROFILER:BOOL=ON\r?$'
$experimentalMoves = $cacheText -match '(?m)^CHLORIUM_EXPERIMENTAL_NATIVE_MOVES:BOOL=ON\r?$'
$measurementOnly = $cacheText -match '(?m)^CHLORIUM_MEASUREMENT_ONLY:BOOL=ON\r?$'
$streaming = $cacheText -match '(?m)^CHLORIUM_EXPERIMENTAL_NATIVE_STREAMING:BOOL=ON\r?$'
$area = $cacheText -match '(?m)^CHLORIUM_EXPERIMENTAL_AREA_DIVISIONS:BOOL=ON\r?$'
$matrix = $cacheText -match '(?m)^CHLORIUM_EXPERIMENTAL_MATRIX_REUSE:BOOL=ON\r?$'
$affine = $cacheText -match '(?m)^CHLORIUM_EXPERIMENTAL_AFFINE_POINT:BOOL=ON\r?$'
$areaMath = $cacheText -match '(?m)^CHLORIUM_EXPERIMENTAL_AREA_MATH:BOOL=ON\r?$'
$pipeline = $cacheText -match '(?m)^CHLORIUM_EXPERIMENTAL_MATH_PIPELINE:BOOL=ON\r?$'
$heap = $cacheText -match '(?m)^CHLORIUM_EXPERIMENTAL_HEAP_RELEASE:BOOL=ON\r?$'
$retiredAtlas = $cacheText -match '(?m)^CHLORIUM_NATIVE_ATLAS_CACHE:BOOL=ON\r?$'
if (!$Audit -and $retiredAtlas) { throw 'The retired GPU atlas cache cannot be packaged as a normal mod' }
if ((($snapshots -and !$baseRenderer) -or $streaming -or $area -or $matrix -or $affine -or $areaMath -or $heap) -and !$Experimental) { throw 'Use -Experimental with a separate output directory for alternate research builds' }
if (!$Audit -and !$Experimental -and !$baseRenderer) { throw 'Legacy Chlorium is archived; the default package requires the native-vertex renderer' }
if ($Experimental -and ((!$snapshots -and !$streaming -and !$area -and !$matrix -and !$affine -and !$areaMath -and !$heap) -or $Audit -or
    [IO.Path]::GetFullPath((Join-Path $root $OutputDirectory)) -eq (Join-Path $root 'dist'))) {
    throw 'An experimental package requires an isolated experimental build and output directory'
}
if ([bool]$Audit -ne $auditBuild) {
    throw 'Package mode does not match the configured DLL: audit builds cannot be distributed as Chlorium'
}
if (!$Audit -and ($harnessOnly -or $orbitProbe -or $intrusiveProfiler -or $experimentalMoves -or $measurementOnly)) {
    throw 'A development harness is enabled: create a normal build before packaging Chlorium'
}
if (!$Audit -and $staticRenderer -and !$snapshots) {
    throw 'The incomplete static renderer is development-only; create a normal native-compatible build'
}
$out = Join-Path $root $OutputDirectory
New-Item -ItemType Directory -Force $out | Out-Null
$manifest = Get-Content (Join-Path $root 'mod.json') -Raw | ConvertFrom-Json
if ($manifest.gd.win -ne '2.2081') { throw 'Unexpected game version' }
$id = if ($Audit) { 'local.chlorium.audit' } else { $manifest.id }
$manifest.id = $id
if ($Audit) { $manifest.name = 'Chlorium runtime audit' }
elseif ($Experimental) {
    $manifest.name = if ($baseRenderer) { 'Chlorium CPU/GPU renderer' } elseif ($nativeQuads) { 'Chlorium exact native quad GPU experiment' } elseif ($snapshots) { 'Chlorium native snapshot GPU experiment' } elseif ($heap) { 'Chlorium unused heap cache experiment' } elseif ($pipeline) { 'Chlorium exact math pipeline experiment' } elseif ($areaMath) { 'Chlorium Area decomposition experiment' } elseif ($affine) { 'Chlorium affine point experiment' } elseif ($matrix) { 'Chlorium matrix reuse experiment' } elseif ($area) { 'Chlorium Area arithmetic experiment' } else { 'Chlorium streaming experiment' }
    # Geode accepts a known prerelease tag and one numeric suffix only.
    # Keep the experiment name in `name`; extra suffixes prevent loading.
    $manifest.version = if ($baseRenderer) { $manifest.version } elseif ($nativeQuads) { 'v0.2.0-alpha.610' } elseif ($snapshots) { 'v0.2.0-alpha.608' } elseif ($heap) { 'v0.2.0-alpha.607' } elseif ($pipeline) { 'v0.2.0-alpha.606' } elseif ($areaMath) { 'v0.2.0-alpha.605' } elseif ($affine) { 'v0.2.0-alpha.604' } elseif ($matrix) { 'v0.2.0-alpha.603' } elseif ($area) { 'v0.2.0-alpha.602' } else { 'v0.2.0-alpha.601' }
}
else {
    # Native packages expose only controls used by their active implementation.
    $manifest.settings.enabled.name = 'Activer Chlorium'
    $manifest.settings.PSObject.Properties.Remove('ingame_enable')
    $manifest.settings.PSObject.Properties.Remove('use_mapped_buffers')
}
if ($Experimental -and (!$snapshots -or $nativeQuads)) {
    $manifest.settings.enabled.name = 'Activer cette experience'
    $manifest.settings.PSObject.Properties.Remove('ingame_enable')
    $manifest.settings.PSObject.Properties.Remove('use_mapped_buffers')
}
$dll = Join-Path $root 'build\local.chlorium.dll'
if ($manifest.version -notmatch '^v?[0-9]+\.[0-9]+\.[0-9]+(?:-(?:alpha|beta|prerelease)(?:\.[0-9]+)?)?$') {
    throw 'Version must use the restricted Geode prerelease format'
}
if (!(Test-Path $dll)) { throw 'Compile first with build-local.cmd' }
$binaryText = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($dll))
$identity = if ($Audit) { 'CHLORIUM_RUNTIME_AUDIT_BINARY_V1' } else { 'CHLORIUM_NORMAL_BINARY_V1' }
if (!$binaryText.Contains($identity)) {
    throw 'The DLL build identity does not match this package: a fresh build is required'
}
if (!$Audit -and !$Experimental -and
    (!$binaryText.Contains('CHLORIUM_NATIVE_CANONICAL_BACKEND_V10') -or
      !$binaryText.Contains('CHLORIUM_CANONICAL_AREA_QUOTIENTS_V10') -or
      $binaryText.Contains('CHLORIUM_NATIVE_ATLAS_CACHE_IMPLEMENTATION_V1'))) {
    throw 'The base renderer requires canonical V10 identity and no retired atlas cache'
}
if ($streaming -ne $binaryText.Contains('CHLORIUM_EXPERIMENTAL_NATIVE_STREAMING_V1')) {
    throw 'Streaming build identity mismatch: rebuild before packaging'
}
if ($area -ne $binaryText.Contains('CHLORIUM_EXPERIMENTAL_AREA_DIVISIONS_V1')) { throw 'Area build identity mismatch: rebuild before packaging' }
if ($matrix -ne $binaryText.Contains('CHLORIUM_EXPERIMENTAL_MATRIX_REUSE_V1')) { throw 'Matrix build identity mismatch: rebuild before packaging' }
if ($matrix -and !$binaryText.Contains('CHLORIUM_MATRIX_PERSISTENT_REUSE_V3')) { throw 'Matrix revision mismatch: rebuild matrix.3 before packaging' }
if ($affine -ne $binaryText.Contains('CHLORIUM_EXPERIMENTAL_AFFINE_POINT_V1')) { throw 'Affine point build identity mismatch: rebuild before packaging' }
if ($affine -and !$binaryText.Contains('CHLORIUM_AFFINE_POINT_REVISION_V2')) { throw 'Affine point revision mismatch: rebuild before packaging' }
if ($areaMath -ne $binaryText.Contains('CHLORIUM_EXPERIMENTAL_AREA_MATH_V1')) { throw 'Area math build identity mismatch: rebuild before packaging' }
if ($areaMath -and !$binaryText.Contains('CHLORIUM_AREA_MATH_TARGETED_CALLS_V3')) { throw 'Area math revision mismatch: rebuild before packaging' }
if ($pipeline -ne $binaryText.Contains('CHLORIUM_EXPERIMENTAL_MATH_PIPELINE_V1') -or ($pipeline -and (!$matrix -or !$areaMath))) { throw 'Combined math pipeline identity/configuration mismatch' }
if ($heap -ne $binaryText.Contains('CHLORIUM_EXPERIMENTAL_HEAP_RELEASE_V1')) { throw 'Heap release identity mismatch' }
if ($snapshots -ne $binaryText.Contains('CHLORIUM_EXPERIMENTAL_NATIVE_SNAPSHOTS_V1')) { throw 'Native snapshot build identity mismatch' }
if ($snapshots -and $binaryText.Contains('CHLORIUM_NATIVE_ATLAS_CACHE_IMPLEMENTATION_V1')) { throw 'Snapshot builds must not include the retired atlas cache' }
if ($nativeQuads -and (!$binaryText.Contains('CHLORIUM_NATIVE_CANONICAL_BACKEND_V10') -or !$binaryText.Contains('CHLORIUM_CANONICAL_AREA_QUOTIENTS_V10'))) { throw 'Canonical renderer identity mismatch: rebuild before packaging' }
if ($baseRenderer -and $binaryText.Contains('CHLORIUM_NATIVE_VERTEX_BACKEND_V5')) { throw 'Retired V5 renderer cannot be packaged as the canonical renderer' }
$binaryText = $null
$package = Join-Path $out "$id.geode"
if (Test-Path -LiteralPath $package) { Remove-Item -LiteralPath $package }
$zip = [IO.Compression.ZipFile]::Open($package, [IO.Compression.ZipArchiveMode]::Create)
try {
    $entry = $zip.CreateEntry('mod.json')
    $writer = [IO.StreamWriter]::new($entry.Open(), [Text.UTF8Encoding]::new($false))
    try { $writer.Write(($manifest | ConvertTo-Json -Depth 12)) } finally { $writer.Dispose() }
    foreach ($name in @('about.md','changelog.md','support.md','logo.png','LICENSE.md','COPYING','NOTICE.md','THIRD_PARTY_NOTICES.md')) {
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, (Join-Path $root $name), $name) | Out-Null
    }
    Get-ChildItem (Join-Path $root 'LICENSES') -File | ForEach-Object {
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $_.FullName, "LICENSES/$($_.Name)") | Out-Null
    }
    [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $dll, "$id.dll") | Out-Null
    $names = @{}
    Get-ChildItem (Join-Path $root 'resources') -File -Recurse | ForEach-Object {
        if ($names.ContainsKey($_.Name)) { throw "Duplicate resource: $($_.Name)" }
        $names[$_.Name] = $true
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $_.FullName, "resources/$id/$($_.Name)") | Out-Null
    }
} finally { $zip.Dispose() }
$zip = [IO.Compression.ZipFile]::OpenRead($package)
try {
    if (!$zip.GetEntry("$id.dll") -or !$zip.GetEntry("resources/$id/objectDrawing.vert") -or !$zip.GetEntry("resources/$id/shared.h")) { throw 'Package incomplete' }
    Write-Output "Package verified: $($zip.Entries.Count) entries"
} finally { $zip.Dispose() }
Get-FileHash $package | Format-List
