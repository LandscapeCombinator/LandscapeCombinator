function Run($exe, $argList) {
  & $exe @argList
  if ($LASTEXITCODE -ne 0) { throw "$exe failed with exit code $LASTEXITCODE" }
}

function Resolve-GdalVersion([string]$Version) {
  if ($Version) { return $Version }
  try {
    $tag = (Invoke-RestMethod 'https://api.github.com/repos/OSGeo/gdal/releases/latest' -Headers @{ 'User-Agent' = 'pwsh' }).tag_name
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
