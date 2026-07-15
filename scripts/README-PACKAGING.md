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

## GitHub Actions

Workflow: `.github/workflows/package.yml`

- **Manual:** Actions → Package → Run workflow  
- **Tag:** push `v0.9.6` (or any `v*`) to build and create a GitHub Release

CI builds both the portable zip and the Inno Setup installer, and verifies CRT DLLs are present.

## Notes

- AppImage bundles FFmpeg shared libraries for stage video.
- Windows packages ship FFmpeg DLLs **and** MSVC CRT DLLs next to `JamStudio.exe`.
- AI tools (Demucs/Whisper) remain optional external installs.
- Multi-out mixer buses need a multi-channel interface (FOH 1–2, Mon A 3–4, Mon B 5–6).
