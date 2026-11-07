param(
  [Parameter(Mandatory)][string]$Arm64,   # unzipped gdal-mac-arm64 artifact (bin, include, share)
  [Parameter(Mandatory)][string]$X64,     # unzipped gdal-mac-x86_64 artifact
  [Parameter(Mandatory)][string]$Out
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'GDALBuildCommon.ps1')
$lib = 'libgdal.dylib'
foreach ($p in $Arm64, $X64) { foreach ($s in "bin/$lib", 'include', 'share/gdal', 'share/proj') { if (-not (Test-Path "$p/$s")) { throw "missing: $p/$s" } } }
Remove-Item $Out -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force "$Out/bin" | Out-Null
foreach ($s in 'include', 'share') { Copy-Item "$Arm64/$s" "$Out/$s" -Recurse }   # headers and data are identical across archs
Run 'lipo' @('-create', "$Arm64/bin/$lib", "$X64/bin/$lib", '-output', "$Out/bin/$lib")
Run 'codesign' @('--force', '-s', '-', "$Out/bin/$lib")   # lipo drops the signature
(& lipo -info "$Out/bin/$lib")