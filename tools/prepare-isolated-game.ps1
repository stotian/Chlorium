$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$source = 'C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash'
$target = Join-Path $root 'test-game'
New-Item -ItemType Directory -Force -Path $target | Out-Null
Get-ChildItem -LiteralPath $source -File | Where-Object { $_.Extension -eq '.dll' -or $_.Name -in @('GeometryDash.exe', 'steam_appid.txt', 'GeodeUpdater.exe') } | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $target $_.Name)
}
$resources = Join-Path $target 'Resources'
if (!(Test-Path -LiteralPath $resources)) {
    New-Item -ItemType Junction -Path $resources -Value (Join-Path $source 'Resources') | Out-Null
}
New-Item -ItemType Directory -Force -Path (Join-Path $target 'geode\mods') | Out-Null
Copy-Item -LiteralPath (Join-Path $source 'geode\resources') -Destination (Join-Path $target 'geode') -Recurse -Force
Copy-Item -LiteralPath (Join-Path $source 'geode\mods\geode.node-ids.geode') -Destination (Join-Path $target 'geode\mods\geode.node-ids.geode')
Write-Output "Isolated game fixture: $target"
