param(
    [Parameter(Mandatory)][string]$InputPackage,
    [Parameter(Mandatory)][string]$OutputPackage
)
# Add distribution notices to a previously measured normal package without
# rebuilding or replacing its DLL. Never use this to relabel an experiment.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Add-Type -AssemblyName System.IO.Compression.FileSystem
if (Test-Path -LiteralPath $OutputPackage) { throw 'Output already exists' }
$inputZip = [IO.Compression.ZipFile]::OpenRead([IO.Path]::GetFullPath($InputPackage))
$outputZip = $null
function Read-EntryBytes($entry) {
    if (!$entry) { throw 'Required ZIP entry is missing' }
    $stream = $entry.Open(); $memory = [IO.MemoryStream]::new()
    try { $stream.CopyTo($memory); return ,$memory.ToArray() }
    finally { $stream.Dispose(); $memory.Dispose() }
}
function Hash-Bytes([byte[]]$bytes) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return [Convert]::ToHexString($sha.ComputeHash($bytes)) }
    finally { $sha.Dispose() }
}
try {
    $manifest = [Text.Encoding]::UTF8.GetString((Read-EntryBytes $inputZip.GetEntry('mod.json'))) | ConvertFrom-Json
    $sourceManifest = Get-Content -LiteralPath (Join-Path $root 'mod.json') -Raw | ConvertFrom-Json
    if ($manifest.id -ne 'local.chlorium' -or $manifest.version -ne $sourceManifest.version -or
        $manifest.gd.win -ne $sourceManifest.gd.win -or $manifest.geode -ne $sourceManifest.geode) {
        throw 'Only the matching normal version may be repackaged'
    }
    $dllBytes = Read-EntryBytes $inputZip.GetEntry('local.chlorium.dll')
    $binary = [Text.Encoding]::ASCII.GetString($dllBytes)
    if (!$binary.Contains('CHLORIUM_NORMAL_BINARY_V1') -or $binary.Contains('CHLORIUM_RUNTIME_AUDIT_BINARY_V1') -or
        $binary.Contains('CHLORIUM_EXPERIMENTAL_')) { throw 'Not a normal production DLL' }
    $dllHash = Hash-Bytes $dllBytes
    $manifest.links.source = $sourceManifest.links.source
    $manifest.developer = $sourceManifest.developer
    $outputPath = [IO.Path]::GetFullPath($OutputPackage)
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $outputPath) | Out-Null
    $outputZip = [IO.Compression.ZipFile]::Open($outputPath, [IO.Compression.ZipArchiveMode]::Create)
    $notices = @('LICENSE.md','COPYING','NOTICE.md','THIRD_PARTY_NOTICES.md')
    foreach ($entry in $inputZip.Entries) {
        if ($entry.FullName -eq 'mod.json' -or $entry.FullName -in $notices -or $entry.FullName.StartsWith('LICENSES/')) { continue }
        $source = $entry.Open(); $destination = $outputZip.CreateEntry($entry.FullName).Open()
        try { $source.CopyTo($destination) } finally { $source.Dispose(); $destination.Dispose() }
    }
    $writer = [IO.StreamWriter]::new($outputZip.CreateEntry('mod.json').Open(), [Text.UTF8Encoding]::new($false))
    try { $writer.Write(($manifest | ConvertTo-Json -Depth 20)) } finally { $writer.Dispose() }
    foreach ($name in $notices) {
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($outputZip, (Join-Path $root $name), $name) | Out-Null
    }
    Get-ChildItem -LiteralPath (Join-Path $root 'LICENSES') -File | ForEach-Object {
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($outputZip, $_.FullName, "LICENSES/$($_.Name)") | Out-Null
    }
    $outputZip.Dispose(); $outputZip = $null
    $check = [IO.Compression.ZipFile]::OpenRead($outputPath)
    try {
        if ((Hash-Bytes (Read-EntryBytes $check.GetEntry('local.chlorium.dll'))) -ne $dllHash) {
            throw 'DLL changed during repackaging'
        }
    } finally { $check.Dispose() }
    [PSCustomObject]@{
        Version = $manifest.version
        PackageSHA256 = (Get-FileHash -LiteralPath $outputPath).Hash
        DllSHA256 = $dllHash
        DllUnchanged = $true
    } | ConvertTo-Json
} finally {
    if ($outputZip) { $outputZip.Dispose() }
    $inputZip.Dispose()
}
