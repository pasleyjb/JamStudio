# JamStudio packaging

## Linux AppImage (this machine / CI)

```bash
# Dependencies (Ubuntu/Debian)
sudo apt-get install -y build-essential cmake pkg-config \
  libasound2-dev libfreetype6-dev libx11-dev libxcomposite-dev \
  libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev \
  libcurl4-openssl-dev libavformat-dev libavcodec-dev libavutil-dev \
  libswresample-dev libfontconfig1-dev

chmod +x scripts/package-linux-appimage.sh
./scripts/package-linux-appimage.sh
```

Output:

- `dist/JamStudio-<version>-x86_64.AppImage`
- `dist/JamStudio-<version>-x86_64.AppImage.sha256`

Run:

```bash
chmod +x dist/JamStudio-*.AppImage
./dist/JamStudio-*.AppImage
```

## Windows portable zip / installer

On a Windows machine (or GitHub Actions `windows-latest`):

1. Install Visual Studio 2022/2026 C++ tools and CMake.
2. Install [vcpkg](https://vcpkg.io/) and FFmpeg:

   ```powershell
   git clone https://github.com/microsoft/vcpkg C:\vcpkg
   C:\vcpkg\bootstrap-vcpkg.bat
   C:\vcpkg\vcpkg install ffmpeg[avcodec,avformat,avutil,swresample]:x64-windows
   $env:VCPKG_ROOT = "C:\vcpkg"
   ```

3. (Optional) Install [Inno Setup 6](https://jrsoftware.org/isinfo.php) for `Setup.exe`.
4. From a **VS Developer PowerShell** (so MSVC CRT redistributables are discoverable):

   ```powershell
   .\scripts\package-windows.ps1
   ```

Output:

- `dist/JamStudio-<version>-win64.zip` — portable folder (exe + FFmpeg + **MSVC CRT** DLLs)
- `dist/JamStudio-win64\` — staged folder used by the zip/installer
- `dist/JamStudio-Setup-<version>.exe` — Inno Setup installer (if ISCC is installed)

### Running on a clean Windows PC

1. Prefer **Setup.exe** if available: run it, then launch from Start Menu / desktop.
2. Or unzip the portable zip and run `JamStudio.exe` **from inside the extracted folder**.
3. Keep every DLL next to the exe (do not move the exe alone).
4. If Windows still reports missing `VCRUNTIME140.dll` / `MSVCP140.dll`, install:
   [VC++ Redistributable x64](https://aka.ms/vs/17/release/vc_redist.x64.exe)

> Note: older `v0.9.6.1` portable zips did **not** bundle the MSVC runtime. Rebuild with this script (or a newer release) for out-of-the-box installs.

## macOS disk image

On a Mac with current Xcode Command Line Tools:

```bash
# Apple Silicon Homebrew is /opt/homebrew. Intel Homebrew is /usr/local.
brew install cmake ffmpeg ninja pkg-config
chmod +x scripts/package-macos.sh
./scripts/package-macos.sh
```

Output:

- `dist/JamStudio-<version>-mac-<arch>.dmg`
- `dist/JamStudio-<version>-mac-<arch>.dmg.sha256`
- `build-macos/JamStudio_artefacts/Release/JamStudio.app` (the app inside the image)

Install:

1. Open the disk image.
2. Drag **JamStudio** onto **Applications**.
3. Launch it from Applications. The first launch asks for microphone access so recording and input monitoring work.
4. Core Audio is the audio backend. Plug-and-play routing prefers a USB interface (Scarlett and similar) for input and the Mac speakers or headphones for monitoring.

The image bundles the FFmpeg libraries the app was linked against, then ad-hoc signs the bundle so Apple Silicon will launch it. It is not notarized. The first time you open it, if Gatekeeper blocks the app, right-click JamStudio in Applications and choose Open.

The disk image, AppImage, and Windows zip include Demucs and the htdemucs_6s weights. Stem separation does not need a separate install.

Whisper and basic-pitch stay optional. On a Mac:

```bash
brew install pipx ffmpeg
pipx ensurepath
pipx install openai-whisper
```

JamStudio, when launched from Finder, adds Homebrew and `~/.local/bin` to `PATH` so those tools are found.

If `xcrun` fails or the Command Line Tools are an old Intel install on an Apple Silicon Mac, remove them and install the current tools before packaging:

```bash
sudo rm -rf /Library/Developer/CommandLineTools
xcode-select --install
```

## GitHub Actions

Workflow: `.github/workflows/package.yml`

- **Manual:** Actions → Package → Run workflow  
- **Tag:** push `v0.10.1` (or any `v*`) to build and create a GitHub Release

CI builds the Linux AppImage, the Windows portable zip and Inno Setup installer, and the macOS disk image. The Windows job verifies CRT DLLs are present.

## Notes

- AppImage bundles FFmpeg shared libraries for stage video.
- Windows packages ship FFmpeg DLLs **and** MSVC CRT DLLs next to `JamStudio.exe`.
- The macOS disk image ships a `JamStudio.app` with FFmpeg libraries inside `Contents/Frameworks`, ad-hoc signed.
- Demucs and the htdemucs_6s model are included in the downloads. Whisper and basic-pitch stay optional external installs.
- Linux sends FOH, Mon A, and Mon B through a PipeWire bus. macOS and Windows list real playback devices for those three mixes.
- The lyric page script is inside each download. On Windows it needs Python on PATH.
