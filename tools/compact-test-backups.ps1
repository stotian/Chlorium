#Requires -Version 7.0
$ErrorActionPreference='Stop'
if(Get-Process cl,link,ninja,GeometryDash -ErrorAction SilentlyContinue){throw 'Active build/game; refusing backup compaction'}
$backupRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../build-research'))
$freeBefore=(Get-PSDrive C).Free
if((Get-Item -LiteralPath $backupRoot).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Research root is a reparse point'}
$masters=@{};$checkedParents=@{};$links=[Collections.Generic.List[object]]::new()
function Assert-BackupPath([string]$value){
    $path=[IO.Path]::GetFullPath($value)
    if(!$path.StartsWith($backupRoot+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Backup path escaped research directory'}
    $parent=[IO.Path]::GetDirectoryName($path)
    while($parent -and $parent -ne $backupRoot){
        if($checkedParents.ContainsKey($parent)){break}
        $item=Get-Item -LiteralPath $parent -Force
        if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Backup parent is a reparse point'}
        $checkedParents[$parent]=$true;$parent=$item.Parent.FullName
    }
    return $path
}
foreach($manifestFile in (Get-ChildItem -LiteralPath $backupRoot -Recurse -File -Filter 'profile-backup.local.json')){
    $manifest=Get-Content -LiteralPath $manifestFile.FullName -Raw | ConvertFrom-Json
    if($manifest.status -ne 'restored_and_sha256_verified'){continue}
    $before=Join-Path $manifestFile.DirectoryName 'profile-before'
    foreach($record in $manifest.files){
        $path=Assert-BackupPath (Join-Path $before $record.relative)
        if(!(Test-Path -LiteralPath $path -PathType Leaf)){continue}
        $item=Get-Item -LiteralPath $path -Force
        # Large immutable copies dominate storage; leave small configs and existing links alone.
        if($item.Length -lt 1MB -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){continue}
        $hash=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        if($hash -ne $record.sha256){throw 'Backup content differs from its verified manifest'}
        $key=$hash+':'+$item.Length
        if(!$masters.ContainsKey($key)){$masters[$key]=$path;continue}
        if($item.LinkType -eq 'HardLink'){continue}
        $master=Assert-BackupPath $masters[$key]
        $temporary=Assert-BackupPath ($path+'.dedup-'+[Guid]::NewGuid().ToString('N'))
        try{
            New-Item -ItemType HardLink -Path $temporary -Target $master | Out-Null
            if((Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash -ne $hash){throw 'Temporary link content mismatch'}
            [IO.File]::Replace($temporary,$path,[NullString]::Value)
            if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $hash){throw 'Final backup content mismatch'}
            $links.Add(@{path=$path;master=$master;sha256=$hash;bytes=$item.Length})
        }finally{if(Test-Path -LiteralPath $temporary){Remove-Item -LiteralPath $temporary}}
    }
}
$bytes=($links | Measure-Object bytes -Sum).Sum
@{backups_still_restorable=$true;only_completed_verified_backups=$true;files=$links.Count;logical_duplicate_bytes_avoided=$bytes;disk_free_before=$freeBefore;disk_free_after=(Get-PSDrive C).Free;links=$links} |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $backupRoot ('backup-compaction-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')+'.local.json')) -Encoding utf8
@{files=$links.Count;logical_duplicates_gib=[Math]::Round($bytes/1GB,2);disk_free_gib=[Math]::Round((Get-PSDrive C).Free/1GB,2)} | ConvertTo-Json -Compress
