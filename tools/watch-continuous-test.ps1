#Requires -Version 7.0
param([ValidateSet(0,129598490,88952972,133260800,139648964)][int]$Level=0,[switch]$Stop,[switch]$PrepareOnly,[switch]$Controller,[switch]$FullLevel)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$marker=Join-Path $root 'build-research/visible-continuous-session.local.json'
if($Stop){
    if(!(Test-Path -LiteralPath $marker)){Write-Output 'Aucun test visible en cours.';exit 0}
    $session=Get-Content -LiteralPath $marker -Raw | ConvertFrom-Json
    if($session.status -ne 'running'){Write-Output 'Le dernier test est deja termine.';exit 0}
    $request=[IO.Path]::GetFullPath([string]$session.stop_request)
    if(!$request.StartsWith((Join-Path $root 'build-research')+'\',[StringComparison]::OrdinalIgnoreCase) -or
       !$request.EndsWith('.jsonl.stop-request',[StringComparison]::Ordinal)){throw 'Chemin de demande d arret invalide'}
    $parent=[IO.Path]::GetDirectoryName($request)
    while($parent -and $parent -ne $root){
        if((Test-Path -LiteralPath $parent) -and ((Get-Item -LiteralPath $parent -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Dossier d arret redirige'}
        $parent=[IO.Path]::GetDirectoryName($parent)
    }
    $game=@(Get-Process GeometryDash -ErrorAction SilentlyContinue | Where-Object {$_.Path -eq 'C:\Users\stoti\Chlorium\test-game\GeometryDash.exe'})
    if(!$game){Write-Output 'Le jeu de test est ferme ou en preparation.';exit 0}
    [IO.File]::WriteAllText($request,'stop')
    Write-Output 'Arret demande. Le jeu va fermer et restaurer les donnees.';exit 0
}
if(!$Level){
    Write-Output 'CHLORIUM GPU CONTINU - tests visibles'
    Write-Output '1 - Heliopolis auto | 2 - Grief auto | 3 - Orbit auto | 4 - Society auto'
    $choice=Read-Host 'Choix (Entree = Heliopolis)'
    $Level=switch($choice){''{129598490};'1'{129598490};'2'{88952972};'3'{133260800};'4'{139648964};default{throw 'Choisir 1, 2, 3 ou 4'}}
}
$wrapper=Join-Path $root 'research/R4-04/run-protected-test.ps1'
if($PrepareOnly){& $wrapper -Level $Level -Mode candidate -Canonical -Performance -PrepareOnly -FullLevel:$FullLevel;exit $LASTEXITCODE}
if(!$Controller){
    if(Get-Process GeometryDash,cl,link,ninja -ErrorAction SilentlyContinue){throw 'Un jeu ou une compilation est deja en cours'}
    Write-Output 'Preparation du test visible...'
    & $wrapper -Level $Level -Mode candidate -Canonical -Performance -PrepareOnly -FullLevel:$FullLevel | Out-Null
    $shell=(Get-Process -Id $PID).Path
    $log=Join-Path $root ('build-research/visible-launch-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss'))
    $arguments=@('-NoProfile','-ExecutionPolicy','RemoteSigned','-File',('"'+$PSCommandPath+'"'),'-Level',[string]$Level,'-Controller')
    if($FullLevel){$arguments+='-FullLevel'}
    $child=Start-Process -FilePath $shell -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput ($log+'.out.log') -RedirectStandardError ($log+'.err.log')
    @{controller_pid=$child.Id;level=$Level;log=$log;game_window='visible'} | ConvertTo-Json -Compress
    return
}
$mutex=[Threading.Mutex]::new($false,'Local\ChloriumVisibleTestSession')
if(!$mutex.WaitOne(0)){$mutex.Dispose();throw 'Un test visible est deja en cours'}
try{
    if(Get-Process GeometryDash,cl,link,ninja -ErrorAction SilentlyContinue){throw 'Ferme le jeu deja ouvert ou attends la compilation avant de lancer le test'}
    $revision='visible-r57-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,4)
    $campaign=Join-Path $root ('build-research/runtime-canonical-v10-isolated-'+$revision+'-performance')
    $stopRequest=Join-Path $campaign "reports-$Level/auto-$Level-chlorium.jsonl.stop-request"
    $session=@{status='running';level=$Level;full_level=[bool]$FullLevel;revision=$revision;stop_request=$stopRequest;controller_pid=$PID}
    $session | ConvertTo-Json | Set-Content -LiteralPath $marker -Encoding utf8
    Write-Output 'Lancement du renderer R57. Details complets, shake coupe, collisions normales.'
    Write-Output 'Regarde le compteur FPS et GPU ACTIF. Utilise Arreter-test.cmd pour finir.'
    & $wrapper -Level $Level -Mode candidate -Canonical -Performance -RunRevision $revision -FullLevel:$FullLevel
    $session.status='finished'
}catch{
    if($session){$session.status='failed';$session.error=$_.Exception.Message}
    throw
}finally{
    if($session){$session | ConvertTo-Json | Set-Content -LiteralPath $marker -Encoding utf8}
    $mutex.ReleaseMutex();$mutex.Dispose()
}
