<#
.SYNOPSIS
  Builds OpenTyrian2000 (Korean fork) for Windows x64 and packages a portable zip.

.DESCRIPTION
  - Uses SDL2 from -SDL2Dir (default: the "SDL2" folder next to the repository).
    If it is missing, the pinned SDL2 VC development package is downloaded from
    the official GitHub release and verified by SHA-256.
  - Finds MSBuild with vswhere and builds Release|x64 without networking.
  - Assembles dist\OpenTyrian2000-KR\ (exe, SDL2.dll, docs) and zips it.

  The Tyrian 2000 data files are not included: their redistribution terms are
  unclear, so the game offers to download them on first run instead.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File windows\build.ps1
#>
param(
  [string]$SDL2Dir = "",
  [string]$PlatformToolset = "",
  [string]$Version = "",
  [switch]$NoZip
)

$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
if ($SDL2Dir -eq "") { $SDL2Dir = Join-Path (Split-Path -Parent $repo) "SDL2" }

$sdlVersion = "2.32.10"
$sdlZipName = "SDL2-devel-$sdlVersion-VC.zip"
$sdlUrl = "https://github.com/libsdl-org/SDL/releases/download/release-$sdlVersion/$sdlZipName"
$sdlSha256 = "af347939395a58b365846aaea27391e69f9ec9d4dd650d6ac40802159b418a6e"

function Install-SDL2 {
  if (Test-Path (Join-Path $SDL2Dir "include\SDL.h")) { return }

  Write-Host "SDL2 not found at $SDL2Dir; downloading $sdlZipName"
  $tmp = Join-Path ([IO.Path]::GetTempPath()) ("sdl2-" + [Guid]::NewGuid())
  New-Item -ItemType Directory -Path $tmp | Out-Null
  try {
    $zip = Join-Path $tmp $sdlZipName
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -Uri $sdlUrl -OutFile $zip -UseBasicParsing
    $hash = (Get-FileHash -Algorithm SHA256 $zip).Hash.ToLowerInvariant()
    if ($hash -ne $sdlSha256) { throw "SHA-256 mismatch for $sdlZipName ($hash)" }
    Expand-Archive -Path $zip -DestinationPath $tmp
    Move-Item (Join-Path $tmp "SDL2-$sdlVersion") $SDL2Dir
  }
  finally {
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
  }
}

function Find-MSBuild {
  $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
  if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found; install Visual Studio with the C++ workload." }
  $path = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
  if (-not $path) { throw "MSBuild not found; install Visual Studio with the C++ workload." }
  return $path
}

function Get-BuildVersion {
  $version = "v2.1-kor.$(Get-Date -Format yyyyMMdd)"
  # The commit is optional: Git may be missing, or the source may come from a zip.
  try { $sha = (& git -C $repo rev-parse --short HEAD 2>$null) } catch { $sha = $null }
  if (-not $sha) { return $version }
  $sha = "$sha".Trim()
  if (& git -C $repo status --porcelain) { $sha += "-dirty" }
  return "$version.$sha"
}

Install-SDL2
$msbuild = Find-MSBuild
if ($Version -eq "") { $Version = Get-BuildVersion }

Write-Host "Building OpenTyrian2000 $Version"
$msbuildArgs = @(
  (Join-Path $repo "visualc\opentyrian.sln"),
  "/m", "/nologo", "/v:minimal",
  "/p:Configuration=Release",
  "/p:Platform=x64",
  "/p:WITH_NETWORK=false",
  "/p:SDL2BaseDir=$SDL2Dir",
  "/p:OPENTYRIAN_VERSION=$Version"
)
if ($PlatformToolset -ne "") { $msbuildArgs += "/p:PlatformToolset=$PlatformToolset" }
& $msbuild @msbuildArgs
if ($LASTEXITCODE -ne 0) { throw "MSBuild failed with exit code $LASTEXITCODE" }

$dist = Join-Path $repo "dist"
$pkgName = "OpenTyrian2000-KR"
$pkg = Join-Path $dist $pkgName

if (Test-Path $pkg) { Remove-Item -Recurse -Force $pkg }
New-Item -ItemType Directory -Force -Path $pkg | Out-Null

Copy-Item (Join-Path $repo "opentyrian-x64-Release.exe") (Join-Path $pkg "OpenTyrian2000.exe")
Copy-Item (Join-Path $SDL2Dir "lib\x64\SDL2.dll") $pkg
Copy-Item (Join-Path $PSScriptRoot "README-KO.txt") $pkg
Copy-Item (Join-Path $repo "COPYING") (Join-Path $pkg "COPYING.txt")
Copy-Item (Join-Path $SDL2Dir "LICENSE.txt") (Join-Path $pkg "SDL2-LICENSE.txt")

# Korean fonts (Galmuri, SIL Open Font License) and their license
$kor = Join-Path $pkg "kor"
New-Item -ItemType Directory -Force -Path $kor | Out-Null
Copy-Item (Join-Path $repo "kor\*.ttf") $kor
Copy-Item (Join-Path $repo "kor\Galmuri-OFL.md") $kor

# Translations: only the translated entries, without the English text of the
# game data (the game needs just msgctxt and msgstr; see src/translate.h).
function Export-Translations([string]$src, [string]$dst) {
  $utf8 = New-Object System.Text.UTF8Encoding($false)
  New-Item -ItemType Directory -Force -Path $dst | Out-Null
  $total = 0
  foreach ($file in Get-ChildItem (Join-Path $src "*.po")) {
    $out = New-Object System.Collections.Generic.List[string]
    $out.Add('msgid ""'); $out.Add('msgstr ""'); $out.Add('"Content-Type: text/plain; charset=UTF-8\n"'); $out.Add('')
    $text = [IO.File]::ReadAllText($file.FullName, $utf8) -replace "`r", ""
    foreach ($block in ($text -split "`n`n+")) {
      $lines = $block -split "`n"
      if ($lines -match '^#, .*fuzzy') { continue }
      $key = $null; $str = New-Object System.Collections.Generic.List[string]; $field = ""
      foreach ($line in $lines) {
        if ($line -match '^msgctxt "(.*)"$') { $key = $matches[1]; $field = "ctxt" }
        elseif ($line.StartsWith("msgid ")) { $field = "id" }
        elseif ($line.StartsWith("msgstr ")) { $field = "str"; $str.Add($line) }
        elseif ($line.StartsWith('"') -and $field -eq "str") { $str.Add($line) }
      }
      if ($key -eq $null -or ($str -join "") -eq 'msgstr ""') { continue }
      $out.Add("msgctxt `"$key`""); $out.Add("msgid `"$key`""); $out.AddRange($str); $out.Add('')
      $total++
    }
    if ($out.Count -gt 4) { [IO.File]::WriteAllText((Join-Path $dst $file.Name), ($out -join "`n"), $utf8) }
  }
  return $total
}
$translated = Export-Translations (Join-Path $repo "kor\lang") (Join-Path $kor "lang")
Write-Host "Translations packaged: $translated"

Write-Host "Package folder: $pkg"

if (-not $NoZip) {
  $zip = Join-Path $dist "$pkgName-$Version-win64.zip"
  if (Test-Path $zip) { Remove-Item -Force $zip }
  Compress-Archive -Path $pkg -DestinationPath $zip
  Write-Host "Package zip: $zip"
}
