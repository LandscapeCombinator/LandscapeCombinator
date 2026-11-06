param(
  [string]$Version = '',
  [string]$Work = 'C:\gdal-build',
  [string]$Generator = 'Visual Studio 17 2022'
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
. (Join-Path $PSScriptRoot 'GDALBuildCommon.ps1')
$Version = Resolve-GdalVersion $Version
Use-Cmake
$conda = (Get-Command conda, mamba, micromamba -ErrorAction SilentlyContinue | Select-Object -First 1).Source
if (-not $conda) { throw 'conda/mamba/micromamba not found on PATH' }

$envDir = Join-Path $Work 'env'
$lib = Join-Path $envDir 'Library'
$build = Join-Path $Work 'build'
$cores = [Environment]::ProcessorCount
New-Item -ItemType Directory -Force $Work | Out-Null

# conda only supplies the dependencies (proj, curl, ...); our own gdal.dll replaces its libgdal
if (-not (Test-Path $envDir)) {
  Run $conda @('create','-y','-p',$envDir,'-c','conda-forge','--override-channels',"libgdal-core=$Version")
}
foreach ($pkg in 'libgdal-core','libgdal','geos','libspatialite') {
  & $conda remove -y -p $envDir --force $pkg 2>$null | Out-Null   # fine if not installed
}
Run $conda @('install','-y','-p',$envDir,'-c','conda-forge','--override-channels','expat')

$src = Fetch $Work 'gdal' $Version "https://download.osgeo.org/gdal/$Version/gdal-$Version.tar.gz"
$condaBase = Fwd (& $conda info --base).Trim()
$pdb = '/PDBALTPATH:%_PDB%'   # keeps the build path out of the PDB reference inside the binaries
$off = 'GEOS','SPATIALITE','ICONV','LIBXML2','FREEXL','ARCHIVE' | ForEach-Object { "-DGDAL_USE_$($_)=OFF" }
$noBindings = 'PYTHON','JAVA','CSHARP' | ForEach-Object { "-DBUILD_$($_)_BINDINGS=OFF" }
Run 'cmake' (@('-S',$src,'-B',$build,'-G',$Generator,'-A','x64',
  "-DCMAKE_PREFIX_PATH=$lib","-DCMAKE_INSTALL_PREFIX=$lib","-DCMAKE_IGNORE_PREFIX_PATH=$condaBase",
  '-DGDAL_USE_GEOTIFF_INTERNAL=ON','-DGDAL_IGNORE_FAILED_CONDITIONS=ON','-DBUILD_TESTING=OFF',
  "-DCMAKE_SHARED_LINKER_FLAGS=$pdb","-DCMAKE_EXE_LINKER_FLAGS=$pdb",
  "-DCMAKE_CXX_FLAGS=/MP$cores","-DCMAKE_C_FLAGS=/MP$cores") + $off + $noBindings)
Run 'cmake' @('--build',$build,'--config','Release','--parallel',$cores)
Run 'cmake' @('--install',$build,'--config','Release')

Assert-NoCopyleft $envDir (Get-DllClosure $envDir).Copy
"built GDAL $Version in $envDir ; next: Scripts\UpgradeGDAL.ps1"
