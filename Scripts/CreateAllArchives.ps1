param (
    [string]$PluginVersion,
    [string[]]$EngineVersions
)

Set-StrictMode -Version 3

$ErrorActionPreference = 'Stop'

$Manifests = Get-ChildItem "$env:ProgramData\Epic\EpicGamesLauncher\Data\Manifests\*.item"
$Manifests = $Manifests | Sort-Object { $v = (Get-Content $_ -Raw | ConvertFrom-Json).AppName -replace '^UE_', ''; if ($v -match '^\d+\.\d+$') { [version]$v } else { [version]'0.0' } }

foreach ($Manifest in $Manifests)
{
    $Item = Get-Content $Manifest -Raw | ConvertFrom-Json

    if ($Item.AppName -match '^UE_(\d+\.\d+)$')
    {
        $EngineVersion = $Matches[1]
        if ($EngineVersions -and $EngineVersions -notcontains $EngineVersion) { continue }
        Write-Output "=== $PluginVersion for UE $EngineVersion"

        $global:LASTEXITCODE = 0
        & "$PSScriptRoot\CreateArchive.ps1" -PluginVersion $PluginVersion -EngineVersion $EngineVersion -UEFolder $Item.InstallLocation
        if ($LASTEXITCODE) { throw "CreateArchive failed for UE $EngineVersion" }
    }
}
