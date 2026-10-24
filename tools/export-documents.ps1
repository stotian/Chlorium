param([string]$Destination = 'C:\Users\stoti\Documents\chlorium')
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root = Split-Path -Parent $PSScriptRoot
$package = Join-Path $root 'dist\local.chlorium.geode'
if (!(Test-Path -LiteralPath $package)) { throw 'Create a normal package first' }
$zip = [IO.Compression.ZipFile]::OpenRead($package)
try {
    $reader = [IO.StreamReader]::new($zip.GetEntry('mod.json').Open())
    try { $manifest = $reader.ReadToEnd() | ConvertFrom-Json } finally { $reader.Dispose() }
    if ($manifest.id -ne 'local.chlorium' -or !$zip.GetEntry('local.chlorium.dll')) { throw 'Refusing a test package' }
    $dllReader = [IO.MemoryStream]::new()
    $dllStream = $zip.GetEntry('local.chlorium.dll').Open()
    try { $dllStream.CopyTo($dllReader); $binary = [Text.Encoding]::ASCII.GetString($dllReader.ToArray()) }
    finally { $dllStream.Dispose(); $dllReader.Dispose() }
    if ($binary.Contains('CHLORIUM_EXPERIMENTAL_AREA_MATH_V1') -or $binary.Contains('CHLORIUM_EXPERIMENTAL_AFFINE_POINT_V1') -or $binary.Contains('CHLORIUM_EXPERIMENTAL_NATIVE_STREAMING_V1') -or $binary.Contains('CHLORIUM_EXPERIMENTAL_AREA_DIVISIONS_V1') -or $binary.Contains('CHLORIUM_EXPERIMENTAL_MATRIX_REUSE_V1')) { throw 'Experiments must remain separate from the installed normal package' }
    $sourceManifest = Get-Content -LiteralPath (Join-Path $root 'mod.json') -Raw | ConvertFrom-Json
    if ($manifest.version -ne $sourceManifest.version) { throw 'Source and package versions must match before export' }
    if (!$binary.Contains('CHLORIUM_NATIVE_GPU_TRANSFERS_ONLY_V1') -or $binary.Contains('CHLORIUM_NATIVE_ATLAS_CACHE_IMPLEMENTATION_V1')) {
        throw 'The normal candidate must use native GPU transfers without the retired atlas cache'
    }
} finally { $zip.Dispose() }
New-Item -ItemType Directory -Force -Path $Destination | Out-Null
Copy-Item -LiteralPath $package -Destination $Destination -Force
Copy-Item -LiteralPath (Join-Path $root 'logo.png') -Destination $Destination -Force
$archive = Join-Path $Destination 'chlorium-sources.zip'
if (Test-Path -LiteralPath $archive) { Remove-Item -LiteralPath $archive }
$zip = [IO.Compression.ZipFile]::Open($archive, [IO.Compression.ZipArchiveMode]::Create)
try {
    # Explicit source-only roots: never walk the game fixture/Resources junction,
    # game data, build trees, personal saves or audit binaries.
    $files = @('.gitignore','.gitattributes','README.md','LICENSE.md','COPYING','NOTICE.md','THIRD_PARTY_NOTICES.md','about.md','changelog.md','support.md','mod.json',
        'logo.png','CMakeLists.txt','build-local.cmd','package-local.ps1',
        'test-local.cmd','test-gpu.cmd','test-native-atlas.cmd','test-native-transform.cmd','test-native-matrix.cmd','test-streaming.cmd','test-area-divisions.cmd','test-native-movement.cmd','test-native-affine.cmd','test-sprite-mesh.cmd','test-visual-signature.cmd','test-visual-gpu.cmd') |
        ForEach-Object { Get-Item -LiteralPath (Join-Path $root $_) }
    foreach ($name in @('src','resources','docs','tests','tools','LICENSES','.github')) {
        $files += Get-ChildItem -LiteralPath (Join-Path $root $name) -File -Recurse |
            Where-Object { $_.Extension -notin @('.pyc','.exe','.obj','.dll','.pdb','.geode') }
    }
    foreach ($file in $files) {
        $relative = $file.FullName.Substring($root.Length + 1).Replace('\','/')
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $file.FullName, $relative) | Out-Null
    }
} finally { $zip.Dispose() }
Write-Output "Normal package, source archive and logo exported to $Destination"
