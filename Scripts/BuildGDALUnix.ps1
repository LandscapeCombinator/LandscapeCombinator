param(
  [Parameter(Mandatory)][ValidateSet('Linux','Mac')][string]$Platform,
  [string]$Version = '',
  [string]$ProjVersion = '',
  [string]$Work = '',
  [string]$Toolchain = 'C:\UnrealToolchains\v26_clang-20.1.8-rockylinux8\x86_64-unknown-linux-gnu',   # Linux only
  [string]$WinEnv = 'C:\gdal-build\env',   # Linux only: host sqlite3.exe and PROJ version
  [string]$MacArch = 'arm64',              # Mac only
  [string]$MacTarget = '11.0',             # Mac only: minimum macOS version
  [string]$SqliteYear = '2025',
  [string]$SqliteNum = '3500400'
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
. (Join-Path $PSScriptRoot 'GDALBuildCommon.ps1')
$Version = Resolve-GdalVersion $Version

if (-not $Work) { $Work = if ($Platform -eq 'Mac') { "$HOME/gdal-build-mac" } else { 'C:\gdal-build-linux' } }
$Work = Fwd $Work
$d = "$Work/deps"
$out = "$Work/out"
$gb = "$Work/gdal-build"
$cores = [Environment]::ProcessorCount
$prefixMap = "-ffile-prefix-map=$Work/="
Use-Cmake
$ninja = Find-Ninja
New-Item -ItemType Directory -Force $d | Out-Null

if ($Platform -eq 'Linux') {
  $tc = Fwd $Toolchain
  $bin = "$tc/bin"
  foreach ($t in 'clang.exe','clang++.exe','ld.lld.exe','llvm-ar.exe') { if (-not (Test-Path "$bin/$t")) { throw "missing $t in $bin" } }
  $hostSqlite = "$WinEnv\Library\bin\sqlite3.exe"
  if (-not (Test-Path $hostSqlite)) { throw "host sqlite3.exe not found: $hostSqlite" }
  if (-not $ProjVersion) {
    $projMeta = Get-ChildItem "$WinEnv\conda-meta" -Filter 'proj-*.json' | Where-Object Name -match '^proj-\d' | Select-Object -First 1
    if (-not $projMeta) { throw 'no proj package in conda env' }
    $ProjVersion = ($projMeta.BaseName -split '-')[1]
  }
  $link = "-fuse-ld=lld -nodefaultlibs $tc/lib64/libc++.a $tc/lib64/libc++abi.a -lpthread -lm -ldl -lc -lgcc_s -lgcc"
  # toolchain file; libsharpyuv (built with libwebp) is linked in once it exists
  $tcFile = "$Work/ue-linux-toolchain.cmake"
  @"
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_PLATFORM_NO_VERSIONED_SONAME ON)
set(CMAKE_SYSROOT "$tc")
set(CMAKE_C_COMPILER "$tc/bin/clang.exe")
set(CMAKE_CXX_COMPILER "$tc/bin/clang++.exe")
set(CMAKE_AR "$tc/bin/llvm-ar.exe")
set(CMAKE_RANLIB "$tc/bin/llvm-ranlib.exe")
set(CMAKE_C_COMPILER_TARGET x86_64-unknown-linux-gnu)
set(CMAKE_CXX_COMPILER_TARGET x86_64-unknown-linux-gnu)
set(CMAKE_C_FLAGS_INIT "-fPIC $prefixMap")
set(CMAKE_CXX_FLAGS_INIT "-fPIC -stdlib=libc++ $prefixMap")
set(CMAKE_EXE_LINKER_FLAGS_INIT "$link")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "$link -Wl,--exclude-libs,ALL")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "$link")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH "$d")
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
if(EXISTS "$d/lib/libsharpyuv.a")
  string(APPEND CMAKE_SHARED_LINKER_FLAGS_INIT " $d/lib/libsharpyuv.a")
