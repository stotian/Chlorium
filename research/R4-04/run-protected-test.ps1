#Requires -Version 7.0
param(
    [Parameter(Mandatory)][ValidateSet(88952972,129598490,133260800,139648964)][int]$Level,
    [Parameter(Mandatory)][ValidateSet('native','candidate')][string]$Mode,
    [switch]$WithUserMods,
    [switch]$PrepareOnly,
    [switch]$AreaCensus,
    [switch]$PoseCensus,
    [switch]$AdaptiveUpload,
    [switch]$NativeBridge,
    [switch]$GpuExchange,
    [switch]$PersistentTransfer,
    [switch]$Canonical,
    [switch]$ExternalFocus,
    [switch]$ExternalSampling,
    [switch]$OpacityShadow,
    [switch]$PassCapture,
    [switch]$FieldWatch,
    [switch]$RotationTrace,
    [switch]$Performance,
    [switch]$BlackScreen,
    [ValidateSet('initial','visible','frames')][string]$CensusRevision='initial',
    [ValidateSet('initial','r2')][string]$TransferRevision='initial',
    [switch]$RetryStartup,
    [ValidatePattern('^[a-z0-9][a-z0-9-]{0,31}$')][string]$RunRevision,
    [switch]$FullLevel,
    [string]$PlanPath
)
$ErrorActionPreference = 'Stop'
if($RotationTrace -and (!$Canonical -or $Mode -ne 'candidate' -or $Performance -or $WithUserMods -or $ExternalSampling -or $OpacityShadow -or $PassCapture -or $FieldWatch)){throw 'R72 rotation trace requires its isolated candidate diagnostic; never a performance test'}
if($FieldWatch -and (!$Canonical -or $Mode -ne 'candidate' -or $Performance -or $WithUserMods -or $ExternalSampling -or $OpacityShadow -or $PassCapture)){throw 'R69 field watch requires its isolated candidate diagnostic; never a performance test'}
if($FullLevel -and ($Level -ne 88952972 -or $Mode -ne 'candidate' -or !$Canonical -or !$Performance)){throw 'Full-level override is restricted to the explicitly requested Grief continuous-renderer test'}
if($PassCapture -and (!$Canonical -or $Mode -ne 'native' -or $Performance -or $WithUserMods -or $ExternalSampling -or $OpacityShadow)){throw 'Native pass capture requires its separate isolated native diagnostic'}
if($ExternalSampling -and (!$Canonical -or $Performance -or $WithUserMods)){throw 'External sampling requires a separate canonical diagnostic with isolated mods'}
if($OpacityShadow -and (!$Canonical -or $Mode -ne 'native' -or $Performance -or $WithUserMods -or $ExternalSampling)){throw 'Opacity shadow requires its isolated native canonical diagnostic, never a performance comparison'}
if ($TransferRevision -ne 'initial' -and !$PersistentTransfer) { throw 'Transfer revision requires the explicit persistent profile' }
if ($Canonical -and ($GpuExchange -or $PersistentTransfer -or $NativeBridge -or $AdaptiveUpload -or $AreaCensus -or $PoseCensus -or $BlackScreen)) {throw 'Canonical core requires its separate comparison profile'}
if ($Performance -and (!($GpuExchange -or $PersistentTransfer -or $Canonical) -or $PoseCensus -or $AreaCensus -or $BlackScreen)) { throw 'Performance comparison requires an explicit production profile' }
if ($AdaptiveUpload -and ($PoseCensus -or $AreaCensus -or $BlackScreen)) { throw 'Choose one isolated experiment' }
if ($PersistentTransfer -and ($PoseCensus -or $AreaCensus -or $BlackScreen -or $AdaptiveUpload -or $NativeBridge -or $GpuExchange)) { throw 'Choose one isolated experiment' }
if ($GpuExchange -and ($PoseCensus -or $AreaCensus -or $BlackScreen -or $AdaptiveUpload -or $NativeBridge)) { throw 'Choose one isolated experiment' }
if ($NativeBridge -and ($PoseCensus -or $AreaCensus -or $BlackScreen -or $AdaptiveUpload)) { throw 'Choose one isolated experiment' }
if ($PoseCensus -and $AreaCensus) { throw 'Choose one census' }
if ($PoseCensus) { $AreaCensus=$true }
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$taskPlanPath = Join-Path $PSScriptRoot 'runtime-plan.local.json'
if($PlanPath){
    $taskPlanPath=[IO.Path]::GetFullPath($PlanPath)
    $taskPlansRoot=[IO.Path]::GetFullPath((Join-Path $taskRoot 'build-research'))
    if(!$taskPlanPath.StartsWith($taskPlansRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or !$taskPlanPath.EndsWith('.local.json',[StringComparison]::OrdinalIgnoreCase)){
        throw 'Research plans must be local JSON files inside build-research'
    }
    $taskProbe=$taskPlanPath
    while($taskProbe -and $taskProbe -ne $taskPlansRoot){
        if((Test-Path -LiteralPath $taskProbe) -and ((Get-Item -LiteralPath $taskProbe -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Research plan path is redirected'}
        $taskProbe=[IO.Path]::GetDirectoryName($taskProbe)
    }
}
$taskPlan = Get-Content -LiteralPath $taskPlanPath -Raw | ConvertFrom-Json
if($RunRevision){$taskPlan.run_revision=$RunRevision}
if ($taskPlan.status -ne 'authorized_by_user') { throw 'Runtime campaign lacks explicit user authorization' }
if($taskPlan.allowed_modes -and $Mode -notin $taskPlan.allowed_modes){throw 'This campaign only authorizes the selected Chlorium renderer; native baseline tests are stopped'}
if($ExternalSampling -and $Mode -eq 'candidate' -and !$taskPlan.adaptive_sampling_r66){throw 'Candidate sampling requires its distinct R66 diagnostic plan'}
if($taskPlan.adaptive_sampling_r66 -and (!$ExternalSampling -or $Mode -ne 'candidate')){throw 'R66 sampling plan requires its explicit candidate diagnostic flag'}
if([bool]$taskPlan.group_trace_r63 -ne ($env:CHLORIUM_GROUP_TRACE_R63 -eq '1')){throw 'R63 trace requires its explicit campaign and diagnostic selection'}
if([bool]$taskPlan.field_watch_r69 -ne [bool]$FieldWatch){throw 'R69 field watch requires its distinct campaign and explicit diagnostic flag'}
if([bool]$taskPlan.rotation_trace_r72 -ne [bool]$RotationTrace){throw 'R72 rotation trace requires its distinct campaign and explicit diagnostic flag'}
if($RotationTrace -and $taskPlan.group_trace_r63){throw 'R72 rotation trace cannot combine movement capture'}
if($FieldWatch -and $taskPlan.group_trace_r63){throw 'R69 field watch cannot combine group capture'}
if($taskPlan.group_trace_r63 -and ($Mode -ne 'candidate' -or $WithUserMods -or $PassCapture)){throw 'R63 trace only permits Chlorium active with isolated mods'}
if ($taskPlan.run_revision -and $taskPlan.run_revision -notmatch '^[a-z0-9][a-z0-9-]{0,31}$') {throw 'Invalid campaign revision'}
if([bool]$taskPlan.opacity_shadow -ne [bool]$OpacityShadow){throw 'Opacity shadow requires its distinct, explicitly selected campaign plan'}
if([bool]$taskPlan.native_pass_capture -ne [bool]$PassCapture){throw 'Native pass capture requires its distinct campaign plan'}
if ($WithUserMods -and $taskPlan.mod_profile -eq 'isolated') { throw 'Current campaign requires isolated mods; personal mod profiles are not authorized' }
$taskTarget = @($taskPlan.targets | Where-Object level -eq $Level)
if ($taskTarget.Count -ne 1) { throw 'Level is outside the authorized campaign' }
if($FullLevel){$taskTarget[0].from_percent=0.0;$taskTarget[0].to_percent=100.0}
if($Canonical) {
    foreach($planned in @(@{path=$taskPlan.measurement_package;sha256=$taskPlan.measurement_sha256},@{path=$taskPlan.candidate_package;sha256=$taskPlan.candidate_sha256})) {
        if(!$planned.path -or !$planned.sha256 -or (Get-FileHash -LiteralPath (Join-Path $taskRoot $planned.path) -Algorithm SHA256).Hash -ne $planned.sha256) {
            throw 'Campaign package hash mismatch; no backup, staging or game launch permitted'
        }
    }
}
if([IO.Path]::GetFullPath($taskPlan.game_fixture) -ne [IO.Path]::GetFullPath('C:/Users/stoti/Chlorium/test-game') -or $taskPlan.main_installation_changes){
    throw 'This wrapper only supports the isolated GD fixture; main installation changes are forbidden'
}
. (Join-Path $taskRoot 'tools/ModProfile.ps1')
$taskIsolatedProfile=if(!$WithUserMods){Get-ResearchModProfile -ModsDirectory (Join-Path $taskPlan.game_fixture 'geode/mods') -ProfileRoot (Join-Path $env:LOCALAPPDATA 'GeometryDash') -AllowedIds @('geode.node-ids')}else{$null}
if(!$WithUserMods -and @($taskIsolatedProfile.mods).Count -ne 1){throw 'Isolated profile requires the node-ids dependency'}
if (Get-Process GeometryDash,cl,link,ninja -ErrorAction SilentlyContinue) { throw 'A game/build process is running; refusing concurrent testing' }
$taskProfileRoot = [IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA 'GeometryDash'))
$taskCampaign = Join-Path $taskRoot $(if ($AreaCensus) {'build-research/runtime-area-census-v5'} else {'build-research/runtime-v4'})
if ($AreaCensus -and $CensusRevision -eq 'visible') { $taskCampaign += '-visible' }
if ($AreaCensus -and $CensusRevision -eq 'frames') { $taskCampaign += '-frames' }
if ($BlackScreen) { $taskCampaign=Join-Path $taskRoot 'build-research/runtime-blackscreen-v5' }
if ($PoseCensus) { $taskCampaign=Join-Path $taskRoot 'build-research/runtime-pose-census-v6' }
if ($PoseCensus -and !$WithUserMods) { $taskCampaign += '-isolated' }
if ($AdaptiveUpload) { $taskCampaign=Join-Path $taskRoot 'build-research/runtime-adaptive-upload-v6'; if(!$WithUserMods){$taskCampaign+='-isolated'} }
if ($NativeBridge) { $taskCampaign=Join-Path $taskRoot 'build-research/runtime-native-bridge-v7'; if(!$WithUserMods){$taskCampaign+='-isolated'} }
if ($GpuExchange) { $taskCampaign=Join-Path $taskRoot 'build-research/runtime-gpu-exchange-v8'; if(!$WithUserMods){$taskCampaign+='-isolated'} }
if ($PersistentTransfer) { $taskCampaign=Join-Path $taskRoot 'build-research/runtime-persistent-transfer-v9'; if(!$WithUserMods){$taskCampaign+='-isolated'} }
if ($PersistentTransfer -and $TransferRevision -eq 'r2') { $taskCampaign+='-r2' }
if ($Canonical) { $taskCampaign=Join-Path $taskRoot 'build-research/runtime-canonical-v10'; if(!$WithUserMods){$taskCampaign+='-isolated'} }
if ($taskPlan.run_revision) { $taskCampaign+='-'+$taskPlan.run_revision }
if ($ExternalSampling) { $taskCampaign+='-sampling' }
if ($OpacityShadow) { $taskCampaign+='-opacity-shadow' }
if ($PassCapture) { $taskCampaign+='-pass-capture' }
if ($FieldWatch) { $taskCampaign+='-field-watch' }
if ($RotationTrace) { $taskCampaign+='-rotation-trace' }
if ($Performance) { $taskCampaign+='-performance' }
if ($AreaCensus -and ($Level -ne 133260800 -or $Mode -ne 'native')) { throw 'Area census is a native Orbit diagnostic only' }
$taskRun = Join-Path $taskCampaign "$Level-$Mode"
if (!$PrepareOnly -and (Test-Path -LiteralPath $taskRun)) {
    if (!$RetryStartup) { throw 'This authorized pass already has a run directory; refusing an implicit repeat' }
    $previousBackup=Get-Content -LiteralPath (Join-Path $taskRun 'profile-backup.local.json') -Raw | ConvertFrom-Json
    $previousMode=if ($Mode -eq 'native') {'native'} else {'chlorium'}
    $previousReport=Join-Path $taskCampaign "reports-$Level/auto-$Level-$previousMode.jsonl"
    if ($previousBackup.status -ne 'restored_and_sha256_verified' -or (Test-Path -LiteralPath $previousReport)) {
        throw 'Startup retry allowed only after restored data and no harness/gameplay report'
    }
    $taskRun += '-startup-retry'
    if (Test-Path -LiteralPath $taskRun) { throw 'The single startup retry was already used' }
}

function Assert-ProfilePath([string]$path) {
    $absolute = [IO.Path]::GetFullPath($path)
    if (!$absolute.StartsWith($taskProfileRoot + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
        throw 'Refusing an operation outside the shared GD profile'
    }
    $probe = $absolute
    while ($probe -and $probe -ne $taskProfileRoot) {
        if (Test-Path -LiteralPath $probe) {
            if ((Get-Item -LiteralPath $probe -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw 'Refusing to follow a profile reparse point'
            }
        }
        $probe = [IO.Path]::GetDirectoryName($probe)
    }
    return $absolute
}
function Get-ProtectedFiles {
    # Protect progression/levels and configuration. Never traverse music,
    # thumbnails, downloaded assets, recordings or backup/cache trees.
    $found = [Collections.Generic.List[IO.FileInfo]]::new()
    foreach ($file in Get-ChildItem -LiteralPath $taskProfileRoot -File -Force) {
        if ($file.Name -match '^CC.*\.dat' -or $file.Extension -in @('.dat','.json','.plist','.bak','.vdf')) {
            Assert-ProfilePath $file.FullName | Out-Null
            $found.Add($file)
        }
    }
    $pending = [Collections.Generic.Stack[string]]::new()
    $geode = Join-Path $taskProfileRoot 'geode'
    if (Test-Path -LiteralPath $geode) {
        foreach ($file in Get-ChildItem -LiteralPath $geode -File -Force) {
            if (!($file.Attributes -band [IO.FileAttributes]::ReparsePoint)) { $found.Add($file) }
        }
    }
    $mods = Join-Path $geode 'mods'
    if (Test-Path -LiteralPath $mods) {
        foreach ($mod in Get-ChildItem -LiteralPath $mods -Directory -Force) {
            if ($mod.Attributes -band [IO.FileAttributes]::ReparsePoint) { continue }
            Assert-ProfilePath $mod.FullName | Out-Null
            foreach ($item in Get-ChildItem -LiteralPath $mod.FullName -Force) {
                if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { continue }
                if (!$item.PSIsContainer) { $found.Add($item) }
                elseif ($item.Name -in @('config','configs','settings','profiles')) { $pending.Push($item.FullName) }
            }
        }
    }
    $visited = 0
    while ($pending.Count) {
        if (++$visited -gt 256) { throw 'Configuration traversal budget exceeded; no game launched' }
        $folder = $pending.Pop()
        Assert-ProfilePath $folder | Out-Null
        foreach ($item in Get-ChildItem -LiteralPath $folder -Force) {
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { continue }
            if ($item.PSIsContainer) { $pending.Push($item.FullName) }
            else { $found.Add($item) }
        }
    }
    return $found.ToArray()
}
$taskFiles = @(Get-ProtectedFiles)
$taskBytes = ($taskFiles | Measure-Object Length -Sum).Sum
if ($taskBytes -gt 512MB) { throw 'Profile backup exceeds the 512 MiB preflight budget; no game launched' }
if ($PrepareOnly) {
    [ordered]@{target_from_percent=$taskTarget[0].from_percent;target_to_percent=$taskTarget[0].to_percent;profile_files=$taskFiles.Count;backup_bytes=$taskBytes;game_launch=$false;level=$Level;mode=$Mode;mod_profile=$(if($WithUserMods){'user-mods'}else{'isolated'});mod_profile_sha256=$taskIsolatedProfile.profile_sha256;profile_settings_sha256=$taskIsolatedProfile.settings_sha256;measurement_sha256=$taskPlan.measurement_sha256;candidate_sha256=$taskPlan.candidate_sha256;campaign=$taskCampaign} | ConvertTo-Json
    exit 0
}
[IO.Directory]::CreateDirectory($taskRun) | Out-Null
$taskBackup = Join-Path $taskRun 'profile-before'
[IO.Directory]::CreateDirectory($taskBackup) | Out-Null
$taskRecords = @()
foreach ($file in $taskFiles) {
    $relative = $file.FullName.Substring($taskProfileRoot.Length + 1)
    $copy = Join-Path $taskBackup $relative
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($copy)) | Out-Null
    [IO.File]::Copy($file.FullName,$copy,$false)
    $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    if ($hash -ne (Get-FileHash -LiteralPath $copy -Algorithm SHA256).Hash) { throw 'Backup hash mismatch; no game launched' }
    $taskRecords += [ordered]@{relative=$relative;sha256=$hash;last_write_utc=$file.LastWriteTimeUtc.ToString('o')}
}
$taskManifestPath = Join-Path $taskRun 'profile-backup.local.json'
$taskManifest = [ordered]@{status='backed_up';scope='progression_levels_and_configuration';assets_caches_recordings_excluded=$true;files=$taskRecords;count=$taskRecords.Count;bytes=$taskBytes}
[IO.File]::WriteAllText($taskManifestPath,($taskManifest | ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))
$taskMainPackage = 'C:/Program Files (x86)/Steam/steamapps/common/Geometry Dash/geode/mods/local.chlorium.geode'
$taskMainExisted = Test-Path -LiteralPath $taskMainPackage
$taskMainHash = if ($taskMainExisted) { (Get-FileHash -LiteralPath $taskMainPackage -Algorithm SHA256).Hash } else { $null }
$taskFailure = $null
try {
    if($taskPlan.startup_diagnostic){
        $taskLoaderSettingsPath=Assert-ProfilePath (Join-Path $taskProfileRoot 'geode/mods/geode.loader/settings.json')
        $taskLoaderSettings=Get-Content -LiteralPath $taskLoaderSettingsPath -Raw | ConvertFrom-Json -AsHashtable
        $taskLoaderSettings['file-log-level']='debug'
        [IO.File]::WriteAllText($taskLoaderSettingsPath,($taskLoaderSettings | ConvertTo-Json -Depth 30),[Text.UTF8Encoding]::new($false))
    }
    if (!(Get-Process steam -ErrorAction SilentlyContinue)) {
        Start-Process -FilePath 'C:/Program Files (x86)/Steam/steam.exe' -ArgumentList '-silent' -WindowStyle Hidden
        Start-Sleep -Seconds 8
    }
    if ($Mode -eq 'candidate') {
        $settingsPath = Join-Path $taskProfileRoot 'geode/mods/local.chlorium/settings.json'
        Assert-ProfilePath $settingsPath | Out-Null
        $settings = if (Test-Path -LiteralPath $settingsPath) {
            Get-Content -LiteralPath $settingsPath -Raw | ConvertFrom-Json -AsHashtable
        } else { @{} }
        $settings['enabled'] = $true
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($settingsPath)) | Out-Null
        [IO.File]::WriteAllText($settingsPath,($settings | ConvertTo-Json -Depth 100),[Text.UTF8Encoding]::new($false))
    }
    $taskParameters = @{
        Level=$Level;GameDirectory=$taskPlan.game_fixture
        Package=(Join-Path $taskRoot $taskPlan.measurement_package)
        ProductionPackage=(Join-Path $taskRoot $taskPlan.candidate_package)
        ReportDirectory=(Join-Path $taskCampaign "reports-$Level")
        FromPercent=[double]$taskTarget[0].from_percent;ToPercent=[double]$taskTarget[0].to_percent
        VisualWatch=(!$Performance);BenchmarkSeconds=900
    }
    if($taskPlan.startup_diagnostic){$taskParameters.StartupDiagnostic=$true}
    if ($AdaptiveUpload) {
        $taskParameters.Package=Join-Path $taskRoot 'build-research/measurement-upload-v6/local.chlorium.audit.geode'
        $taskParameters.ProductionPackage=Join-Path $taskRoot 'build-research/native-adaptive-upload-v6/local.chlorium.geode'
    }
    if ($NativeBridge) {
        $taskParameters.Package=Join-Path $taskRoot 'build-research/measurement-upload-v6/local.chlorium.audit.geode'
        $taskParameters.ProductionPackage=Join-Path $taskRoot 'build-research/native-draw-bridge-v7/local.chlorium.geode'
    }
    if ($GpuExchange) {
        $taskParameters.Package=Join-Path $taskRoot 'build-research/measurement-upload-v6/local.chlorium.audit.geode'
        $taskParameters.ProductionPackage=Join-Path $taskRoot 'build-research/native-gpu-exchange-v8/local.chlorium.geode'
    }
    if ($PersistentTransfer) {
        $taskParameters.Package=Join-Path $taskRoot 'build-research/measurement-transfer-v9/local.chlorium.audit.geode'
        $taskParameters.ProductionPackage=Join-Path $taskRoot 'build-research/native-persistent-transfer-v9/local.chlorium.geode'
        if ($TransferRevision -eq 'r2') { $taskParameters.Package=Join-Path $taskRoot 'build-research/measurement-transfer-v9-r2/local.chlorium.audit.geode' }
    }
    if ($AreaCensus) {
        $taskParameters.Package = Join-Path $taskRoot 'build-research/measurement-area-census-v5/local.chlorium.audit.geode'
        if ($CensusRevision -eq 'visible') {
            $taskParameters.Package = Join-Path $taskRoot 'build-research/measurement-area-visible-v5/local.chlorium.audit.geode'
        }
        if ($CensusRevision -eq 'frames') {
            $taskParameters.Package = Join-Path $taskRoot 'build-research/measurement-area-frames-v5/local.chlorium.audit.geode'
        }
        if ($PoseCensus) { $taskParameters.Package = Join-Path $taskRoot 'build-research/measurement-pose-census-v6/local.chlorium.audit.geode' }
        $taskParameters.FromPercent = 96
        $taskParameters.ToPercent = 97
        $taskParameters.VisualWatch = $false
    }
    if ($Mode -eq 'native') { $taskParameters.Native=$true }
    if ($ExternalFocus) {$taskParameters.ExternalFocus=$true}
    if ($ExternalSampling) {$taskParameters.ExternalSampling=$true;$taskParameters.VisualWatch=$false}
    if ($OpacityShadow) {$taskParameters.OpacityShadow=$true;$taskParameters.VisualWatch=$false}
    if ($PassCapture) {$taskParameters.PassCapture=$true;$taskParameters.VisualWatch=$false}
    if ($FieldWatch) {$taskParameters.FieldWatch=$true;$taskParameters.VisualWatch=$false}
    if ($RotationTrace) {$taskParameters.RotationTrace=$true;$taskParameters.VisualWatch=$false}
    if ($WithUserMods) { $taskParameters.WithUserMods=$true }
    if ($taskParameters.VisualWatch) { $taskParameters.CapturePercent = switch ($Level) { 88952972 { @(45,55,64.5) } 129598490 { @(76.99,77.17) } 133260800 { @(89.5,96.5) } } }
    & (Join-Path $taskRoot 'tools/run-runtime-audit.ps1') @taskParameters
} catch {
    $taskFailure = $_
} finally {
    if (Get-Process GeometryDash -ErrorAction SilentlyContinue) {
        $taskManifest.status='restoration_pending_game_process'
        [IO.File]::WriteAllText($taskManifestPath,($taskManifest | ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))
        throw 'A game process remains: profile backup retained, restoration deferred to protect the session'
    }
    foreach ($record in $taskRecords) {
        $destination = Assert-ProfilePath (Join-Path $taskProfileRoot $record.relative)
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination)) | Out-Null
        [IO.File]::Copy((Join-Path $taskBackup $record.relative),$destination,$true)
        [IO.File]::SetLastWriteTimeUtc($destination,[DateTime]::Parse($record.last_write_utc))
        if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $record.sha256) { throw 'Profile restoration hash mismatch' }
    }
    $original = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($record in $taskRecords) { $original.Add($record.relative) | Out-Null }
    foreach ($file in @(Get-ProtectedFiles)) {
        $relative = $file.FullName.Substring($taskProfileRoot.Length + 1)
        if (!$original.Contains($relative)) {
            $safe = Assert-ProfilePath $file.FullName
            Remove-Item -LiteralPath $safe -Force # only newly created profile files, never recursive
        }
    }
    if ((Test-Path -LiteralPath $taskMainPackage) -ne $taskMainExisted -or
        ($taskMainExisted -and (Get-FileHash -LiteralPath $taskMainPackage -Algorithm SHA256).Hash -ne $taskMainHash)) { throw 'Main installed package changed unexpectedly' }
    $taskManifest.status='restored_and_sha256_verified'
    [IO.File]::WriteAllText($taskManifestPath,($taskManifest | ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))
    Write-Output "Profile restored and verified: $($taskRecords.Count) files; installed package unchanged"
}
if ($taskFailure) { throw $taskFailure }
