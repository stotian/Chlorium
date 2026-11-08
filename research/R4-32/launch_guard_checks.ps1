#Requires -Version 7.0
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$shell=(Get-Process -Id $PID).Path
$wrapper=Join-Path $root 'research/R4-04/run-protected-test.ps1'
foreach($extra in @(@('-Performance'),@('-WithUserMods'),@('-ExternalSampling'),@('-OpacityShadow'))) {
    $output=& $shell -NoProfile -File $wrapper -Level 129598490 -Mode native -Canonical -PassCapture -PrepareOnly @extra 2>&1
    if($LASTEXITCODE -eq 0 -or ($output -join "`n") -notmatch 'Native pass capture requires its separate isolated native diagnostic') {throw 'Forbidden capture mode was not rejected before staging'}
}
$receipt=& $shell -NoProfile -File $wrapper -Level 129598490 -Mode native -Canonical -PassCapture -PrepareOnly
if($LASTEXITCODE -ne 0){throw 'Native capture preflight failed'}
$receipt=($receipt -join "`n") | ConvertFrom-Json
if($receipt.game_launch -or $receipt.mod_profile -ne 'isolated' -or $receipt.campaign -notlike '*-pass-capture'){throw 'Wrong capture preflight receipt'}
$plan=Get-Content -LiteralPath (Join-Path $root 'research/R4-04/runtime-plan.local.json') -Raw | ConvertFrom-Json
$unused=Join-Path $root ('build-tests/pass-marker-'+[Guid]::NewGuid().ToString('N'))
$runtime=Join-Path $root 'tools/run-runtime-audit.ps1'
$fake=Join-Path $unused 'fixture-without-executable'
[IO.Directory]::CreateDirectory((Join-Path $fake 'geode/mods')) | Out-Null
$node=@(Get-ChildItem -LiteralPath (Join-Path $plan.game_fixture 'geode/mods') -Filter '*.geode' -File)
if($node.Count -ne 1){throw 'Guard fixture requires the isolated node-ids dependency only'}
[IO.File]::Copy($node[0].FullName,(Join-Path $fake ('geode/mods/'+$node[0].Name)),$false)
$unused=Join-Path $unused 'rejected-report'
foreach($case in @(@{package='build-research/measurement-canonical-v10-r4/local.chlorium.audit.geode';flag=$true},
                    @{package=$plan.measurement_package;flag=$false})) {
    $argsForCase=@{Native=$true;Package=(Join-Path $root $case.package);GameDirectory=$fake;ReportDirectory=$unused}
    if($case.flag){$argsForCase.PassCapture=$true}
    $output=& $shell -NoProfile -File $runtime @argsForCase 2>&1
    if($LASTEXITCODE -eq 0 -or ($output -join "`n") -notmatch 'Native pass capture requires its explicit diagnostic flag and matching auditor'){throw 'Binary/mode mismatch was not rejected'}
    if(Test-Path -LiteralPath $unused){throw 'Rejected mode created a report directory'}
}
foreach($diagnostic in @(@{name='CHLORIUM_QUAD_READ_WATCH';outside='Native quad read observation requires';old='Rebuild the native quad read observer'},
                         @{name='CHLORIUM_STARTUP_DEATH_WATCH';outside='Startup death observation requires';old='Rebuild the startup death observer'},
                         @{name='CHLORIUM_OPACITY_INVOCATIONS';outside='Native opacity invocation census requires';old='Rebuild the native opacity invocation census'},
                         @{name='CHLORIUM_OPAQUE_INPUT_CENSUS';outside='Opaque input census requires';old='Rebuild the opaque input census'},
                         @{name='CHLORIUM_MOVE_COMMAND_CENSUS';outside='Movement command census requires';old='Rebuild the native movement census'},
                         @{name='CHLORIUM_OWNED_REGISTER_ORACLE';outside='Owned register oracle requires';old='Rebuild the owned register oracle'})) {
    $previous=[Environment]::GetEnvironmentVariable($diagnostic.name,'Process')
    try {
        [Environment]::SetEnvironmentVariable($diagnostic.name,'1','Process')
        $output=& $shell -NoProfile -File $runtime -Native -Package (Join-Path $root $plan.measurement_package) -GameDirectory $fake -ReportDirectory $unused 2>&1
        if($LASTEXITCODE -eq 0 -or ($output -join "`n") -notmatch $diagnostic.outside){throw 'Observer was not rejected outside its separate diagnostic'}
        $output=& $shell -NoProfile -File $runtime -Native -PassCapture -Package (Join-Path $root 'build-research/measurement-canonical-v10-r4/local.chlorium.audit.geode') -GameDirectory $fake -ReportDirectory $unused 2>&1
        if($LASTEXITCODE -eq 0 -or ($output -join "`n") -notmatch $diagnostic.old){throw 'Old binary was not rejected for the observer'}
        if(Test-Path -LiteralPath $unused){throw 'Rejected observer mode created a report'}
    } finally {[Environment]::SetEnvironmentVariable($diagnostic.name,$previous,'Process')}
}
$replayNames=@('CHLORIUM_NATIVE_BATCH_REPLAY','CHLORIUM_OPAQUE_INPUT_CENSUS','CHLORIUM_OPAQUE_PIXEL_PAIR')
$replayPrevious=@{};foreach($name in $replayNames){$replayPrevious[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
try {
    foreach($name in $replayNames){[Environment]::SetEnvironmentVariable($name,'1','Process')}
    foreach($case in @(@{capture=$false;message='Native batch replay requires the isolated'},@{capture=$true;message='Rebuild the native batch replay'})){
        foreach($name in @('CHLORIUM_OPAQUE_INPUT_CENSUS','CHLORIUM_OPAQUE_PIXEL_PAIR')){[Environment]::SetEnvironmentVariable($name,$(if($case.capture){'1'}else{$null}),'Process')}
        $parameters=@{Native=$true;Package=(Join-Path $root 'build-research/measurement-canonical-v10-r4/local.chlorium.audit.geode');GameDirectory=$fake;ReportDirectory=$unused};if($case.capture){$parameters.PassCapture=$true}
        $output=& $shell -NoProfile -File $runtime @parameters 2>&1
        if($LASTEXITCODE -eq 0 -or ($output -join "`n") -notmatch $case.message){throw 'Native replay scope/marker guard failed'}
        if(Test-Path -LiteralPath $unused){throw 'Rejected native replay created output'}
    }
    [Environment]::SetEnvironmentVariable('CHLORIUM_OPAQUE_PIXEL_PAIR',$null,'Process')
    $output=& $shell -NoProfile -File $runtime -Native -PassCapture -Package (Join-Path $root $plan.measurement_package) -GameDirectory $fake -ReportDirectory $unused 2>&1
    if($LASTEXITCODE -eq 0 -or ($output -join "`n") -notmatch 'Native batch replay requires its explicit native pixel pair'){throw 'Native replay pixel-pair gate failed'}
} finally {foreach($name in $replayNames){[Environment]::SetEnvironmentVariable($name,$replayPrevious[$name],'Process')}}
$zeroNames=@('CHLORIUM_ZERO_FRAGMENT_DISCARD','CHLORIUM_NATIVE_BATCH_REPLAY','CHLORIUM_OPAQUE_INPUT_CENSUS','CHLORIUM_OPAQUE_PIXEL_PAIR')
$zeroPrevious=@{};foreach($name in $zeroNames){$zeroPrevious[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
try {
    [Environment]::SetEnvironmentVariable('CHLORIUM_ZERO_FRAGMENT_DISCARD','1','Process')
    $output=& $shell -NoProfile -File $runtime -Native -PassCapture -Package (Join-Path $root $plan.measurement_package) -GameDirectory $fake -ReportDirectory $unused 2>&1
    if($LASTEXITCODE -eq 0 -or ($output -join "`n") -notmatch 'Zero fragment discard requires its explicit native batch replay'){throw 'Zero fragment replay dependency guard failed'}
    foreach($name in $zeroNames){[Environment]::SetEnvironmentVariable($name,'1','Process')}
    $output=& $shell -NoProfile -File $runtime -Native -PassCapture -Package (Join-Path $root 'build-research/native-batch-replay/before-zero-discard-r43.geode') -GameDirectory $fake -ReportDirectory $unused 2>&1
    if($LASTEXITCODE -eq 0 -or ($output -join "`n") -notmatch 'Rebuild the zero fragment discard'){throw 'Zero fragment old-marker guard failed'}
    if(Test-Path -LiteralPath $unused){throw 'Rejected zero fragment diagnostic created output'}
} finally {foreach($name in $zeroNames){[Environment]::SetEnvironmentVariable($name,$zeroPrevious[$name],'Process')}}
$handoffNames=@('CHLORIUM_ZERO_NATIVE_HANDOFF','CHLORIUM_ZERO_FRAGMENT_DISCARD','CHLORIUM_NATIVE_BATCH_REPLAY','CHLORIUM_OPAQUE_INPUT_CENSUS','CHLORIUM_OPAQUE_PIXEL_PAIR')
$handoffPrevious=@{};foreach($name in $handoffNames){$handoffPrevious[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
try {
    [Environment]::SetEnvironmentVariable('CHLORIUM_ZERO_NATIVE_HANDOFF','1','Process')
    $output=& $shell -NoProfile -File $runtime -Native -PassCapture -Package (Join-Path $root $plan.measurement_package) -GameDirectory $fake -ReportDirectory $unused 2>&1
    if($LASTEXITCODE -eq 0 -or ($output -join "`n") -notmatch 'Native shader handoff requires its explicit zero fragment diagnostic'){throw 'Native shader handoff dependency guard failed'}
    foreach($name in $handoffNames){[Environment]::SetEnvironmentVariable($name,'1','Process')}
    $output=& $shell -NoProfile -File $runtime -Native -PassCapture -Package (Join-Path $root 'build-research/native-batch-replay/before-zero-discard-r43.geode') -GameDirectory $fake -ReportDirectory $unused 2>&1
    if($LASTEXITCODE -eq 0 -or ($output -join "`n") -notmatch 'Rebuild the native shader handoff'){throw 'Native shader handoff old-marker guard failed'}
    if(Test-Path -LiteralPath $unused){throw 'Rejected native handoff created output'}
} finally {foreach($name in $handoffNames){[Environment]::SetEnvironmentVariable($name,$handoffPrevious[$name],'Process')}}
$residentNames=@('CHLORIUM_RESIDENT_NATIVE_STREAM','CHLORIUM_RESIDENT_NATIVE_REPLAY','CHLORIUM_RESIDENT_VERTEX_FILE','CHLORIUM_NATIVE_BATCH_REPLAY','CHLORIUM_OPAQUE_INPUT_CENSUS','CHLORIUM_OPAQUE_PIXEL_PAIR','CHLORIUM_ZERO_FRAGMENT_DISCARD','CHLORIUM_ZERO_NATIVE_HANDOFF')
$residentPrevious=@{};foreach($name in $residentNames){$residentPrevious[$name]=[Environment]::GetEnvironmentVariable($name,'Process');[Environment]::SetEnvironmentVariable($name,$null,'Process')}
try {
    $env:CHLORIUM_RESIDENT_NATIVE_REPLAY='1'
    $output=& $shell -NoProfile -File $runtime -Native -Package (Join-Path $root $plan.measurement_package) -GameDirectory $fake -ReportDirectory $unused 2>&1
    if($LASTEXITCODE -eq 0 -or ($output -join "`n") -notmatch 'Resident replay requires the isolated native pass-capture'){throw 'Resident replay scope guard failed'}
    $output=& $shell -NoProfile -File $runtime -Native -PassCapture -Package (Join-Path $root $plan.measurement_package) -GameDirectory $fake -ReportDirectory $unused 2>&1
    if($LASTEXITCODE -eq 0 -or ($output -join "`n") -notmatch 'Resident replay requires its explicit native pixel replay chain'){throw 'Resident replay dependency guard failed'}
    foreach($name in @('CHLORIUM_NATIVE_BATCH_REPLAY','CHLORIUM_OPAQUE_INPUT_CENSUS','CHLORIUM_OPAQUE_PIXEL_PAIR')){[Environment]::SetEnvironmentVariable($name,'1','Process')}
    $output=& $shell -NoProfile -File $runtime -Native -PassCapture -Package (Join-Path $root $plan.measurement_package) -GameDirectory $fake -ReportDirectory $unused 2>&1
    if($LASTEXITCODE -eq 0 -or ($output -join "`n") -notmatch 'Resident replay requires the pinned local vertex source'){throw 'Resident replay missing-source guard failed'}
    $env:CHLORIUM_RESIDENT_VERTEX_FILE=Join-Path $root 'research/R4-33/resident.vert'
    $env:CHLORIUM_ZERO_FRAGMENT_DISCARD='1'
    $output=& $shell -NoProfile -File $runtime -Native -PassCapture -Package (Join-Path $root $plan.measurement_package) -GameDirectory $fake -ReportDirectory $unused 2>&1
    if($LASTEXITCODE -eq 0 -or ($output -join "`n") -notmatch 'Resident replay cannot combine'){throw 'Resident replay mixed-renderer guard failed'}
    [Environment]::SetEnvironmentVariable('CHLORIUM_ZERO_FRAGMENT_DISCARD',$null,'Process')
    $output=& $shell -NoProfile -File $runtime -Native -PassCapture -Package (Join-Path $root 'build-research/native-batch-replay/before-zero-discard-r43.geode') -GameDirectory $fake -ReportDirectory $unused 2>&1
    if($LASTEXITCODE -eq 0 -or ($output -join "`n") -notmatch 'Rebuild the resident native replay'){throw 'Resident replay old-marker guard failed'}
    $output=& $shell -NoProfile -File $runtime -Native -PassCapture -Package (Join-Path $root 'build-research/resident-native-replay/r49-qualified.geode') -GameDirectory $fake -ReportDirectory $unused 2>&1
    if($LASTEXITCODE -eq 0 -or ($output -join "`n") -notmatch 'Rebuild the persistent resident cache'){throw 'Persistent cache old-marker guard failed'}
    $env:CHLORIUM_RESIDENT_NATIVE_STREAM='1'
    $output=& $shell -NoProfile -File $runtime -Native -PassCapture -Package (Join-Path $root 'build-research/resident-native-replay/r50-r2-qualified.geode') -GameDirectory $fake -ReportDirectory $unused 2>&1
    if($LASTEXITCODE -eq 0 -or ($output -join "`n") -notmatch 'Rebuild the native vertex stream'){throw 'Native stream old-marker guard failed'}
    $output=& $shell -NoProfile -File $runtime -Native -PassCapture -Package (Join-Path $root 'build-research/resident-native-replay/r51-qualified-batches.geode') -GameDirectory $fake -ReportDirectory $unused 2>&1
    if($LASTEXITCODE -eq 0 -or ($output -join "`n") -notmatch 'Rebuild the native slot cache'){throw 'Native slot cache old-marker guard failed'}
    $output=& $shell -NoProfile -File $runtime -Native -PassCapture -Package (Join-Path $root 'build-research/resident-native-replay/r52-qualified-batches.geode') -GameDirectory $fake -ReportDirectory $unused 2>&1
    if($LASTEXITCODE -eq 0 -or ($output -join "`n") -notmatch 'Rebuild the exact GPU updates'){throw 'Exact GPU updates old-marker guard failed'}
    $output=& $shell -NoProfile -File $runtime -Native -PassCapture -Package (Join-Path $root 'build-research/resident-native-replay/r55-event-qualified-helio.geode') -GameDirectory $fake -ReportDirectory $unused 2>&1
    if($LASTEXITCODE -eq 0 -or ($output -join "`n") -notmatch 'Rebuild the GPU overwrite barrier'){throw 'GPU overwrite old-marker guard failed'}
    [Environment]::SetEnvironmentVariable('CHLORIUM_RESIDENT_NATIVE_REPLAY',$null,'Process')
    $output=& $shell -NoProfile -File $runtime -Native -PassCapture -Package (Join-Path $root $plan.measurement_package) -GameDirectory $fake -ReportDirectory $unused 2>&1
    if($LASTEXITCODE -eq 0 -or ($output -join "`n") -notmatch 'Native vertex stream requires its explicit resident replay'){throw 'Native stream dependency guard failed'}
    if(Test-Path -LiteralPath $unused){throw 'Rejected resident replay created output'}
} finally {foreach($name in $residentNames){[Environment]::SetEnvironmentVariable($name,$residentPrevious[$name],'Process')}}
$producerNames=@('CHLORIUM_NATIVE_PRODUCER_CAPTURE','CHLORIUM_QUAD_READ_WATCH')
$producerPrevious=@{};foreach($name in $producerNames){$producerPrevious[$name]=[Environment]::GetEnvironmentVariable($name,'Process');[Environment]::SetEnvironmentVariable($name,$null,'Process')}
try{
    $env:CHLORIUM_NATIVE_PRODUCER_CAPTURE='1'
    $output=& $shell -NoProfile -File $runtime -Native -Package (Join-Path $root $plan.measurement_package) -GameDirectory $fake -ReportDirectory $unused 2>&1
    if($LASTEXITCODE -eq 0 -or ($output -join "`n") -notmatch 'Producer observer requires the isolated native pass-capture'){throw 'Producer scope guard failed'}
    $env:CHLORIUM_QUAD_READ_WATCH='1'
    $output=& $shell -NoProfile -File $runtime -Native -PassCapture -Package (Join-Path $root $plan.measurement_package) -GameDirectory $fake -ReportDirectory $unused 2>&1
    if($LASTEXITCODE -eq 0 -or ($output -join "`n") -notmatch 'Producer observer cannot combine'){throw 'Producer/hardware conflict guard failed'}
    [Environment]::SetEnvironmentVariable('CHLORIUM_QUAD_READ_WATCH',$null,'Process')
    $output=& $shell -NoProfile -File $runtime -Native -PassCapture -Package (Join-Path $root 'build-research/resident-native-replay/r53-qualified-batches.geode') -GameDirectory $fake -ReportDirectory $unused 2>&1
    if($LASTEXITCODE -eq 0 -or ($output -join "`n") -notmatch 'Rebuild the native producer observer'){throw 'Producer old marker guard failed'}
    $output=& $shell -NoProfile -File $runtime -Native -PassCapture -Package (Join-Path $root 'build-research/resident-native-replay/r54-windowed-qualified-helio.geode') -GameDirectory $fake -ReportDirectory $unused 2>&1
    if($LASTEXITCODE -eq 0 -or ($output -join "`n") -notmatch 'Rebuild the event packet replay'){throw 'Event packet old marker guard failed'}
    if(Test-Path -LiteralPath $unused){throw 'Rejected producer observer created output'}
}finally{foreach($name in $producerNames){[Environment]::SetEnvironmentVariable($name,$producerPrevious[$name],'Process')}}
Write-Output 'PASS capture preflight and forbidden modes; actual package markers reject mismatches and unsafe replay scope before staging or game launch'
