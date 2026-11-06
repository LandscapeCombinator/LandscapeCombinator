param(
  [string]$Version = '',
  [string]$Work = 'C:\gdal-build-linux',
  [string]$Toolchain = 'C:\UnrealToolchains\v26_clang-20.1.8-rockylinux8\x86_64-unknown-linux-gnu',
  [string]$WinEnv = 'C:\gdal-build\env',
  [string]$SqliteYear = '2025',
  [string]$SqliteNum = '3500400'
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
. (Join-Path $PSScriptRoot 'GDALBuildCommon.ps1')
$Version = Resolve-GdalVersion $Version

$bin = "$Toolchain\bin"
foreach ($t in 'clang.exe','clang++.exe','ld.lld.exe','llvm-ar.exe') { if (-not (Test-Path "$bin\$t")) { throw "missing $t in $bin" } }
$hostSqlite = "$WinEnv\Library\bin\sqlite3.exe"
if (-not (Test-Path $hostSqlite)) { throw "host sqlite3.exe not found: $hostSqlite" }
$projMeta = Get-ChildItem "$WinEnv\conda-meta" -Filter 'proj-*.json' | Where-Object Name -match '^proj-\d' | Select-Object -First 1
if (-not $projMeta) { throw 'no proj package in conda env' }
$projVersion = ($projMeta.BaseName -split '-')[1]
Write-Host "PROJ $projVersion, GDAL $Version, toolchain $Toolchain"

$ninja = (Get-Command ninja -ErrorAction SilentlyContinue | Select-Object -First 1).Source
if (-not $ninja) { $ninja = (Get-ChildItem "${env:ProgramFiles}\Microsoft Visual Studio" -Recurse -Filter ninja.exe -ErrorAction SilentlyContinue | Select-Object -First 1).FullName }
if (-not $ninja) { throw 'ninja not found' }
Use-Cmake

$out = "$Work\out"
$gb = "$Work\gdal-build"
$d = Fwd "$Work\deps"
$tc = Fwd $Toolchain
$cores = [Environment]::ProcessorCount
New-Item -ItemType Directory -Force "$Work\deps", $out | Out-Null
$prefixMap = "-ffile-prefix-map=$(Fwd $Work)/="
$link = "-fuse-ld=lld -nodefaultlibs $tc/lib64/libc++.a $tc/lib64/libc++abi.a -lpthread -lm -ldl -lc -lgcc_s -lgcc"

# toolchain file; libsharpyuv (built with libwebp) is linked in once it exists
$tcFile = "$Work\ue-linux-toolchain.cmake"
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

function Build-CMake($src, $b, [string[]]$extra, [switch]$Install) {
  Run 'cmake' (@('-S',(Fwd $src),'-B',(Fwd $b),'-G','Ninja',"-DCMAKE_MAKE_PROGRAM=$(Fwd $ninja)","-DCMAKE_TOOLCHAIN_FILE=$(Fwd $tcFile)",'-DCMAKE_BUILD_TYPE=Release') + $extra)
  Run 'cmake' @('--build',$b,'--parallel',$cores)
  if ($Install) { Run 'cmake' @('--install',$b) }
}
function DepArgs($n, $file) { "-D${n}_LIBRARY=$d/lib/$file"; "-D${n}_INCLUDE_DIR=$d/include" }
function Build-Dep($name, $ver, $url, $file, [string[]]$extra) {
  if (Test-Path "$d/lib/$file") { return }
  Build-CMake (Fetch $Work $name $ver $url) "$Work\$name-build" (@("-DCMAKE_INSTALL_PREFIX=$d",'-DBUILD_SHARED_LIBS=OFF') + $extra) -Install
}

# ---- SQLite (static, from amalgamation)
$sqDir = "$Work\sqlite-amalgamation-$SqliteNum"
$sqLib = "$d/lib/libsqlite3.a"
if (-not (Test-Path $sqLib)) {
  if (-not (Test-Path $sqDir)) {
    Get-File "https://www.sqlite.org/$SqliteYear/sqlite-amalgamation-$SqliteNum.zip" "$sqDir.zip"
    Expand-Archive "$sqDir.zip" -DestinationPath $Work -Force
  }
  New-Item -ItemType Directory -Force "$d/lib", "$d/include" | Out-Null
  Run "$bin\clang.exe" @('--target=x86_64-unknown-linux-gnu',"--sysroot=$Toolchain",'-fPIC','-O2','-DSQLITE_THREADSAFE=1','-DSQLITE_ENABLE_COLUMN_METADATA','-DSQLITE_ENABLE_RTREE','-DSQLITE_OMIT_LOAD_EXTENSION',$prefixMap,'-c',(Fwd "$sqDir\sqlite3.c"),'-o',"$sqDir\sqlite3.o")
  Run "$bin\llvm-ar.exe" @('rcs',$sqLib,"$sqDir\sqlite3.o")
  Copy-Item "$sqDir\sqlite3.h", "$sqDir\sqlite3ext.h" "$d/include"
}

# ---- PROJ, Expat (MIT), libwebp + sharpyuv (BSD-3): all static
$expVer = '2.7.1'
$wpVer = '1.5.0'
$expatOff = 'TOOLS','EXAMPLES','TESTS','DOCS','FUZZERS' | ForEach-Object { "-DEXPAT_BUILD_$($_)=OFF" }
$webpOff = 'ANIM_UTILS','CWEBP','DWEBP','GIF2WEBP','IMG2WEBP','VWEBP','WEBPINFO','WEBPMUX','EXTRAS','LIBWEBPMUX' | ForEach-Object { "-DWEBP_BUILD_$($_)=OFF" }
Build-Dep 'proj' $projVersion "https://download.osgeo.org/proj/proj-$projVersion.tar.gz" 'libproj.a' (@(
  '-DCMAKE_POSITION_INDEPENDENT_CODE=ON','-DBUILD_APPS=OFF','-DBUILD_TESTING=OFF','-DENABLE_TIFF=OFF','-DENABLE_CURL=OFF',
  "-DEXE_SQLITE3=$(Fwd $hostSqlite)") + (DepArgs 'SQLITE3' 'libsqlite3.a') + (DepArgs 'SQLite3' 'libsqlite3.a'))
Build-Dep 'expat' $expVer "https://github.com/libexpat/libexpat/releases/download/R_$($expVer.Replace('.','_'))/expat-$expVer.tar.gz" 'libexpat.a' (@('-DEXPAT_SHARED_LIBS=OFF') + $expatOff)
Build-Dep 'libwebp' $wpVer "https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-$wpVer.tar.gz" 'libwebp.a' $webpOff
if (-not (Test-Path "$d/lib/libsharpyuv.a")) { throw 'libsharpyuv.a missing' }

# ---- GDAL
$off = 'GEOS','SPATIALITE' | ForEach-Object { "-DGDAL_USE_$($_)=OFF" }
$noBindings = 'PYTHON','JAVA','CSHARP' | ForEach-Object { "-DBUILD_$($_)_BINDINGS=OFF" }
Build-CMake (Fetch $Work 'gdal' $Version "https://download.osgeo.org/gdal/$Version/gdal-$Version.tar.gz") $gb (@(
  '-DBUILD_SHARED_LIBS=ON','-DCMAKE_POSITION_INDEPENDENT_CODE=ON','-D_TEST_LINK_STDCPP=TRUE',
  '-DGDAL_USE_INTERNAL_LIBS=ON','-DGDAL_USE_EXTERNAL_LIBS=OFF','-DEXPAT_USE_STATIC_LIBS=ON',
  '-DGDAL_USE_EXPAT=ON','-DGDAL_USE_WEBP=ON','-DGDAL_USE_SQLITE3=ON',
  '-DACCEPT_MISSING_SQLITE3_RTREE=ON','-DACCEPT_MISSING_SQLITE3_MUTEX_ALLOC=ON','-DBUILD_APPS=OFF','-DBUILD_TESTING=OFF') +
  $off + $noBindings + (DepArgs 'EXPAT' 'libexpat.a') + (DepArgs 'WEBP' 'libwebp.a') + (DepArgs 'PROJ' 'libproj.a') + (DepArgs 'SQLite3' 'libsqlite3.a'))

# ---- collect + verify
Copy-Item "$gb\libgdal.so" $out -Force
$readelf = "$bin\x86_64-unknown-linux-gnu-readelf.exe"
'NEEDED: ' + ((& $readelf -d "$out\libgdal.so" | Select-String NEEDED | ForEach-Object { $_.Line.Trim() }) -join ' ')
'glibc: ' + ((& $readelf --dyn-syms "$out\libgdal.so" | Select-String -Pattern 'GLIBC\w*_[\d.]+' -AllMatches | ForEach-Object { $_.Matches.Value } | Sort-Object -Unique) -join ' ')
"built GDAL $Version, PROJ $projVersion -> $out ; next: Scripts\UpgradeGDAL.ps1"
