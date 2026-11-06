param(
  [string]$WinEnv = 'C:\gdal-build\env',
  [string]$WinBuild = 'C:\gdal-build\build',
  [string]$LinuxWork = 'C:\gdal-build-linux',
  [string]$ThirdPartyGDAL = (Join-Path $PSScriptRoot '..\Source\ThirdParty\GDAL')
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'GDALBuildCommon.ps1')
Use-Cmake

$ThirdPartyGDAL = (Resolve-Path $ThirdPartyGDAL).Path
$win = Join-Path $ThirdPartyGDAL 'Win64'
$lin = Join-Path $ThirdPartyGDAL 'Linux'
$lib = Join-Path $WinEnv 'Library'
$winStage = Join-Path (Split-Path $WinBuild) 'install'
$linStage = Join-Path $LinuxWork 'install'
$linSo = Join-Path $LinuxWork 'out\libgdal.so'
$linProj = Join-Path $LinuxWork 'deps\share\proj'

# checks first, nothing is touched until they pass
$cl = Get-DllClosure $WinEnv
if ($cl.Missing) { throw "unresolved DLLs: $($cl.Missing -join ', ')" }
Assert-NoCopyleft $WinEnv $cl.Copy
foreach ($p in $linSo, $linProj) { if (-not (Test-Path $p)) { throw "missing: $p" } }

# fresh install trees: headers, share\gdal, gdal.lib
Remove-Item $winStage, $linStage -Recurse -Force -ErrorAction SilentlyContinue
Run 'cmake' @('--install', $WinBuild, '--config', 'Release', '--prefix', $winStage) | Out-Null
Run 'cmake' @('--install', (Join-Path $LinuxWork 'gdal-build'), '--prefix', $linStage) | Out-Null

# replace the plugin's GDAL folders
foreach ($d in $win, $lin) { foreach ($s in 'bin','lib','include','share') { Remove-Item "$d\$s" -Recurse -Force -ErrorAction SilentlyContinue } }
New-Item -ItemType Directory -Force "$win\bin","$win\lib","$win\share","$lin\bin","$lin\share" | Out-Null
foreach ($n in $cl.Copy) { Copy-Item "$lib\bin\$n" "$win\bin" }
Copy-Item "$winStage\lib\gdal.lib" "$win\lib"
Copy-Item "$winStage\include" "$win\include" -Recurse
Copy-Item "$winStage\share\gdal" "$win\share\gdal" -Recurse
Copy-Item "$lib\share\proj" "$win\share\proj" -Recurse
Copy-Item $linSo "$lin\bin"
Copy-Item "$linStage\include" "$lin\include" -Recurse
Copy-Item "$linStage\share\gdal" "$lin\share\gdal" -Recurse
Copy-Item $linProj "$lin\share\proj" -Recurse

# UE does not overwrite newer-dated DLLs in Binaries\Win64: delete old copies (names from git HEAD + new set)
$names = @(git -C $ThirdPartyGDAL ls-tree -r --name-only HEAD Win64/bin | ForEach-Object { Split-Path $_ -Leaf }) + $cl.Copy
foreach ($bd in "$PSScriptRoot\..\Binaries\Win64", "$PSScriptRoot\..\..\..\Binaries\Win64") {
  if (Test-Path $bd) { Get-ChildItem $bd -Filter *.dll | Where-Object { $names -contains $_.Name } | Remove-Item -Force }
}

# UBT caches the build plan, and Build.cs lists the DLLs
(Get-Item (Join-Path $PSScriptRoot '..\Source\GDALInterface\GDALInterface.Build.cs')).LastWriteTime = Get-Date
"done: $($cl.Copy.Count) Windows DLLs, libgdal.so $([math]::Round((Get-Item $linSo).Length / 1MB, 1)) MB"
