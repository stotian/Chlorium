#Requires -Version 7.0
$ErrorActionPreference='Stop'
$PSNativeCommandUseErrorActionPreference=$false
$root=Split-Path (Split-Path $PSScriptRoot)
$wrapper=Join-Path $root 'research/R4-04/run-protected-test.ps1'
$shell=(Get-Process -Id $PID).Path
foreach($extra in @(@('-Performance'),@('-WithUserMods'),@('-PassCapture'),@('-ExternalSampling'),@('-OpacityShadow'))){
    $result=& $shell -NoProfile -File $wrapper -Level 129598490 -Mode candidate -Canonical -FieldWatch @extra -PrepareOnly 2>&1
    if($LASTEXITCODE -eq 0 -or ($result -join "`n") -notmatch 'R69 field watch requires its isolated candidate diagnostic'){throw "Unqualified R69 run admitted: $extra"}
}
$result=& $shell -NoProfile -File $wrapper -Level 129598490 -Mode native -Canonical -FieldWatch -PrepareOnly 2>&1
if($LASTEXITCODE -eq 0 -or ($result -join "`n") -notmatch 'R69 field watch requires its isolated candidate diagnostic'){throw 'Native-only R69 test admitted'}
Write-Output 'PASS six R69 combination guards reject before campaign reads, staging or launch'