endif()
"@ | Set-Content $tcFile -Encoding ASCII
  $CMakeBase = @("-DCMAKE_MAKE_PROGRAM=$ninja","-DCMAKE_TOOLCHAIN_FILE=$tcFile")
  $cc = "$bin/clang.exe"
  $ar = "$bin/llvm-ar.exe"
  $ccFlags = @('--target=x86_64-unknown-linux-gnu',"--sysroot=$tc",$prefixMap)
  $gdalPlat = @('-D_TEST_LINK_STDCPP=TRUE')
  $lib = 'libgdal.so'
}
else {
  $hostSqlite = (Get-Command sqlite3).Source
  if (-not $ProjVersion) { $ProjVersion = (Resolve-LatestTag 'OSGeo/PROJ').TrimStart('v') }
  $CMakeBase = @("-DCMAKE_MAKE_PROGRAM=$ninja","-DCMAKE_OSX_ARCHITECTURES=$MacArch","-DCMAKE_OSX_DEPLOYMENT_TARGET=$MacTarget","-DCMAKE_PREFIX_PATH=$d","-DCMAKE_C_FLAGS=$prefixMap","-DCMAKE_CXX_FLAGS=$prefixMap")
  $cc = 'clang'
  $ar = 'ar'
  $ccFlags = @('-arch',$MacArch,"-mmacosx-version-min=$MacTarget",$prefixMap)
  $gdalPlat = @("-DCMAKE_SHARED_LINKER_FLAGS=$d/lib/libsharpyuv.a")
  $lib = 'libgdal.dylib'
}
Write-Host "$Platform build: PROJ $ProjVersion, GDAL $Version"

Build-UnixDeps
Build-CMake (Fetch $Work 'gdal' $Version "https://download.osgeo.org/gdal/$Version/gdal-$Version.tar.gz") $gb ($gdalPlat + (Get-UnixGdalArgs))

# ---- collect: bin/<lib>, include, share/gdal, share/proj
Remove-Item $out, "$Work/install" -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force "$out/bin", "$out/share" | Out-Null
Run 'cmake' @('--install',$gb,'--prefix',"$Work/install")
Copy-Item "$Work/install/include" "$out/include" -Recurse
Copy-Item "$Work/install/share/gdal" "$out/share/gdal" -Recurse
Copy-Item "$d/share/proj" "$out/share/proj" -Recurse
if ($Platform -eq 'Linux') {
  Copy-Item "$gb/$lib" "$out/bin" -Force
  $readelf = "$bin/x86_64-unknown-linux-gnu-readelf.exe"
  'NEEDED: ' + ((& $readelf -d "$out/bin/$lib" | Select-String NEEDED | ForEach-Object { $_.Line.Trim() }) -join ' ')
  'glibc: ' + ((& $readelf --dyn-syms "$out/bin/$lib" | Select-String -Pattern 'GLIBC\w*_[\d.]+' -AllMatches | ForEach-Object { $_.Matches.Value } | Sort-Object -Unique) -join ' ')
}
else {
  Run 'cp' @('-L',"$gb/$lib","$out/bin/$lib")   # follows the versioned symlink
  Run 'install_name_tool' @('-id',"@rpath/$lib","$out/bin/$lib")
  Run 'codesign' @('--force','-s','-',"$out/bin/$lib")   # ad-hoc signature, required on arm64
  $deps = @(& otool -L "$out/bin/$lib" | Select-Object -Skip 1 | ForEach-Object { $_.Trim() -replace '\s+\(.*$', '' })
  'DEPENDS: ' + ($deps -join ' ')
  $bad = @($deps | Where-Object { $_ -notmatch '^(/usr/lib/|/System/|@rpath/libgdal\.dylib$)' })
  if ($bad) { throw "non-system dependencies: $($bad -join ', ')" }
  (& lipo -info "$out/bin/$lib")
}
"built GDAL $Version, PROJ $ProjVersion ($Platform) -> $out ; next: Scripts\UpgradeGDAL.ps1"