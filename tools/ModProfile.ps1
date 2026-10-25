function Get-ResearchModProfile {
    param([Parameter(Mandatory)][string]$ModsDirectory,[Parameter(Mandatory)][string]$ProfileRoot)
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $loaderPath = Join-Path $ProfileRoot 'geode/mods/geode.loader/saved.json'
    $preferences = if (Test-Path -LiteralPath $loaderPath) {
        Get-Content -LiteralPath $loaderPath -Raw | ConvertFrom-Json -AsHashtable
    } else { @{} }
    $entries = [Collections.Generic.List[object]]::new()
    $settings = [Collections.Generic.List[string]]::new()
    $ids = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($file in Get-ChildItem -LiteralPath $ModsDirectory -File -Filter '*.geode' | Sort-Object Name) {
        $archive = [IO.Compression.ZipFile]::OpenRead($file.FullName)
        try {
            $entry = $archive.GetEntry('mod.json')
            if (!$entry -or $entry.Length -gt 1MB) { throw 'Missing or oversized Geode manifest' }
            $reader = [IO.StreamReader]::new($entry.Open())
            try { $mod = $reader.ReadToEnd() | ConvertFrom-Json } finally { $reader.Dispose() }
        } finally { $archive.Dispose() }
        if ($mod.id -eq 'local.chlorium') { continue } # Paired experimental variable.
        if ($mod.id -notmatch '^[a-z0-9_-]+\.[a-z0-9_.-]+$' -or !$mod.version -or !$ids.Add($mod.id)) {
            throw 'Invalid or duplicate common mod identity'
        }
        $key = 'should-load-' + $mod.id
        $enabled = !$preferences.ContainsKey($key) -or [bool]$preferences[$key]
        $entries.Add([ordered]@{id=$mod.id;version=$mod.version;sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash;requested_enabled=$enabled})
        foreach ($name in @('settings.json','saved.json')) {
            $path = Join-Path $ProfileRoot ('geode/mods/' + $mod.id + '/' + $name)
            $hash = if (Test-Path -LiteralPath $path) {
                if ((Get-Item -LiteralPath $path).Length -gt 16MB) { throw 'Common mod settings exceed fingerprint budget' }
                (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
            } else { 'absent' }
            $settings.Add($mod.id + '/' + $name + ':' + $hash)
        }
    }
    $loaderSettings = Join-Path $ProfileRoot 'geode/mods/geode.loader/settings.json'
    $settings.Add('geode.loader/settings.json:' + $(if (Test-Path -LiteralPath $loaderSettings) {
        (Get-FileHash -LiteralPath $loaderSettings -Algorithm SHA256).Hash
    } else { 'absent' }))
    $hashText = { param([string]$text) [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($text))) }
    return [pscustomobject]@{
        schema='isolated-mod-profile-v2'
        mods=$entries.ToArray()
        profile_sha256=(& $hashText ('isolated-mod-profile-v2:' + ($entries.ToArray() | ConvertTo-Json -Depth 6 -Compress)))
        settings_sha256=(& $hashText ($settings -join "`n"))
    }
}
