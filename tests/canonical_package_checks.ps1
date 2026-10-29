$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot -Parent
$fixture=Join-Path $project ('build-tests/canonical-package-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $fixture 'build') -Force | Out-Null
# Marker-only synthetic DLLs exercise packaging, never executable loading.
foreach($name in @('package-local.ps1','mod.json','about.md','changelog.md','support.md','logo.png','LICENSE.md','COPYING','NOTICE.md','THIRD_PARTY_NOTICES.md')) {
    Copy-Item -LiteralPath (Join-Path $project $name) -Destination $fixture
}
foreach($name in @('LICENSES','resources')) {Copy-Item -LiteralPath (Join-Path $project $name) -Destination $fixture -Recurse}
@'
CHLORIUM_EXPERIMENTAL_NATIVE_QUADS:BOOL=ON
CHLORIUM_NATIVE_VERTEX_PARITY:BOOL=ON
CHLORIUM_EXPERIMENTAL_NATIVE_SNAPSHOTS:BOOL=ON
'@ | Set-Content -LiteralPath (Join-Path $fixture 'build/CMakeCache.txt') -Encoding ASCII
$common='CHLORIUM_NORMAL_BINARY_V1 CHLORIUM_EXPERIMENTAL_NATIVE_SNAPSHOTS_V1 '
$canonical=$common+'CHLORIUM_NATIVE_CANONICAL_BACKEND_V10 CHLORIUM_CANONICAL_AREA_QUOTIENTS_V10'
$dll=Join-Path $fixture 'build/local.chlorium.dll'
[IO.File]::WriteAllText($dll,$canonical,[Text.Encoding]::ASCII)
& (Join-Path $fixture 'package-local.ps1') -OutputDirectory packages | Out-Null
$package=Join-Path $fixture 'packages/local.chlorium.geode'
if(!(Test-Path -LiteralPath $package)){throw 'Canonical package missing'}
$hash=(Get-FileHash -LiteralPath $package).Hash
Write-Output 'PASS canonical V10 accepted in normal packaging mode'
$exporter=Join-Path $fixture 'tools/export-documents.ps1'
New-Item -ItemType Directory -Path (Split-Path $exporter) -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $project 'tools/export-documents.ps1') -Destination $exporter
foreach($name in @('.gitignore','.gitattributes','README.md','RESEARCH_BASELINE.md','CMakeLists.txt','build-local.cmd')) {
    Copy-Item -LiteralPath (Join-Path $project $name) -Destination $fixture
}
Get-ChildItem -LiteralPath $project -Filter 'test-*.cmd' -File | Copy-Item -Destination $fixture
foreach($name in @('src','legacy','docs','tests','.github','dist')) {
    New-Item -ItemType Directory -Path (Join-Path $fixture $name) -Force | Out-Null
}
$export=Join-Path $fixture 'export'
New-Item -ItemType Directory -Path (Join-Path $fixture 'src/graphify-out') -Force | Out-Null
Set-Content -LiteralPath (Join-Path $fixture 'src/graphify-out/private-cache.json') -Value '{}'
Copy-Item -LiteralPath $package -Destination (Join-Path $fixture 'dist/local.chlorium.geode')
& $exporter -Destination $export | Out-Null
$exported=Join-Path $export 'local.chlorium.geode'
if((Get-FileHash -LiteralPath $exported).Hash -ne $hash){throw 'Exported package changed'}
if(!(Test-Path -LiteralPath (Join-Path $export 'chlorium-sources.zip'))){throw 'Exported sources missing'}
$zip=[IO.Compression.ZipFile]::OpenRead((Join-Path $export 'chlorium-sources.zip'))
try {if($zip.Entries.FullName -match 'graphify-out'){throw 'Generated graph cache entered source export'}}finally{$zip.Dispose()}
$cases=@(
    ($common+'CHLORIUM_NATIVE_VERTEX_BACKEND_V5')
    ($canonical+' CHLORIUM_NATIVE_VERTEX_BACKEND_V5')
    ($common+'CHLORIUM_NATIVE_CANONICAL_BACKEND_V10')
)
if($cases.Count -ne 3){throw 'Wrong negative fixture cardinality'}
foreach($markers in $cases) {
    [IO.File]::WriteAllText($dll,$markers,[Text.Encoding]::ASCII)
    $rejected=$false
    try {& (Join-Path $fixture 'package-local.ps1') -OutputDirectory packages | Out-Null}
    catch {if($_.Exception.Message -match 'canonical V10 identity|Retired V5 renderer'){$rejected=$true}else{throw}}
    if(!$rejected){throw 'Wrong binary accepted'}
    if((Get-FileHash -LiteralPath $package).Hash -ne $hash){throw 'Rejected binary changed the existing package'}
    $invalid=Join-Path $fixture 'dist/local.chlorium.geode'
    Copy-Item -LiteralPath $package -Destination $invalid -Force
    $zip=[IO.Compression.ZipFile]::Open($invalid,[IO.Compression.ZipArchiveMode]::Update)
    try {
        $zip.GetEntry('local.chlorium.dll').Delete()
        $writer=[IO.StreamWriter]::new($zip.CreateEntry('local.chlorium.dll').Open())
        try {$writer.Write($markers)}finally{$writer.Dispose()}
    }finally{$zip.Dispose()}
    $rejected=$false
    try {& $exporter -Destination $export | Out-Null}
    catch {if($_.Exception.Message -match 'Only the canonical V10 renderer'){$rejected=$true}else{throw}}
    if(!$rejected -or (Get-FileHash -LiteralPath $exported).Hash -ne $hash){throw 'Invalid export accepted or prior export changed'}
}
Write-Output 'PASS V5, mixed V5/V10 and incomplete V10 rejected; prior package unchanged'
Write-Output 'PASS real source exporter accepts canonical V10 and rejects the same invalid identities before replacing files'
Copy-Item -LiteralPath $package -Destination (Join-Path $fixture 'dist/local.chlorium.geode') -Force
$archiveHash=(Get-FileHash -LiteralPath (Join-Path $export 'chlorium-sources.zip')).Hash
Remove-Item -LiteralPath (Join-Path $fixture 'README.md')
$rejected=$false
try {& $exporter -Destination $export | Out-Null}
catch {if($_.Exception.Message -match 'README.md'){$rejected=$true}else{throw}}
if(!$rejected -or (Get-FileHash -LiteralPath $exported).Hash -ne $hash -or
    (Get-FileHash -LiteralPath (Join-Path $export 'chlorium-sources.zip')).Hash -ne $archiveHash){throw 'Incomplete source export changed prior package/archive'}
if(Get-ChildItem -LiteralPath $export -Directory -Force -Filter '.chlorium-export-*'){throw 'Export staging not cleaned'}
Write-Output 'PASS missing source rejects export, preserves prior package/archive hashes and cleans staging'
