# Build Release JamStudio for Windows and stage a portable zip + optional installer.
# Run from a Visual Studio Developer PowerShell (or any shell with cmake + MSVC + vcpkg).
#
# Prerequisites:
#   - Visual Studio 2022/2026 Build Tools (C++)
#   - CMake 3.22+
#   - vcpkg with ffmpeg:  vcpkg install ffmpeg:x64-windows
#   - Optional: Inno Setup 6 (for Setup.exe)
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

function Find-MsvcCrtDirectory {
    $candidates = @()

    if ($env:VCToolsRedistDir -and (Test-Path $env:VCToolsRedistDir)) {
        $candidates += Get-ChildItem -Path $env:VCToolsRedistDir -Recurse -Directory -Filter "Microsoft.VC*.CRT" -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match '[\\/]x64[\\/]' }
    }

    if ($env:VCINSTALLDIR -and (Test-Path $env:VCINSTALLDIR)) {
        $redist = Join-Path $env:VCINSTALLDIR "Redist\MSVC"
        if (Test-Path $redist) {
            $candidates += Get-ChildItem -Path $redist -Recurse -Directory -Filter "Microsoft.VC*.CRT" -ErrorAction SilentlyContinue |
                Where-Object { $_.FullName -match '[\\/]x64[\\/]' }
        }
    }

    foreach ($root in @(
            "${env:ProgramFiles}\Microsoft Visual Studio",
            "${env:ProgramFiles(x86)}\Microsoft Visual Studio"
        )) {
        if (-not (Test-Path $root)) { continue }
        $candidates += Get-ChildItem -Path $root -Recurse -Directory -Filter "Microsoft.VC*.CRT" -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match '[\\/]x64[\\/]' }
    }

    $hit = $candidates | Sort-Object FullName -Descending | Select-Object -First 1
    if ($hit) { return $hit.FullName }
    return $null
}

function Copy-MsvcCrtDlls {
    param([Parameter(Mandatory = $true)][string]$Dest)

    $crtDir = Find-MsvcCrtDirectory
    if (-not $crtDir) {
        Write-Error @"
MSVC C/C++ runtime redistributable folder not found.
JamStudio.exe requires VCRUNTIME140.dll / MSVCP140.dll on target PCs.
Open a VS Developer shell, or install the 'Desktop development with C++' workload,
then re-run this script. Alternatively install VC++ Redistributable x64 on the target machine:
https://aka.ms/vs/17/release/vc_redist.x64.exe
"@
    }

    Write-Host "==> Copying MSVC CRT from $crtDir"
    $patterns = @("msvcp140*.dll", "vcruntime140*.dll", "concrt140.dll", "vccorlib140.dll")
    $copied = @()
    foreach ($pat in $patterns) {
        Get-ChildItem $crtDir -Filter $pat -ErrorAction SilentlyContinue | ForEach-Object {
            Copy-Item $_.FullName $Dest -Force
            $copied += $_.Name
            Write-Host "    $($_.Name)"
        }
    }

    foreach ($need in @("vcruntime140.dll", "vcruntime140_1.dll", "msvcp140.dll")) {
        if (-not (Test-Path (Join-Path $Dest $need))) {
            Write-Error "Required CRT DLL missing after copy: $need (from $crtDir). Copied: $($copied -join ', ')"
        }
    }
}

function Find-Iscc {
    $cmd = Get-Command iscc -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    foreach ($p in @(
            "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
            "${env:ProgramFiles}\Inno Setup 6\ISCC.exe",
            "${env:LocalAppData}\Programs\Inno Setup 6\ISCC.exe"
        )) {
        if (Test-Path $p) { return $p }
    }
    return $null
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

# MSVC runtime — required on clean Windows PCs without VS / VC++ Redistributable
Copy-MsvcCrtDlls -Dest $StageDir

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

1. Unzip this folder anywhere and run JamStudio.exe
2. Keep all DLLs in the same folder as JamStudio.exe
3. FFmpeg DLLs enable stage video (MP4 etc.)
4. MSVC runtime DLLs (vcruntime140 / msvcp140) are included so a separate
   Visual C++ Redistributable install is usually not required
5. Optional: install Audacity for external recording with plugins
6. Multi-out audio interfaces: first 6 channels = FOH / Mon A / Mon B

See Help -> Instructions inside the app.
"@ | Set-Content (Join-Path $StageDir "README.txt")

# Zip contains a single top-level folder (cleaner extract UX)
$ZipPath = Join-Path $DistDir "JamStudio-${Version}-win64.zip"
if (Test-Path $ZipPath) { Remove-Item $ZipPath -Force }
Compress-Archive -Path $StageDir -DestinationPath $ZipPath -Force

Get-FileHash $ZipPath -Algorithm SHA256 | ForEach-Object {
    "$($_.Hash.ToLower())  $(Split-Path $ZipPath -Leaf)" | Set-Content ($ZipPath + ".sha256")
}

$SetupPath = $null
$Iscc = Find-Iscc
if ($Iscc) {
    Write-Host "==> Building Inno Setup installer with $Iscc"
    $Iss = Join-Path $Root "scripts\windows\JamStudio.iss"
    & $Iscc "/DMyAppVersion=$Version" $Iss
    if ($LASTEXITCODE -ne 0) { Write-Error "Inno Setup compile failed" }
    $SetupPath = Join-Path $DistDir "JamStudio-Setup-$Version.exe"
    if (-not (Test-Path $SetupPath)) {
        # Fallback: pick newest Setup exe if version string differs slightly
        $SetupPath = Get-ChildItem $DistDir -Filter "JamStudio-Setup-*.exe" |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1 -ExpandProperty FullName
    }
    if ($SetupPath -and (Test-Path $SetupPath)) {
        Get-FileHash $SetupPath -Algorithm SHA256 | ForEach-Object {
            "$($_.Hash.ToLower())  $(Split-Path $SetupPath -Leaf)" | Set-Content ($SetupPath + ".sha256")
        }
    }
} else {
    Write-Host "Inno Setup (ISCC) not found — skipping Setup.exe (portable zip only)."
}

Write-Host ""
Write-Host "Done."
Write-Host "  Portable: $ZipPath"
Write-Host "  Folder:   $StageDir"
if ($SetupPath -and (Test-Path $SetupPath)) {
    Write-Host "  Setup:    $SetupPath"
}
Write-Host ""
Write-Host "Staged files:"
Get-ChildItem $StageDir | ForEach-Object { Write-Host ("  {0,12}  {1}" -f $_.Length, $_.Name) }
