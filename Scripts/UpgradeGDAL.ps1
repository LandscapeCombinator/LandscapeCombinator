param(
  [string]$WinEnv = 'C:\gdal-build\env',
  [string]$WinBuild = 'C:\gdal-build\build',
  [string]$LinuxWork = 'C:\gdal-build-linux',
  [string]$MacOut = '',   # unzipped gdal-mac artifact (bin, include, share); empty = leave Mac\ untouched
  [string]$ThirdPartyGDAL = (Join-Path $PSScriptRoot '..\Source\ThirdParty\GDAL')
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'GDALBuildCommon.ps1')
Use-Cmake

$ThirdPartyGDAL = (Resolve-Path $ThirdPartyGDAL).Path
$win = Join-Path $ThirdPartyGDAL 'Win64'
$lib = Join-Path $WinEnv 'Library'
$winStage = Join-Path (Split-Path $WinBuild) 'install'
# Linux/Mac: destination folder = build output folder (bin, include, share)
$unix = [ordered]@{ (Join-Path $ThirdPartyGDAL 'Linux') = (Join-Path $LinuxWork 'out') }
if ($MacOut) { $unix[(Join-Path $ThirdPartyGDAL 'Mac')] = (Resolve-Path $MacOut).Path }

# checks first, nothing is touched until they pass
$cl = Get-DllClosure $WinEnv
if ($cl.Missing) { throw "unresolved DLLs: $($cl.Missing -join ', ')" }
Assert-NoCopyleft $WinEnv $cl.Copy
foreach ($o in $unix.Values) { foreach ($s in 'bin','include','share\gdal','share\proj') { if (-not (Test-Path (Join-Path $o $s))) { throw "missing: $o\$s" } } }

# fresh install tree: headers, share\gdal, gdal.lib
Remove-Item $winStage -Recurse -Force -ErrorAction SilentlyContinue
Run 'cmake' @('--install', $WinBuild, '--config', 'Release', '--prefix', $winStage) | Out-Null

# replace the plugin's GDAL folders
foreach ($s in 'bin','lib','include','share') { Remove-Item "$win\$s" -Recurse -Force -ErrorAction SilentlyContinue }
New-Item -ItemType Directory -Force "$win\bin","$win\lib","$win\share" | Out-Null
foreach ($n in $cl.Copy) { Copy-Item "$lib\bin\$n" "$win\bin" }
Copy-Item "$winStage\lib\gdal.lib" "$win\lib"
Copy-Item "$winStage\include" "$win\include" -Recurse
Copy-Item "$winStage\share\gdal" "$win\share\gdal" -Recurse
Copy-Item "$lib\share\proj" "$win\share\proj" -Recurse
foreach ($dest in $unix.Keys) { Install-UnixTree $unix[$dest] $dest }

# UE does not overwrite newer-dated DLLs in Binaries\Win64: delete old copies (names from git HEAD + new set)
$names = @(git -C $ThirdPartyGDAL ls-tree -r --name-only HEAD Win64/bin | ForEach-Object { Split-Path $_ -Leaf }) + $cl.Copy
foreach ($bd in "$PSScriptRoot\..\Binaries\Win64", "$PSScriptRoot\..\..\..\Binaries\Win64") {
  if (Test-Path $bd) { Get-ChildItem $bd -Filter *.dll | Where-Object { $names -contains $_.Name } | Remove-Item -Force }
}

# UBT caches the build plan, and Build.cs lists the DLLs
(Get-Item (Join-Path $PSScriptRoot '..\Source\GDALInterface\GDALInterface.Build.cs')).LastWriteTime = Get-Date
"done: $($cl.Copy.Count) Windows DLLs, Linux $([math]::Round((Get-Item (Join-Path $LinuxWork 'out\bin\libgdal.so')).Length / 1MB, 1)) MB" + $(if ($MacOut) { ', Mac updated' } else { '' })