#!/usr/bin/env bash
# Build a Release JamStudio AppImage for x86_64 Linux.
# Usage: ./scripts/package-linux-appimage.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

VERSION="$(grep -E 'project\(JamStudio VERSION' CMakeLists.txt | sed -E 's/.*VERSION ([0-9.]+).*/\1/')"
ARCH="$(uname -m)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build-release}"
DIST_DIR="${DIST_DIR:-$ROOT/dist}"
APPDIR="$DIST_DIR/JamStudio.AppDir"
TOOLS_DIR="$DIST_DIR/tools"

echo "==> JamStudio $VERSION AppImage ($ARCH)"
mkdir -p "$DIST_DIR" "$TOOLS_DIR"

# --- Dependencies for packaging ---
if ! command -v cmake >/dev/null; then
  echo "cmake is required" >&2
  exit 1
fi

if ! pkg-config --exists libavformat libavcodec libavutil libswresample; then
  echo "FFmpeg dev packages required (libavformat-dev libavcodec-dev libavutil-dev libswresample-dev)" >&2
  exit 1
fi

# --- Download linuxdeploy / appimagetool if missing ---
LINUXDEPLOY="$TOOLS_DIR/linuxdeploy-${ARCH}.AppImage"
APPIMAGETOOL="$TOOLS_DIR/appimagetool-${ARCH}.AppImage"

if [[ ! -x "$LINUXDEPLOY" ]]; then
  echo "==> Fetching linuxdeploy"
  curl -fsSL -o "$LINUXDEPLOY" \
    "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${ARCH}.AppImage"
  chmod +x "$LINUXDEPLOY"
fi

if [[ ! -x "$APPIMAGETOOL" ]]; then
  echo "==> Fetching appimagetool"
  curl -fsSL -o "$APPIMAGETOOL" \
    "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-${ARCH}.AppImage"
  chmod +x "$APPIMAGETOOL"
fi

# Extract tools if FUSE unavailable (common in CI/containers)
run_appimage() {
  local tool="$1"; shift
  if "$tool" --appimage-help >/dev/null 2>&1; then
    "$tool" "$@"
  else
    local extract_dir
    extract_dir="$(mktemp -d)"
    pushd "$extract_dir" >/dev/null
    "$tool" --appimage-extract >/dev/null
    popd >/dev/null
    "$extract_dir/squashfs-root/AppRun" "$@"
    rm -rf "$extract_dir"
  fi
}

# --- Release build ---
echo "==> Configuring Release build in $BUILD_DIR"
cmake -S "$ROOT" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr

echo "==> Building"
cmake --build "$BUILD_DIR" -j"$(nproc)"

# Locate binary (JUCE Debug/Release layout)
BIN=""
for cand in \
  "$BUILD_DIR/JamStudio_artefacts/Release/JamStudio" \
  "$BUILD_DIR/JamStudio_artefacts/Debug/JamStudio" \
  "$BUILD_DIR/JamStudio_artefacts/JamStudio"
do
  if [[ -x "$cand" ]]; then
    BIN="$cand"
    break
  fi
done

if [[ -z "$BIN" ]]; then
  echo "Could not find JamStudio binary under $BUILD_DIR/JamStudio_artefacts" >&2
  find "$BUILD_DIR/JamStudio_artefacts" -type f -name 'JamStudio*' 2>/dev/null | head -20
  exit 1
fi

echo "==> Using binary: $BIN"
strip --strip-unneeded "$BIN" 2>/dev/null || true

# --- Assemble AppDir ---
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/share/applications" \
         "$APPDIR/usr/share/icons/hicolor/256x256/apps" \
         "$APPDIR/usr/share/icons/hicolor/48x48/apps" \
         "$APPDIR/usr/share/metainfo"

cp "$BIN" "$APPDIR/usr/bin/JamStudio"
chmod +x "$APPDIR/usr/bin/JamStudio"

cp "$ROOT/resources/jamstudio.desktop" "$APPDIR/usr/share/applications/jamstudio.desktop"
cp "$ROOT/resources/JamStudioIcon256.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/jamstudio.png"
cp "$ROOT/resources/JamStudioIcon48.png" "$APPDIR/usr/share/icons/hicolor/48x48/apps/jamstudio.png"
# AppImage top-level icon
cp "$ROOT/resources/JamStudioIcon256.png" "$APPDIR/jamstudio.png"
cp "$ROOT/resources/jamstudio.desktop" "$APPDIR/jamstudio.desktop"

# Fix desktop Exec for AppImage
sed -i 's|^Exec=.*|Exec=JamStudio|' "$APPDIR/jamstudio.desktop"
sed -i 's|^Icon=.*|Icon=jamstudio|' "$APPDIR/jamstudio.desktop"

# AppStream metainfo (optional but nice)
cat > "$APPDIR/usr/share/metainfo/com.jamstudio.app.appdata.xml" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<component type="desktop-application">
  <id>com.jamstudio.app</id>
  <name>JamStudio</name>
  <summary>Guitar practice workstation with stems, mixer, and stage video</summary>
  <metadata_license>MIT</metadata_license>
  <project_license>LicenseRef-proprietary</project_license>
  <description>
    <p>JamStudio is a guitar practice and performance workstation with stem mixing,
    multi-bus monitors, stage video, set lists, and external recording handoff.</p>
  </description>
  <launchable type="desktop-id">jamstudio.desktop</launchable>
  <releases>
    <release version="${VERSION}" date="$(date -I)"/>
  </releases>
</component>
EOF

echo "==> Bundling dependencies with linuxdeploy"
export NO_STRIP=1
run_appimage "$LINUXDEPLOY" \
  --appdir "$APPDIR" \
  --executable "$APPDIR/usr/bin/JamStudio" \
  --desktop-file "$APPDIR/jamstudio.desktop" \
  --icon-file "$APPDIR/jamstudio.png"

# Ensure core FFmpeg libs are present (linuxdeploy usually pulls them)
for lib in libavformat libavcodec libavutil libswresample; do
  if ! ls "$APPDIR/usr/lib/${lib}"*.so* >/dev/null 2>&1; then
    echo "Warning: ${lib} not found in AppDir — copying from system"
    for so in $(ldconfig -p | awk '/'"$lib"'\.so/ {print $NF}' | head -5); do
      cp -a "$so" "$APPDIR/usr/lib/" 2>/dev/null || true
      # Also copy soname links
    done
  fi
done

OUTPUT="$DIST_DIR/JamStudio-${VERSION}-${ARCH}.AppImage"
echo "==> Creating $OUTPUT"
export ARCH
run_appimage "$APPIMAGETOOL" "$APPDIR" "$OUTPUT"
chmod +x "$OUTPUT"

# Checksums
( cd "$DIST_DIR" && sha256sum "$(basename "$OUTPUT")" > "$(basename "$OUTPUT").sha256" )

echo ""
echo "Done."
echo "  AppImage: $OUTPUT"
echo "  SHA256:   ${OUTPUT}.sha256"
ls -lh "$OUTPUT"
