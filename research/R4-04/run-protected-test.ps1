param(
    [Parameter(Mandatory)][ValidateSet(88952972,129598490,133260800)][int]$Level,
    [Parameter(Mandatory)][ValidateSet('native','candidate')][string]$Mode,
    [switch]$WithUserMods,
    [switch]$PrepareOnly,
    [switch]$AreaCensus,
    [switch]$BlackScreen,
    [ValidateSet('initial','visible','frames')][string]$CensusRevision='initial',
    [switch]$RetryStartup
)
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$taskPlanPath = Join-Path $PSScriptRoot 'runtime-plan.local.json'
$taskPlan = Get-Content -LiteralPath $taskPlanPath -Raw | ConvertFrom-Json
if ($taskPlan.status -ne 'authorized_by_user') { throw 'Runtime campaign lacks explicit user authorization' }
$taskTarget = @($taskPlan.targets | Where-Object level -eq $Level)
if ($taskTarget.Count -ne 1) { throw 'Level is outside the authorized campaign' }
if (Get-Process GeometryDash,cl,link,ninja -ErrorAction SilentlyContinue) { throw 'A game/build process is running; refusing concurrent testing' }
$taskProfileRoot = [IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA 'GeometryDash'))
$taskCampaign = Join-Path $taskRoot $(if ($AreaCensus) {'build-research/runtime-area-census-v5'} else {'build-research/runtime-v4'})
if ($AreaCensus -and $CensusRevision -eq 'visible') { $taskCampaign += '-visible' }
if ($AreaCensus -and $CensusRevision -eq 'frames') { $taskCampaign += '-frames' }
if ($BlackScreen) { $taskCampaign=Join-Path $taskRoot 'build-research/runtime-blackscreen-v5' }
if ($AreaCensus -and ($Level -ne 133260800 -or $Mode -ne 'native')) { throw 'Area census is a native Orbit diagnostic only' }
$taskRun = Join-Path $taskCampaign "$Level-$Mode"
if (Test-Path -LiteralPath $taskRun) {
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
    [ordered]@{profile_files=$taskFiles.Count;backup_bytes=$taskBytes;game_launch=$false;level=$Level;mode=$Mode} | ConvertTo-Json
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
        VisualWatch=$true;BenchmarkSeconds=900
    }
    if ($AreaCensus) {
        $taskParameters.Package = Join-Path $taskRoot 'build-research/measurement-area-census-v5/local.chlorium.audit.geode'
        if ($CensusRevision -eq 'visible') {
            $taskParameters.Package = Join-Path $taskRoot 'build-research/measurement-area-visible-v5/local.chlorium.audit.geode'
        }
        if ($CensusRevision -eq 'frames') {
            $taskParameters.Package = Join-Path $taskRoot 'build-research/measurement-area-frames-v5/local.chlorium.audit.geode'
        }
        $taskParameters.FromPercent = 96
        $taskParameters.ToPercent = 97
        $taskParameters.VisualWatch = $false
    }
    if ($Mode -eq 'native') { $taskParameters.Native=$true }
    if ($WithUserMods) { $taskParameters.WithUserMods=$true }
    if (!$AreaCensus) { $taskParameters.CapturePercent = switch ($Level) { 88952972 { @(24.9,25.2) } 129598490 { @(76.99,77.17) } 133260800 { @(89.5,96.5) } } }
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
