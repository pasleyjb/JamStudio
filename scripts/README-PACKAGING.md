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

1. Install Visual Studio 2022 C++ tools and CMake.
2. Install [vcpkg](https://vcpkg.io/) and FFmpeg:

   ```powershell
   git clone https://github.com/microsoft/vcpkg C:\vcpkg
   C:\vcpkg\bootstrap-vcpkg.bat
   C:\vcpkg\vcpkg install ffmpeg[avcodec,avformat,avutil,swresample]:x64-windows
   $env:VCPKG_ROOT = "C:\vcpkg"
   ```

3. Package:

   ```powershell
   .\scripts\package-windows.ps1
   ```

Output:

- `dist/JamStudio-<version>-win64.zip` (portable)
- `dist/JamStudio-win64\` folder with exe + FFmpeg DLLs

Optional installer (Inno Setup):

1. Install [Inno Setup](https://jrsoftware.org/isinfo.php)
2. Open `scripts/windows/JamStudio.iss` and Build  
   → `dist/JamStudio-Setup-<version>.exe`

## GitHub Actions

Workflow: `.github/workflows/package.yml`

- **Manual:** Actions → Package → Run workflow  
- **Tag:** push `v0.9.6` (or any `v*`) to build and create a GitHub Release

## Notes

- AppImage bundles FFmpeg shared libraries for stage video.
- Windows zip ships FFmpeg DLLs next to `JamStudio.exe`.
- AI tools (Demucs/Whisper) remain optional external installs.
- Multi-out mixer buses need a multi-channel interface (FOH 1–2, Mon A 3–4, Mon B 5–6).
