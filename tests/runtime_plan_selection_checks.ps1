#Requires -Version 7.0
$ErrorActionPreference='Stop'
$PSNativeCommandUseErrorActionPreference=$false
$root=Split-Path $PSScriptRoot -Parent
$wrapper=Join-Path $root 'research/R4-04/run-protected-test.ps1'
$shell=(Get-Process -Id $PID).Path
$original=Join-Path $root 'research/R4-04/runtime-plan.local.json'
$before=(Get-FileHash -LiteralPath $original).Hash
function Reject-Plan([string]$path,[string]$message){
    $output=& $shell -NoProfile -File $wrapper -Level 88952972 -Mode candidate -Canonical -PrepareOnly -PlanPath $path 2>&1
    if($LASTEXITCODE -eq 0 -or ($output -join "`n") -notmatch [regex]::Escape($message)){throw "Wrong plan rejection: $message ; $output"}
}
Reject-Plan (Join-Path $env:TEMP 'chlorium-outside.local.json') 'Research plans must be local JSON files'
Reject-Plan (Join-Path $root 'build-research/invalid-plan.txt') 'Research plans must be local JSON files'
$fixture=Join-Path $root ('build-research/plan-selection-'+[Guid]::NewGuid().ToString('N')+'.local.json')
try{
    $plan=Get-Content -LiteralPath $original -Raw | ConvertFrom-Json
    $plan.game_fixture='C:/Program Files (x86)/Steam/steamapps/common/Geometry Dash'
    $plan | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $fixture -Encoding utf8
    Reject-Plan $fixture 'only supports the isolated GD fixture'
    $plan.game_fixture='C:/Users/stoti/Chlorium/test-game';$plan.candidate_sha256='0'*64
    $plan | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $fixture -Encoding utf8
    Reject-Plan $fixture 'Campaign package hash mismatch'
}finally{if(Test-Path -LiteralPath $fixture){Remove-Item -LiteralPath $fixture}}
if((Get-FileHash -LiteralPath $original).Hash -ne $before){throw 'Original campaign plan changed'}
Write-Output 'PASS local plan boundaries, main-game refusal, package hashes and reference plan preservation; no game launched'
