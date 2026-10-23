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
$staticRenderer = $cacheText -match '(?m)^CHLORIUM_EXPERIMENTAL_STATIC_RENDERER:BOOL=ON\r?$'
$intrusiveProfiler = $cacheText -match '(?m)^CHLORIUM_ENABLE_PROFILER:BOOL=ON\r?$'
$experimentalMoves = $cacheText -match '(?m)^CHLORIUM_EXPERIMENTAL_NATIVE_MOVES:BOOL=ON\r?$'
$measurementOnly = $cacheText -match '(?m)^CHLORIUM_MEASUREMENT_ONLY:BOOL=ON\r?$'
$streaming = $cacheText -match '(?m)^CHLORIUM_EXPERIMENTAL_NATIVE_STREAMING:BOOL=ON\r?$'
$area = $cacheText -match '(?m)^CHLORIUM_EXPERIMENTAL_AREA_DIVISIONS:BOOL=ON\r?$'
$matrix = $cacheText -match '(?m)^CHLORIUM_EXPERIMENTAL_MATRIX_REUSE:BOOL=ON\r?$'
$retiredAtlas = $cacheText -match '(?m)^CHLORIUM_NATIVE_ATLAS_CACHE:BOOL=ON\r?$'
if (!$Audit -and $retiredAtlas) { throw 'The retired GPU atlas cache cannot be packaged as a normal mod' }
if (($streaming -or $area -or $matrix) -and !$Experimental) { throw 'Use -Experimental with a separate output directory for experimental builds' }
if ($Experimental -and ((!$streaming -and !$area -and !$matrix) -or $Audit -or
    [IO.Path]::GetFullPath((Join-Path $root $OutputDirectory)) -eq (Join-Path $root 'dist'))) {
    throw 'An experimental package requires an isolated experimental build and output directory'
}
if ([bool]$Audit -ne $auditBuild) {
    throw 'Package mode does not match the configured DLL: audit builds cannot be distributed as Chlorium'
}
if (!$Audit -and ($harnessOnly -or $orbitProbe -or $intrusiveProfiler -or $experimentalMoves -or $measurementOnly)) {
    throw 'A development harness is enabled: create a normal build before packaging Chlorium'
}
if (!$Audit -and $staticRenderer) {
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
    $manifest.name = if ($matrix) { 'Chlorium matrix reuse experiment' } elseif ($area) { 'Chlorium Area arithmetic experiment' } else { 'Chlorium streaming experiment' }
    # Geode accepts a known prerelease tag and one numeric suffix only.
    # Keep the experiment name in `name`; extra suffixes prevent loading.
    $manifest.version = if ($matrix) { 'v0.2.0-alpha.603' } elseif ($area) { 'v0.2.0-alpha.602' } else { 'v0.2.0-alpha.601' }
}
else {
    # Native packages expose only controls used by their active implementation.
    $manifest.settings.enabled.name = 'Activer Chlorium'
    $manifest.settings.PSObject.Properties.Remove('ingame_enable')
    $manifest.settings.PSObject.Properties.Remove('use_mapped_buffers')
}
if ($Experimental) {
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
    (!$binaryText.Contains('CHLORIUM_NATIVE_GPU_TRANSFERS_ONLY_V1') -or
      $binaryText.Contains('CHLORIUM_NATIVE_ATLAS_CACHE_IMPLEMENTATION_V1'))) {
    throw 'Normal packages require native GPU transfers and no retired atlas cache'
}
if ($streaming -ne $binaryText.Contains('CHLORIUM_EXPERIMENTAL_NATIVE_STREAMING_V1')) {
    throw 'Streaming build identity mismatch: rebuild before packaging'
}
if ($area -ne $binaryText.Contains('CHLORIUM_EXPERIMENTAL_AREA_DIVISIONS_V1')) { throw 'Area build identity mismatch: rebuild before packaging' }
if ($matrix -ne $binaryText.Contains('CHLORIUM_EXPERIMENTAL_MATRIX_REUSE_V1')) { throw 'Matrix build identity mismatch: rebuild before packaging' }
if ($matrix -and !$binaryText.Contains('CHLORIUM_MATRIX_PERSISTENT_REUSE_V3')) { throw 'Matrix revision mismatch: rebuild matrix.3 before packaging' }
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
