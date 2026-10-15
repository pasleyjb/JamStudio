#!/usr/bin/env bash
# Build a Release JamStudio.app and a drag-to-Applications disk image.
# Usage: ./scripts/package-macos.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "package-macos.sh must run on macOS." >&2
  exit 1
fi

VERSION="$(grep -E 'project\(JamStudio VERSION' CMakeLists.txt | sed -E 's/.*VERSION ([0-9.]+).*/\1/')"
ARCH="$(uname -m)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build-macos}"
DIST_DIR="${DIST_DIR:-$ROOT/dist}"
JOBS="$(sysctl -n hw.ncpu)"

echo "==> JamStudio $VERSION macOS ($ARCH)"

fail_toolchain() {
  cat >&2 <<'EOF'
This Mac cannot compile JamStudio yet.

JamStudio needs the current Xcode Command Line Tools (Apple clang and the
macOS SDK). An Intel-only tools install from an older Mac, or a missing
install, will not link a native app on Apple Silicon.

Fix:
  1. sudo rm -rf /Library/Developer/CommandLineTools
  2. xcode-select --install
  3. Install a native toolchain and FFmpeg:
       # Apple Silicon (preferred):
       /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
       eval "$(/opt/homebrew/bin/brew shellenv)"
       brew install cmake ffmpeg ninja pkg-config
     Intel Homebrew, if that is what you already use:
       brew install cmake ffmpeg ninja pkg-config
  4. ./scripts/package-macos.sh

The script writes dist/JamStudio-<version>-mac-<arch>.dmg
EOF
  exit 1
}

if ! xcrun --show-sdk-path >/dev/null 2>&1; then
  fail_toolchain
fi

if ! command -v cmake >/dev/null 2>&1; then
  echo "cmake is required. Install it with: brew install cmake" >&2
  exit 1
fi

if ! command -v pkg-config >/dev/null 2>&1; then
  echo "pkg-config is required. Install it with: brew install pkg-config" >&2
  exit 1
fi

# Homebrew FFmpeg (Apple Silicon /opt/homebrew, Intel /usr/local).
for prefix in /opt/homebrew /usr/local; do
  for rel in opt/ffmpeg/lib/pkgconfig lib/pkgconfig; do
    dir="${prefix}/${rel}"
    if [[ -d "$dir" ]]; then
      export PKG_CONFIG_PATH="${dir}${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
    fi
  done
done

if ! pkg-config --exists libavformat libavcodec libavutil libswresample; then
  echo "FFmpeg development libraries not found. Install them with: brew install ffmpeg" >&2
  exit 1
fi

GENERATOR=()
if command -v ninja >/dev/null 2>&1; then
  GENERATOR=(-G Ninja)
fi

echo "==> Configuring Release build in $BUILD_DIR"
cmake -S "$ROOT" -B "$BUILD_DIR" \
  "${GENERATOR[@]}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="$ARCH"

echo "==> Building"
cmake --build "$BUILD_DIR" --config Release -j"$JOBS"

APP=""
for cand in \
  "$BUILD_DIR/JamStudio_artefacts/Release/JamStudio.app" \
  "$BUILD_DIR/JamStudio_artefacts/RelWithDebInfo/JamStudio.app" \
  "$BUILD_DIR/JamStudio_artefacts/JamStudio.app"
do
  if [[ -d "$cand" ]]; then
    APP="$cand"
    break
  fi
done

if [[ -z "$APP" ]]; then
  echo "Could not find JamStudio.app under $BUILD_DIR/JamStudio_artefacts" >&2
  find "$BUILD_DIR" -name 'JamStudio.app' -type d 2>/dev/null | head -20 >&2 || true
  exit 1
fi

echo "==> Using app: $APP"

# Copy FFmpeg and its non-system dependencies into the bundle and rewrite
# install names so the disk image runs without a Homebrew install.
FRAMEWORKS="$APP/Contents/Frameworks"
EXE="$APP/Contents/MacOS/JamStudio"
mkdir -p "$FRAMEWORKS"

