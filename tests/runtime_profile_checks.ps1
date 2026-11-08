#Requires -Version 7.0
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$wrapper=Join-Path $root 'research/R4-04/run-protected-test.ps1'
$plan=Get-Content -LiteralPath (Join-Path (Split-Path $wrapper) 'runtime-plan.local.json') -Raw | ConvertFrom-Json
if($plan.mod_profile -ne 'isolated'){throw 'This local integration check requires the authorized isolated campaign'}
$shell=(Get-Process -Id $PID).Path
$campaign=$null
$profileHash=$null
$settingsHash=$null
$existingCampaigns=@(Get-ChildItem -LiteralPath (Join-Path $root 'build-research') -Directory | ForEach-Object FullName)
$modes=@(if($plan.allowed_modes){$plan.allowed_modes}else{'native';'candidate'})
if(!$modes.Count){throw 'Campaign has no authorized mode'}
foreach($mode in $modes) {
    $result=& $shell -NoProfile -File $wrapper -Level 88952972 -Mode $mode -Canonical -Performance -PrepareOnly
    if($LASTEXITCODE -ne 0){throw "Isolated preflight failed: $mode"}
    $receipt=($result -join "`n") | ConvertFrom-Json
    if($receipt.game_launch -or $receipt.mod_profile -ne 'isolated' -or $receipt.campaign -notlike '*-isolated*-performance'){throw 'Wrong isolated preflight receipt'}
    if($receipt.measurement_sha256 -ne $plan.measurement_sha256 -or $receipt.candidate_sha256 -ne $plan.candidate_sha256){throw 'Wrong campaign package fingerprints'}
    if($campaign -and $campaign -ne $receipt.campaign){throw 'Native/candidate profiles diverged'}
    if($receipt.mod_profile_sha256 -notmatch '^[A-F0-9]{64}$' -or ($profileHash -and $profileHash -ne $receipt.mod_profile_sha256)){throw 'Isolated dependency hashes diverged'}
    if($receipt.profile_settings_sha256 -notmatch '^[A-F0-9]{64}$' -or ($settingsHash -and $settingsHash -ne $receipt.profile_settings_sha256)){throw 'Isolated settings hashes diverged'}
    $campaign=$receipt.campaign
    $profileHash=$receipt.mod_profile_sha256
    $settingsHash=$receipt.profile_settings_sha256
    if($campaign -notin $existingCampaigns -and (Test-Path -LiteralPath $campaign)){throw 'Read-only preflight created a campaign directory'}
}
$result=& $shell -NoProfile -File $wrapper -Level 88952972 -Mode $modes[0] -Canonical -Performance -PrepareOnly -WithUserMods 2>&1
if($LASTEXITCODE -eq 0 -or ($result -join "`n") -notmatch 'Current campaign requires isolated mods'){throw 'Personal mod profile was not rejected early'}
Write-Output ('PASS authorized preflights use the isolated campaign: '+($modes -join ',')+'; no game/directory created; personal mods rejected')
$fixture=Join-Path $root ('build-tests/campaign-hash-'+[Guid]::NewGuid().ToString('N'))
$copy=Join-Path $fixture 'research/R4-04'
New-Item -ItemType Directory -Path $copy -Force | Out-Null
Copy-Item -LiteralPath $wrapper -Destination $copy
[IO.File]::WriteAllText((Join-Path $fixture 'changed.geode'),'changed bytes, never executed')
$invalid=@{status='authorized_by_user';targets=@(@{level=88952972});measurement_package='changed.geode';measurement_sha256=('0'*64);candidate_package='changed.geode';candidate_sha256=('0'*64)}
$invalid | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $copy 'runtime-plan.local.json') -Encoding UTF8
$result=& $shell -NoProfile -File (Join-Path $copy 'run-protected-test.ps1') -Level 88952972 -Mode native -Canonical -PrepareOnly 2>&1
if($LASTEXITCODE -eq 0 -or ($result -join "`n") -notmatch 'Campaign package hash mismatch'){throw 'Changed campaign binary was not rejected before staging'}
if(Test-Path -LiteralPath (Join-Path $fixture 'build-research')){throw 'Bad binary created a run directory'}
Write-Output 'PASS real wrapper rejects changed package bytes before profile reads, backup or launch'
