param([string]$Destination = 'C:\Users\stoti\Documents\chlorium')
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root = Split-Path -Parent $PSScriptRoot
$package = Join-Path $root 'dist\local.chlorium.geode'
if (!(Test-Path -LiteralPath $package)) { throw 'Create the base renderer package first' }
$zip = [IO.Compression.ZipFile]::OpenRead($package)
try {
    $reader = [IO.StreamReader]::new($zip.GetEntry('mod.json').Open())
    try { $manifest = $reader.ReadToEnd() | ConvertFrom-Json } finally { $reader.Dispose() }
    if ($manifest.id -ne 'local.chlorium' -or !$zip.GetEntry('local.chlorium.dll')) { throw 'Refusing a test package' }
    $dllReader = [IO.MemoryStream]::new()
    $dllStream = $zip.GetEntry('local.chlorium.dll').Open()
    try { $dllStream.CopyTo($dllReader); $binary = [Text.Encoding]::ASCII.GetString($dllReader.ToArray()) }
    finally { $dllStream.Dispose(); $dllReader.Dispose() }
    if (!$binary.Contains('CHLORIUM_NATIVE_CANONICAL_BACKEND_V10') -or
        !$binary.Contains('CHLORIUM_CANONICAL_AREA_QUOTIENTS_V10') -or
        $binary.Contains('CHLORIUM_NATIVE_VERTEX_BACKEND_V5') -or
        $binary.Contains('CHLORIUM_RUNTIME_AUDIT_BINARY_V1')) { throw 'Only the canonical V10 renderer can replace the current export; legacy packages stay archived' }
    $sourceManifest = Get-Content -LiteralPath (Join-Path $root 'mod.json') -Raw | ConvertFrom-Json
    if ($manifest.version -ne $sourceManifest.version) { throw 'Source and package versions must match before export' }
    if ($binary.Contains('CHLORIUM_NATIVE_ATLAS_CACHE_IMPLEMENTATION_V1')) {
        throw 'The retired atlas cache cannot be exported as the base renderer'
    }
} finally { $zip.Dispose() }
New-Item -ItemType Directory -Force -Path $Destination | Out-Null
$staging = Join-Path $Destination ('.chlorium-export-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $staging | Out-Null
try {
$stagedArchive = Join-Path $staging 'chlorium-sources.zip'
$zip = [IO.Compression.ZipFile]::Open($stagedArchive, [IO.Compression.ZipArchiveMode]::Create)
try {
    # Explicit source-only roots: never walk the game fixture/Resources junction,
    # game data, build trees, personal saves or audit binaries.
    $files = @('.gitignore','.gitattributes','README.md','RESEARCH_BASELINE.md','LICENSE.md','COPYING','NOTICE.md','THIRD_PARTY_NOTICES.md','about.md','changelog.md','support.md','mod.json',
        'logo.png','CMakeLists.txt','build-local.cmd','package-local.ps1',
        'test-system-input-guard.cmd','test-native-submission-policy.cmd','test-adaptive-upload.cmd','test-dirty-state-uploads.cmd','test-local.cmd','test-gpu.cmd','test-native-atlas.cmd','test-native-transform.cmd','test-native-transform-cost.cmd','test-native-heap-release.cmd','test-native-call-thunk.cmd','test-native-matrix.cmd','test-streaming.cmd','test-area-divisions.cmd','test-native-movement.cmd','test-native-affine.cmd','test-sprite-mesh.cmd','test-visual-signature.cmd','test-visual-gpu.cmd') |
        ForEach-Object { Get-Item -LiteralPath (Join-Path $root $_) }
    foreach ($name in @('src','legacy','resources','docs','tests','tools','LICENSES','.github')) {
        $files += Get-ChildItem -LiteralPath (Join-Path $root $name) -File -Recurse |
            Where-Object { $_.Extension -notin @('.pyc','.exe','.obj','.dll','.pdb','.geode') -and $_.Name -notlike '*.local.*' -and $_.FullName -notmatch '[\\/]graphify-out[\\/]' }
    }
    foreach ($name in @('R4-00','R4-01','R4-04','R4-06','R4-07','R4-08','R4-09','R4-10','R4-11','R4-12','R4-13','R4-14','R4-15','R4-16','R4-17','R4-18','R4-19','R4-20','R4-21','R4-22','R4-23','R4-25')) {
        $directory = Join-Path $root "research/$name"
        if (!(Test-Path -LiteralPath $directory)) { continue }
        $files += Get-ChildItem -LiteralPath $directory -File -Recurse |
            Where-Object { $_.Extension -in @('.md','.txt','.json','.csv','.py','.cpp','.hpp','.asm','.cmd','.ps1') -and $_.Name -notlike '*.local.*' }
    }
    foreach ($file in $files) {
        $relative = $file.FullName.Substring($root.Length + 1).Replace('\','/')
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $file.FullName, $relative) | Out-Null
    }
} finally { $zip.Dispose() }
$zip = [IO.Compression.ZipFile]::OpenRead($stagedArchive)
try {
    foreach ($entry in $zip.Entries) {
        $stream = $entry.Open()
        try { $stream.CopyTo([IO.Stream]::Null) } finally { $stream.Dispose() }
    }
} finally { $zip.Dispose() }
Copy-Item -LiteralPath $package -Destination $staging
Copy-Item -LiteralPath (Join-Path $root 'logo.png') -Destination $staging
# Prepare every file before replacing any previous export.
foreach ($name in @('chlorium-sources.zip','local.chlorium.geode','logo.png')) {
    Move-Item -LiteralPath (Join-Path $staging $name) -Destination (Join-Path $Destination $name) -Force
}
} finally {
    # Only these known staging files; never recurse into a computed directory.
    foreach ($name in @('chlorium-sources.zip','local.chlorium.geode','logo.png')) {
        $temporary = Join-Path $staging $name
        if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary }
    }
    Remove-Item -LiteralPath $staging
}
Write-Output "Base renderer package, curated sources and logo exported to $Destination"
