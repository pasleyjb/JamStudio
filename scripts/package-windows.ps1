# Build Release JamStudio for Windows and stage a portable zip + optional installer sources.
# Run from a Visual Studio Developer PowerShell (or any shell with cmake + MSVC + vcpkg).
#
# Prerequisites:
#   - Visual Studio 2022 Build Tools (C++)
#   - CMake 3.22+
#   - vcpkg with ffmpeg:  vcpkg install ffmpeg:x64-windows
#
# Usage:
#   $env:VCPKG_ROOT = "C:\path\to\vcpkg"
#   .\scripts\package-windows.ps1

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $Root

$Version = (Select-String -Path "CMakeLists.txt" -Pattern 'project\(JamStudio VERSION ([0-9.]+)').Matches.Groups[1].Value
if (-not $Version) { $Version = "0.0.0" }

$BuildDir = Join-Path $Root "build-release-win"
$DistDir  = Join-Path $Root "dist"
$StageDir = Join-Path $DistDir "JamStudio-win64"
New-Item -ItemType Directory -Force -Path $DistDir, $StageDir | Out-Null

Write-Host "==> JamStudio $Version Windows x64 package"

if (-not $env:VCPKG_ROOT) {
    Write-Error "Set VCPKG_ROOT to your vcpkg checkout (with ffmpeg:x64-windows installed)."
}

$Toolchain = Join-Path $env:VCPKG_ROOT "scripts\buildsystems\vcpkg.cmake"
if (-not (Test-Path $Toolchain)) {
    Write-Error "vcpkg toolchain not found: $Toolchain"
}

# Prefer Ninja+MSVC when available (GitHub windows-latest); fall back to VS generators.
$Generator = $null
$IsMultiConfig = $false
if (Get-Command ninja -ErrorAction SilentlyContinue) {
    $Generator = "Ninja"
    $IsMultiConfig = $false
} else {
    foreach ($g in @("Visual Studio 18 2026", "Visual Studio 17 2022", "Visual Studio 16 2019")) {
        $probe = Join-Path $env:TEMP "jamstudio-cmake-probe"
        Remove-Item -Recurse -Force $probe -ErrorAction SilentlyContinue
        $null = & cmake -S $Root -B $probe -G $g -A x64 2>&1
        if ($LASTEXITCODE -eq 0 -or (Test-Path (Join-Path $probe "CMakeCache.txt"))) {
            $Generator = $g
            $IsMultiConfig = $true
            Remove-Item -Recurse -Force $probe -ErrorAction SilentlyContinue
            break
        }
        Remove-Item -Recurse -Force $probe -ErrorAction SilentlyContinue
    }
}

if (-not $Generator) {
    Write-Error "No suitable CMake generator found (need Ninja+MSVC or Visual Studio)."
}

Write-Host "==> Configure (generator: $Generator)"
$ConfigArgs = @(
    "-S", $Root, "-B", $BuildDir,
    "-G", $Generator,
    "-DCMAKE_TOOLCHAIN_FILE=$Toolchain",
    "-DVCPKG_TARGET_TRIPLET=x64-windows"
)
if ($IsMultiConfig) {
    $ConfigArgs += "-A", "x64"
} else {
    $ConfigArgs += "-DCMAKE_BUILD_TYPE=Release"
}
& cmake @ConfigArgs
if ($LASTEXITCODE -ne 0) { Write-Error "CMake configure failed" }

Write-Host "==> Build"
if ($IsMultiConfig) {
    cmake --build $BuildDir --config Release --parallel
} else {
    cmake --build $BuildDir --parallel
}
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed" }
$Bin = $null
$Candidates = @(
    (Join-Path $BuildDir "JamStudio_artefacts\Release\JamStudio.exe"),
    (Join-Path $BuildDir "Release\JamStudio.exe"),
    (Join-Path $BuildDir "JamStudio_artefacts\JamStudio.exe")
)
foreach ($c in $Candidates) {
    if (Test-Path $c) { $Bin = $c; break }
}
if (-not $Bin) {
    Get-ChildItem -Recurse $BuildDir -Filter JamStudio.exe -ErrorAction SilentlyContinue | Select-Object -First 10 FullName
    Write-Error "JamStudio.exe not found"
}

Write-Host "==> Stage portable folder: $StageDir"
Remove-Item -Recurse -Force $StageDir -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $StageDir | Out-Null
Copy-Item $Bin (Join-Path $StageDir "JamStudio.exe")

# Copy FFmpeg and other runtime DLLs from vcpkg installed bin
$VcpkgBin = Join-Path $env:VCPKG_ROOT "installed\x64-windows\bin"
if (Test-Path $VcpkgBin) {
    $DllPatterns = @("avcodec*.dll","avformat*.dll","avutil*.dll","swresample*.dll","swscale*.dll",
                     "libx264*.dll","libx265*.dll","aom*.dll","vpx*.dll","opus*.dll","mp3lame*.dll",
                     "zlib*.dll","libssl*.dll","libcrypto*.dll","brotli*.dll","iconv*.dll")
    foreach ($pat in $DllPatterns) {
        Get-ChildItem $VcpkgBin -Filter $pat -ErrorAction SilentlyContinue | ForEach-Object {
            Copy-Item $_.FullName $StageDir -Force
        }
    }
}

# Also copy any DLLs sitting next to the built exe (vcpkg app-local deploy)
$BinDir = Split-Path $Bin
Get-ChildItem $BinDir -Filter *.dll -ErrorAction SilentlyContinue | ForEach-Object {
    Copy-Item $_.FullName $StageDir -Force
}

# Licenses / readme
@"
JamStudio $Version (Windows x64 portable)

1. Run JamStudio.exe
2. FFmpeg DLLs in this folder enable stage video (MP4 etc.)
3. Optional: install Audacity for external recording with plugins
4. Multi-out audio interfaces: first 6 channels = FOH / Mon A / Mon B

See Help -> Instructions inside the app.
"@ | Set-Content (Join-Path $StageDir "README.txt")

$ZipPath = Join-Path $DistDir "JamStudio-${Version}-win64.zip"
if (Test-Path $ZipPath) { Remove-Item $ZipPath -Force }
Compress-Archive -Path (Join-Path $StageDir "*") -DestinationPath $ZipPath -Force

Get-FileHash $ZipPath -Algorithm SHA256 | ForEach-Object {
    "$($_.Hash.ToLower())  $(Split-Path $ZipPath -Leaf)" | Set-Content ($ZipPath + ".sha256")
}

Write-Host ""
Write-Host "Done."
Write-Host "  Portable: $ZipPath"
Write-Host "  Folder:   $StageDir"
Write-Host ""
Write-Host "Optional installer: install Inno Setup and compile scripts/windows/JamStudio.iss"
