param (
    [string]$NewVersion,
    [string]$ThirdPartyGDAL
)

$DLLFiles = Get-ChildItem "$ThirdPartyGDAL/bin"
foreach ($DLLFile in $DLLFiles)
{
    $DLLName = (Get-Item $DLLFile).Name
    if (Test-Path "$NewVersion/Library/bin/$DLLName")
    {
        Copy-Item -Path "$NewVersion/Library/bin/$DLLName" -Destination $DLLFile -Force
    }
    else
    {
        Remove-Item $DLLFile
    }
}

$Headers = Get-ChildItem "$ThirdPartyGDAL/include"
foreach ($Header in $Headers)
{
    $HeaderName = (Get-Item $Header).Name
    Copy-Item -Path "$NewVersion/Library/include/$HeaderName" -Destination $Header -Force
}

Copy-Item -Path "$NewVersion/Library/lib/gdal.lib" -Destination "$ThirdPartyGDAL/lib/gdal.lib" -Force

Remove-Item -Recurse "$ThirdPartyGDAL/share/gdal"
Remove-Item -Recurse "$ThirdPartyGDAL/share/proj"

Copy-Item -Path "$NewVersion/Library/share/gdal" -Destination "$ThirdPartyGDAL/share/gdal" -Force -Recurse
Copy-Item -Path "$NewVersion/Library/share/proj" -Destination "$ThirdPartyGDAL/share/proj" -Force -Recurse
