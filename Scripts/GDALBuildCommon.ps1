function Run($exe, $argList) {
  & $exe @argList
  if ($LASTEXITCODE -ne 0) { throw "$exe failed with exit code $LASTEXITCODE" }
}

function Resolve-GdalVersion([string]$Version) {
  if ($Version) { return $Version }
  try {
    $tag = Resolve-LatestTag 'OSGeo/gdal'
  } catch { throw "could not resolve latest GDAL version; pass -Version explicitly ($_)" }
  $v = $tag.TrimStart('v')
  Write-Host "latest GDAL: $v"
  $v
}

function Fwd([string]$p) { $p.Replace('\','/') }

function Get-File($url, $dest) {
  if (-not (Test-Path $dest)) { Write-Host "downloading $url"; Invoke-WebRequest $url -OutFile $dest -UseBasicParsing }
}

function Use-Cmake {
  if (Get-Command cmake -ErrorAction SilentlyContinue) { return }
  $c = Get-ChildItem "${env:ProgramFiles}\Microsoft Visual Studio" -Recurse -Filter cmake.exe -ErrorAction SilentlyContinue | Select-Object -First 1
  if (-not $c) { throw 'cmake not found on PATH and no VS-bundled cmake found' }
  $env:PATH = "$($c.DirectoryName);$env:PATH"
}

# DLL closure of gdal.dll in a conda env: Copy = what to ship, Missing = found nowhere (system DLLs are skipped)
function Get-DllClosure([string]$EnvDir) {
  $dumpbin = Get-ChildItem "${env:ProgramFiles}\Microsoft Visual Studio\2022" -Recurse -Filter dumpbin.exe -ErrorAction SilentlyContinue |
    Where-Object FullName -match 'Hostx64\\x64' | Select-Object -First 1 -ExpandProperty FullName
  if (-not $dumpbin) { throw 'dumpbin.exe not found in VS 2022' }
  $bin = Join-Path $EnvDir 'Library\bin'
  $queue = [Collections.Generic.Queue[string]]::new()
  $queue.Enqueue('gdal.dll')
  $seen = @{}; $copy = @(); $missing = @()
  while ($queue.Count) {
    $n = $queue.Dequeue(); $k = $n.ToLower()
    if ($seen[$k]) { continue }
    $seen[$k] = $true
    if ($k -match '^(api-ms-win-|ext-ms-win-|msvcp140|vcruntime140|vcomp140|concrt140)') { continue }
    $p = Join-Path $bin $n
    if (Test-Path $p) {
      $copy += (Get-Item $p).Name
      & $dumpbin /nologo /dependents $p 2>&1 | ForEach-Object { if ("$_" -match '^\s+(\S+\.dll)\s*$') { $queue.Enqueue($Matches[1]) } }
    }
    elseif (-not (Test-Path (Join-Path $env:windir "System32\$n"))) { $missing += $n }
  }
  [pscustomobject]@{ Copy = $copy; Missing = $missing }
}

# Fails if any DLL in the list comes from a (L/A)GPL conda package
function Assert-NoCopyleft([string]$EnvDir, [string[]]$Dlls) {
  $lic = @{}
  foreach ($m in Get-ChildItem (Join-Path $EnvDir 'conda-meta') -Filter *.json) {
    $j = Get-Content $m.FullName -Raw | ConvertFrom-Json
    foreach ($p in $j.files) { if ($p -like 'Library/bin/*.dll') { $lic[$p.Substring(12).ToLower()] = "$($j.name): $($j.license)" } }
  }
  $bad = @($Dlls | Where-Object { $lic[$_.ToLower()] -match 'GPL' } | ForEach-Object { "$_ ($($lic[$_.ToLower()]))" })
  if ($bad) { throw "Copyleft DLLs: $($bad -join ', ')" }
  "license check OK ($($Dlls.Count) DLLs)"
}

# downloads and extracts <name>-<ver>.tar.gz into $Dir, returns the source folder
function Fetch($Dir, $name, $ver, $url) {
  $src = Join-Path $Dir "$name-$ver"
  if (-not (Test-Path $src)) {
    Get-File $url "$src.tar.gz"
    Run 'tar' @('-xzf',"$src.tar.gz",'-C',$Dir) | Out-Null
  }
  $src
}


# ---- shared by the Windows, Linux and Mac builds
function Flags([string]$Fmt, [string[]]$Names) { $Names | ForEach-Object { $Fmt -f $_ } }
$noBindings = Flags '-DBUILD_{0}_BINDINGS=OFF' 'PYTHON','JAVA','CSHARP'

function Resolve-LatestTag([string]$Repo) {
  $h = @{ 'User-Agent' = 'pwsh' }
  if ($env:GITHUB_ACTIONS -and $env:GITHUB_TOKEN) { $h['Authorization'] = "Bearer $env:GITHUB_TOKEN" }
  (Invoke-RestMethod "https://api.github.com/repos/$Repo/releases/latest" -Headers $h).tag_name
}

function Find-Ninja {
  $n = (Get-Command ninja -ErrorAction SilentlyContinue | Select-Object -First 1).Source
  if (-not $n -and $env:ProgramFiles) { $n = (Get-ChildItem "${env:ProgramFiles}\Microsoft Visual Studio" -Recurse -Filter ninja.exe -ErrorAction SilentlyContinue | Select-Object -First 1).FullName }
  if (-not $n) { throw 'ninja not found' }
  Fwd $n
}

# ---- Linux/Mac builds (BuildGDALUnix.ps1): these read $Work $d $cores $CMakeBase (and more) from the calling script
function Build-CMake($src, $b, [string[]]$extra, [switch]$Install) {
  Run 'cmake' (@('-S',(Fwd $src),'-B',(Fwd $b),'-G','Ninja','-DCMAKE_BUILD_TYPE=Release') + $CMakeBase + $extra)
  Run 'cmake' @('--build',$b,'--parallel',$cores)
  if ($Install) { Run 'cmake' @('--install',$b) }
}
function DepArgs($n, $file) { "-D${n}_LIBRARY=$d/lib/$file"; "-D${n}_INCLUDE_DIR=$d/include" }
function Build-Dep($name, $ver, $url, $file, [string[]]$extra) {
  if (Test-Path "$d/lib/$file") { return }
  Build-CMake (Fetch $Work $name $ver $url) "$Work/$name-build" (@("-DCMAKE_INSTALL_PREFIX=$d",'-DBUILD_SHARED_LIBS=OFF') + $extra) -Install
}

# static SQLite from the amalgamation; needs $cc $ccFlags $ar $SqliteYear $SqliteNum
function Build-Sqlite {
  $lib = "$d/lib/libsqlite3.a"
  if (Test-Path $lib) { return }
  $dir = "$Work/sqlite-amalgamation-$SqliteNum"
  if (-not (Test-Path $dir)) {
    Get-File "https://www.sqlite.org/$SqliteYear/sqlite-amalgamation-$SqliteNum.zip" "$dir.zip"
    Expand-Archive "$dir.zip" -DestinationPath $Work -Force
  }
  New-Item -ItemType Directory -Force "$d/lib", "$d/include" | Out-Null
  Run $cc ($ccFlags + @('-fPIC','-O2','-DSQLITE_THREADSAFE=1','-DSQLITE_ENABLE_COLUMN_METADATA','-DSQLITE_ENABLE_RTREE','-DSQLITE_OMIT_LOAD_EXTENSION','-c',"$dir/sqlite3.c",'-o',"$dir/sqlite3.o"))
  Run $ar @('rcs',$lib,"$dir/sqlite3.o")
  Copy-Item "$dir/sqlite3.h", "$dir/sqlite3ext.h" "$d/include"
}

# SQLite, PROJ, Expat (MIT), libwebp + sharpyuv (BSD-3): all static; needs $ProjVersion $hostSqlite
function Build-UnixDeps {
  $expVer = '2.7.1'
  $wpVer = '1.5.0'
  Build-Sqlite
  $expatOff = Flags '-DEXPAT_BUILD_{0}=OFF' 'TOOLS','EXAMPLES','TESTS','DOCS','FUZZERS'
  $webpOff = Flags '-DWEBP_BUILD_{0}=OFF' 'ANIM_UTILS','CWEBP','DWEBP','GIF2WEBP','IMG2WEBP','VWEBP','WEBPINFO','WEBPMUX','EXTRAS','LIBWEBPMUX'
  Build-Dep 'proj' $ProjVersion "https://download.osgeo.org/proj/proj-$ProjVersion.tar.gz" 'libproj.a' (@(
    '-DCMAKE_POSITION_INDEPENDENT_CODE=ON','-DBUILD_APPS=OFF','-DBUILD_TESTING=OFF','-DENABLE_TIFF=OFF','-DENABLE_CURL=OFF',
    "-DEXE_SQLITE3=$(Fwd $hostSqlite)") + (DepArgs 'SQLITE3' 'libsqlite3.a') + (DepArgs 'SQLite3' 'libsqlite3.a'))
  Build-Dep 'expat' $expVer "https://github.com/libexpat/libexpat/releases/download/R_$($expVer.Replace('.','_'))/expat-$expVer.tar.gz" 'libexpat.a' (@('-DEXPAT_SHARED_LIBS=OFF') + $expatOff)
  Build-Dep 'libwebp' $wpVer "https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-$wpVer.tar.gz" 'libwebp.a' $webpOff
  if (-not (Test-Path "$d/lib/libsharpyuv.a")) { throw 'libsharpyuv.a missing' }
}

function Get-UnixGdalArgs {
  $off = Flags '-DGDAL_USE_{0}=OFF' 'GEOS','SPATIALITE'
  @('-DBUILD_SHARED_LIBS=ON','-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
    '-DGDAL_USE_INTERNAL_LIBS=ON','-DGDAL_USE_EXTERNAL_LIBS=OFF','-DEXPAT_USE_STATIC_LIBS=ON',
    '-DGDAL_USE_EXPAT=ON','-DGDAL_USE_WEBP=ON','-DGDAL_USE_SQLITE3=ON',
    '-DACCEPT_MISSING_SQLITE3_RTREE=ON','-DACCEPT_MISSING_SQLITE3_MUTEX_ALLOC=ON','-DBUILD_APPS=OFF','-DBUILD_TESTING=OFF') +
    $off + $noBindings + (DepArgs 'EXPAT' 'libexpat.a') + (DepArgs 'WEBP' 'libwebp.a') + (DepArgs 'PROJ' 'libproj.a') + (DepArgs 'SQLite3' 'libsqlite3.a')
}

# replaces <Dest>\bin, lib, include, share with <Out>\bin, include, share
function Install-UnixTree($Out, $Dest) {
  foreach ($s in 'bin','lib','include','share') { Remove-Item (Join-Path $Dest $s) -Recurse -Force -ErrorAction SilentlyContinue }
  New-Item -ItemType Directory -Force $Dest | Out-Null
  foreach ($s in 'bin','include','share') { Copy-Item (Join-Path $Out $s) (Join-Path $Dest $s) -Recurse }
}