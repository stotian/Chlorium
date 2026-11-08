#Requires -Version 7.0
param([Parameter(Mandatory)][int]$ProcessId, [Parameter(Mandatory)][string]$Report)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$reportPath=[IO.Path]::GetFullPath($Report)
if(!$reportPath.StartsWith((Join-Path $root 'build-research')+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {throw 'Only private diagnostic reports are accepted'}
$provenance=Get-Content -LiteralPath ($reportPath+'.provenance.json') -Raw | ConvertFrom-Json
if(!$provenance.visual_diagnostic -or $provenance.mod_profile -ne 'isolated') {throw 'Never inspect a performance or personal-mod run'}
$process=Get-Process -Id $ProcessId
$expected='C:\Users\stoti\Chlorium\test-game\GeometryDash.exe'
if($process.MainModule.FileName -ne $expected) {throw 'Only the dedicated GD fixture is accepted'}
if((Get-FileHash -LiteralPath $expected -Algorithm SHA256).Hash -ne $provenance.game_sha256) {throw 'Game image differs from diagnostic provenance'}

if(!('ChloriumReadOnlyHookMemory' -as [type])) {
    Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class ChloriumReadOnlyHookMemory {
    [DllImport("kernel32.dll",SetLastError=true)] public static extern IntPtr OpenProcess(uint access,bool inherit,int pid);
    [DllImport("kernel32.dll",SetLastError=true)] public static extern bool ReadProcessMemory(IntPtr handle,IntPtr address,byte[] data,UIntPtr size,out UIntPtr read);
    [DllImport("kernel32.dll",SetLastError=true,CharSet=CharSet.Unicode)] public static extern bool QueryFullProcessImageName(IntPtr handle,uint flags,System.Text.StringBuilder path,ref uint length);
    [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr handle);
}
'@
}
$handle=[ChloriumReadOnlyHookMemory]::OpenProcess(0x410,$false,$ProcessId)
if($handle -eq [IntPtr]::Zero) {throw 'Cannot open read-only fixture handle'}
try {
    $name=[Text.StringBuilder]::new(1024); [uint32]$length=1024
    if(![ChloriumReadOnlyHookMemory]::QueryFullProcessImageName($handle,0,$name,[ref]$length) -or $name.ToString() -ne $expected) {throw 'Fixture handle identity changed'}
    function Read-Bytes([long]$address,[int]$count) {
        if($address -le 0 -or $count -lt 1 -or $count -gt 256) {throw 'Memory-read bounds rejected'}
        $bytes=[byte[]]::new($count); [UIntPtr]$read=[UIntPtr]::Zero
        if(![ChloriumReadOnlyHookMemory]::ReadProcessMemory($handle,[IntPtr]::new($address),$bytes,[UIntPtr]::new([uint64]$count),[ref]$read) -or $read.ToUInt64() -ne $count) {throw 'Incomplete fixture code read'}
        return ,$bytes
    }
    $entry=$process.MainModule.BaseAddress.ToInt64()+0x3af3a0
    $entryBytes=Read-Bytes $entry 16
    [long]$target=$entry
    $jumps=[Collections.Generic.List[object]]::new()
    for($i=0;$i -lt 4;$i++) {
        $bytes=Read-Bytes $target 16
        $next=$null
        if($bytes[0] -eq 0xe9) {$next=$target+5+[BitConverter]::ToInt32($bytes,1)}
        elseif($bytes[0] -eq 0xff -and $bytes[1] -eq 0x25) {
            $slot=$target+6+[BitConverter]::ToInt32($bytes,2)
            $next=[BitConverter]::ToInt64((Read-Bytes $slot 8),0)
        }
        if($null -eq $next) {break}
        $jumps.Add(@{from=('0x{0:x}' -f $target);to=('0x{0:x}' -f $next)})
        $target=$next
    }
    if(!$jumps.Count) {throw 'Visibility is not routed through an observed handler'}
    $handler=Read-Bytes $target 256
    $geode=@($process.Modules | Where-Object ModuleName -eq 'Geode.dll')
    if($geode.Count -ne 1) {throw 'Loaded Geode image is ambiguous'}
    $receipt=[ordered]@{diagnostic_only=$true;read_only=$true;pid=$ProcessId;process_start_utc=$process.StartTime.ToUniversalTime().ToString('o');game_sha256=$provenance.game_sha256;report=$reportPath;function_rva='0x3af3a0';entry_hex=[Convert]::ToHexString($entryBytes);jumps=$jumps;handler_address=('0x{0:x}' -f $target);handler_hex=[Convert]::ToHexString($handler);geode_path=$geode[0].FileName;geode_sha256=(Get-FileHash -LiteralPath $geode[0].FileName -Algorithm SHA256).Hash;limits='One bounded code read of an existing auditor hook; not execution parity or FPS evidence'}
    $destination=$reportPath+'.native-hook.local.json'
    $stream=[IO.File]::Open($destination,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try {
        $writer=[IO.StreamWriter]::new($stream,[Text.UTF8Encoding]::new($false))
        try {$writer.Write(($receipt | ConvertTo-Json -Depth 6))} finally {$writer.Dispose()}
    } finally {$stream.Dispose()}
    [ordered]@{receipt=$destination;pid=$ProcessId;geode_sha256=$receipt.geode_sha256;jump_count=$jumps.Count;bytes=$handler.Length} | ConvertTo-Json
} finally {[void][ChloriumReadOnlyHookMemory]::CloseHandle($handle)}
