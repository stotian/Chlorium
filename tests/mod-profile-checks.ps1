$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../tools/ModProfile.ps1')
$folder = Join-Path (Split-Path $PSScriptRoot -Parent) ('build-research/profile-check-' + [guid]::NewGuid().ToString('N'))
$mods=Join-Path $folder 'mods'; $profile=Join-Path $folder 'profile'
[IO.Directory]::CreateDirectory($mods) | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
function Add-FixtureMod([string]$name,[string]$id) {
    $zip=[IO.Compression.ZipFile]::Open((Join-Path $mods $name),'Create')
    try {
        $entry=$zip.CreateEntry('mod.json'); $writer=[IO.StreamWriter]::new($entry.Open())
        try { $writer.Write((@{id=$id;version='v1.0.0'} | ConvertTo-Json)) } finally { $writer.Dispose() }
    } finally { $zip.Dispose() }
}
function Set-FixtureFile([string]$relative,[string]$value) {
    $path=Join-Path $profile $relative
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path)) | Out-Null
    [IO.File]::WriteAllText($path,$value)
}
function Read-Profile { Get-ResearchModProfile -ModsDirectory $mods -ProfileRoot $profile }
function Assert([bool]$value,[string]$reason) { if (!$value) { throw $reason } }
Add-FixtureMod 'common.geode' 'example.common'
Add-FixtureMod 'audit.geode' 'local.chlorium.audit'
Set-FixtureFile 'geode/mods/geode.loader/saved.json' '{"should-load-local.chlorium":false}'
$baseline=Read-Profile
Assert ($baseline.mods.Count -eq 2) 'Both common package identities captured'
Add-FixtureMod 'candidate.geode' 'local.chlorium'
Set-FixtureFile 'geode/mods/local.chlorium/settings.json' '{"enabled":true}'
Set-FixtureFile 'geode/mods/geode.loader/saved.json' '{"should-load-local.chlorium":true}'
$candidate=Read-Profile
Assert ($candidate.profile_sha256 -eq $baseline.profile_sha256 -and $candidate.settings_sha256 -eq $baseline.settings_sha256) 'Candidate-only changes must not falsify common conditions'
Set-FixtureFile 'geode/mods/example.common/settings.json' '{"changed":true}'
Assert ((Read-Profile).settings_sha256 -ne $baseline.settings_sha256) 'Common settings change detected'
Set-FixtureFile 'geode/mods/geode.loader/saved.json' '{"should-load-example.common":false}'
Assert ((Read-Profile).profile_sha256 -ne $baseline.profile_sha256) 'Common enable flag change detected'
$beforePackageChange=(Read-Profile).profile_sha256
$zip=[IO.Compression.ZipFile]::Open((Join-Path $mods 'common.geode'),'Update')
try {
    $entry=$zip.CreateEntry('changed-content.txt'); $writer=[IO.StreamWriter]::new($entry.Open())
    try { $writer.Write('Different package bytes with the same mod version') } finally { $writer.Dispose() }
} finally { $zip.Dispose() }
Assert ((Read-Profile).profile_sha256 -ne $beforePackageChange) 'Package content change detected independently of its version'
Add-FixtureMod 'duplicate.geode' 'example.common'
$refused=$false
try { Read-Profile | Out-Null } catch { $refused=$true }
Assert $refused 'Ambiguous duplicate identities rejected'
'PASS: 6 isolated profile invariants; fixture retained only in ignored build-research'