is_system_lib() {
  case "$1" in
    /usr/lib/*|/System/*) return 0 ;;
    *) return 1 ;;
  esac
}

# Canonical path, following symlinks. Works on the macOS bash 3.2 that ships
# with the system (no GNU realpath, and python3 is not guaranteed).
canonicalize() {
  local path="$1"
  local dir target
  while [[ -L "$path" ]]; do
    dir="$(cd "$(dirname "$path")" && pwd)"
    target="$(readlink "$path")"
    case "$target" in
      /*) path="$target" ;;
      *) path="$dir/$target" ;;
    esac
  done
  if [[ ! -e "$path" ]]; then
    return 1
  fi
  dir="$(cd "$(dirname "$path")" && pwd)"
  echo "$dir/$(basename "$path")"
}

resolve_dep() {
  local dep="$1"
  local owner="$2"
  local base candidate dir
  case "$dep" in
    @rpath/*)
      base="${dep#@rpath/}"
      for candidate in \
        "$(dirname "$owner")/$base" \
        "$FRAMEWORKS/$base" \
        "/opt/homebrew/opt/ffmpeg/lib/$base" \
        "/opt/homebrew/lib/$base" \
        "/usr/local/opt/ffmpeg/lib/$base" \
        "/usr/local/lib/$base"
      do
        if [[ -e "$candidate" ]]; then
          canonicalize "$candidate"
          return 0
        fi
      done
      return 1
      ;;
    @loader_path/*)
      dir="$(cd "$(dirname "$owner")" && pwd)"
      canonicalize "$dir/${dep#@loader_path/}"
      ;;
    @executable_path/*)
      dir="$(cd "$(dirname "$EXE")" && pwd)"
      canonicalize "$dir/${dep#@executable_path/}"
      ;;
    /*)
      canonicalize "$dep"
      ;;
    *)
      return 1
      ;;
  esac
}

ensure_rpath() {
  local bin="$1"
  local rpath="$2"
  if ! otool -l "$bin" | awk '/cmd LC_RPATH/{getline; getline; print $2}' | grep -Fxq "$rpath"; then
    install_name_tool -add_rpath "$rpath" "$bin"
  fi
}

bundle_one() {
  local owner="$1"
  local dep="$2"
  is_system_lib "$dep" && return 0

  local resolved
  if ! resolved="$(resolve_dep "$dep" "$owner")"; then
    echo "warning: could not resolve $dep (needed by $(basename "$owner"))" >&2
    return 0
  fi

  is_system_lib "$resolved" && return 0
  [[ -f "$resolved" ]] || return 0

  local base
  base="$(basename "$resolved")"
  local dest="$FRAMEWORKS/$base"

  if [[ ! -f "$dest" ]]; then
    cp -p "$resolved" "$dest"
    chmod u+w "$dest"
    install_name_tool -id "@rpath/$base" "$dest"
    ensure_rpath "$dest" "@loader_path"
    echo "$dest"
  fi

  if [[ "$dep" != "@rpath/$base" ]]; then
    install_name_tool -change "$dep" "@rpath/$base" "$owner" || true
  fi
}

echo "==> Bundling FFmpeg libraries"
ensure_rpath "$EXE" "@executable_path/../Frameworks"

pending=("$EXE")
seen_owners="|"
index=0
while [[ $index -lt ${#pending[@]} ]]; do
  owner="${pending[$index]}"
  index=$((index + 1))
  case "$seen_owners" in
    *"|$owner|"*) continue ;;
  esac
  seen_owners="${seen_owners}${owner}|"

  while IFS= read -r dep; do
    [[ -z "$dep" ]] && continue
    new_lib="$(bundle_one "$owner" "$dep" || true)"
    if [[ -n "$new_lib" ]]; then
      pending+=("$new_lib")
    fi
  done < <(otool -L "$owner" | awk 'NR>1 { print $1 }')
done

echo "==> Signing (ad-hoc, so Apple Silicon will launch the bundle)"
if [[ -d "$FRAMEWORKS" ]]; then
  while IFS= read -r lib; do
    codesign --force --sign - "$lib"
  done < <(find "$FRAMEWORKS" -type f \( -name '*.dylib' -o -name '*.so' \))
fi
codesign --force --sign - "$APP"
codesign --verify --verbose=2 "$APP"

mkdir -p "$DIST_DIR"
STAGE="$DIST_DIR/dmg-root"
rm -rf "$STAGE"
mkdir -p "$STAGE"
ditto "$APP" "$STAGE/JamStudio.app"
ln -s /Applications "$STAGE/Applications"

DMG="$DIST_DIR/JamStudio-${VERSION}-mac-${ARCH}.dmg"
rm -f "$DMG"
hdiutil create -volname "JamStudio" -srcfolder "$STAGE" -ov -format UDZO "$DMG" >/dev/null
shasum -a 256 "$DMG" | awk '{ print $1 "  " $2 }' > "${DMG}.sha256"

echo "==> Wrote $DMG"
cat "${DMG}.sha256"
