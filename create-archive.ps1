param (
    [string]$PluginVersion,
    [string]$EngineVersion
)

Set-StrictMode -Version 3

$ErrorActionPreference = 'Stop'

function New-Archive {
    param(
        [string]$Plugin
    )

    Write-Output "Preparing archive for $Plugin plugin"

    $TempFolder = "TEMP_$Plugin"

    New-Item -ItemType Directory -Path $TempFolder -Force | Out-Null

    Copy-Item -Path Config -Destination $TempFolder -Force -Recurse
    Copy-Item -Path Content -Destination $TempFolder -Force -Recurse
    Copy-Item -Path LandscapeCombinator.uplugin -Destination $TempFolder/$Plugin.uplugin -Force

    foreach ($Dependency in $Dependencies[$Plugin])
    {
        Copy-Item -Path "Source/$Dependency" -Destination "$TempFolder/Source/$Dependency" -Force -Recurse
    }

    # Modify uplugin file
 
    $UPlugin = Get-Content "$TempFolder/$Plugin.uplugin" -Raw
    $NewUPlugin = $UPlugin

    $NewUPlugin = $NewUPlugin -replace "`"FriendlyName`": `"LandscapeCombinator`"", "`"FriendlyName`": `"$Plugin`""
    $NewUPlugin = $NewUPlugin -replace
        "`"MarketplaceURL`": `".*`"",
        "`"MarketplaceURL`": `"com.epicgames.launcher://ue/marketplace/product/$($MarketplaceURLs[$Plugin])`""
    $NewUPlugin = $NewUPlugin -replace
        "`"EngineVersion`": `".*`"",
        "`"EngineVersion`": `"$EngineVersion`""
    $NewUPlugin = $NewUPlugin -replace
        "`"VersionName`": `".*`"",
        "`"VersionName`": `"$PluginVersion`""

    $PluginDependencies = $Dependencies[$Plugin]

    foreach ($Module in $Modules)
    {
        if (!$PluginDependencies.Contains($Module))
        {
            $NewUPlugin = $NewUPlugin -replace "[\s\r]*\n.*{[\n\s]+`"Name`": `"$Module`".*\n.*\n.*\n.*\n.*},?", ""
        }
    }
    
    Set-Content $TempFolder/$Plugin.uplugin $NewUPlugin


    # GDAL

    if (!$PluginDependencies.Contains("GDALInterface"))
    {
        # Remove GDAL from FilterPlugin.ini for plugins that do not depend on GDAL
        $FilterPlugin = Get-Content "$TempFolder/Config/FilterPlugin.ini" -Raw
        $FilterPlugin = $FilterPlugin -replace ".*ThirdParty.*\n.*\n", ""
        $FilterPlugin = $FilterPlugin -replace ".*Binaries.*\n.*\n", ""
        Set-Content "$TempFolder/Config/FilterPlugin.ini" $FilterPlugin    
    }
    else
    {
        # Copy ThirdParty/GDAL for plugins that depend on it
        Copy-Item -Path "Source/ThirdParty" -Destination "$TempFolder/Source/ThirdParty" -Force -Recurse

        # And the GDAL DLLs from the Binaries folder, otherwise they don't appear in the Engine package
        New-Item -Path "$TempFolder" -Name "Binaries" -ItemType "directory" | Out-Null
        New-Item -Path "$TempFolder/Binaries" -Name "Win64" -ItemType "directory" | Out-Null
        Copy-Item -Path Binaries/Win64/*.dll -Exclude Unreal* -Destination "$TempFolder/Binaries/Win64" -Force
    }


    # Remove Examples folder for all plugins except LandscapeCombinator and ImageDownloader
    
    if ($Plugin -ne "LandscapeCombinator" -and $Plugin -ne "ImageDownloader")
    {
        $FilterPlugin = Get-Content "$TempFolder/Config/FilterPlugin.ini" -Raw
        $FilterPlugin = $FilterPlugin -replace ".*Examples.*\n.*\n", ""
        Set-Content "$TempFolder/Config/FilterPlugin.ini" $FilterPlugin    
    }


    # Remove LandscapeCombinatorMap from Content folder for all plugins except LandscapeCombinator

    if ($Plugin -ne "LandscapeCombinator")
    {
        Remove-Item -Recurse -Force "$TempFolder/Content/__ExternalActors__"
        Remove-Item -Recurse -Force "$TempFolder/Content/__ExternalObjects__"
        Remove-Item -Force "$TempFolder/Content/LandscapeCombinatorMap.umap"
    }

    Write-Output "Running: 7z a $plugin-v$PluginVersion-UE$EngineVersion.zip $(Resolve-Path "$TempFolder/*" | Select-Object -ExpandProperty Path)"
    & 7z a "$plugin-v$PluginVersion-UE$EngineVersion.zip" (Resolve-Path "$TempFolder/*" | Select-Object -ExpandProperty Path)

    Remove-Item -Force -Recurse $TempFolder
}

function Get-Direct-Dependencies {
    param(
        [string]$Module
    )

    $Build = Get-Content Source/$Module/$Module.Build.cs -Raw

    $Dependencies = [regex]::Matches($Build, '"([^"]*)"') | ForEach-Object { $_.Groups[1].Value }
    $Dependencies += $Module;

    return $Dependencies | Where-Object { $Modules -contains $_ }
}

function Update-Transitive {
    param(
        [HashTable]$Graph        
    )

    $Changed = $true

    while ($Changed) {
        $Changed = $false
        $UpdatedGraph = @{}

        foreach ($Node in $Graph.Keys) {
            $ReachableNodes = @($Graph[$Node])

            foreach ($Node1 in $Graph[$Node]) {
                foreach ($Node2 in $Graph[$Node1]) {
                    if (!$ReachableNodes.Contains($Node2)) {
                        $Changed = $true;
                        $ReachableNodes += $Node2
                    }
                }
            }

            $UpdatedGraph[$Node] = $ReachableNodes
        }

        $Graph = $UpdatedGraph
    }

    $Graph
}

function Write-Graph {
    param(
        [HashTable]$Graph        
    )

    foreach ($Key in $Graph.Keys)
    {
        Write-Output $Key":"
    
        foreach ($Value in $Graph[$Key])
        {
            Write-Output $Value
        }
    
        Write-Output "`n"
    }

}


$Modules = Get-ChildItem Source | Select-Object -ExpandProperty Name
$Modules = $Modules | Where-Object { $_ -ne "ThirdParty" }

$DirectDependencies = @{}

foreach($Module in $Modules)
{
    $DirectDependencies.Add($Module, $(Get-Direct-Dependencies($Module)))
}

$Dependencies = Update-Transitive $DirectDependencies

$MarketPlaceURLs = @{}
$MarketPlaceURLs.Add("LandscapeCombinator", "675e54fcb72f42db82125d5573d0667e")
$MarketPlaceURLs.Add("ImageDownloader",     "")
$MarketPlaceURLs.Add("BuildingFromSpline",  "64c9fedf0ff44d9ba0137db8721f47b6")
$MarketPlaceURLs.Add("Coordinates",         "11fabe5dcee545338cc6818f5e465bf6")
$MarketPlaceURLs.Add("SplineImporter",      "")
$MarketPlaceURLs.Add("HeightmapModifier",   "")

Remove-Item -Force -Recurse "*UE$EngineVersion*.zip"

foreach($Plugin in
    @(
        "LandscapeCombinator",
        "ImageDownloader",
        "BuildingFromSpline",
        "SplineImporter",
        "Coordinates",
        "HeightmapModifier"
    ))
{
    New-Archive -Plugin $Plugin;
}
