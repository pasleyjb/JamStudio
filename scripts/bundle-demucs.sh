#!/usr/bin/env bash
# Build a relocatable Demucs runtime (Python + packages + htdemucs_6s weights).
# Usage: scripts/bundle-demucs.sh [destination]
# The destination is copied into the Mac app, the Linux AppImage, and the Windows folder.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="${1:-$ROOT/dist/demucs-runtime}"
mkdir -p "$DEST"
DEST="$(cd "$DEST" && pwd)"

python_bin() {
  if [[ -x "$DEST/python/bin/python3" ]]; then
    echo "$DEST/python/bin/python3"
  elif [[ -x "$DEST/python/python.exe" ]]; then
    echo "$DEST/python/python.exe"
  else
    echo ""
  fi
}

target_triple() {
  local os arch
  os="$(uname -s)"
  arch="$(uname -m)"
  case "$os" in
    Darwin)
      case "$arch" in
        arm64) echo "aarch64-apple-darwin" ;;
        x86_64) echo "x86_64-apple-darwin" ;;
        *) echo "unsupported arch $arch" >&2; return 1 ;;
      esac
      ;;
    Linux)
      case "$arch" in
        x86_64) echo "x86_64-unknown-linux-gnu" ;;
        aarch64|arm64) echo "aarch64-unknown-linux-gnu" ;;
        *) echo "unsupported arch $arch" >&2; return 1 ;;
      esac
      ;;
    MINGW*|MSYS*|CYGWIN*)
      echo "x86_64-pc-windows-msvc"
      ;;
    *)
      echo "unsupported OS $os" >&2
      return 1
      ;;
  esac
}

download_python() {
  local triple url
  triple="$(target_triple)"
  echo "==> Downloading standalone Python 3.12 for $triple"
  local api="https://api.github.com/repos/astral-sh/python-build-standalone/releases/latest"
  local body
  body="$(curl -fsSL "$api")"
  local version
  for version in "3.12" "3.11" "3.13"; do
    url="$(printf '%s\n' "$body" | grep -o "https://[^\"]*cpython-${version}[^\"]*${triple}-install_only\\.tar\\.gz" | grep -v freethreaded | head -1 || true)"
    if [[ -n "$url" ]]; then
      break
    fi
  done
  if [[ -z "${url:-}" ]]; then
    echo "No python-build-standalone asset for $triple" >&2
    exit 1
  fi
  echo "    $url"
  local tarball="$DEST/python.tar.gz"
  curl -fL --retry 3 -o "$tarball" "$url"
  rm -rf "$DEST/python"
  tar -xzf "$tarball" -C "$DEST"
  rm -f "$tarball"
}

PY="$(python_bin)"
if [[ -z "$PY" ]]; then
  download_python
  PY="$(python_bin)"
fi

if [[ -z "$PY" ]]; then
  echo "Standalone Python was not extracted into $DEST/python" >&2
  exit 1
fi

echo "==> Using $PY"
if ! "$PY" -c "import demucs, torch, torchcodec" >/dev/null 2>&1; then
  echo "==> Installing Demucs, PyTorch, and torchcodec"
  "$PY" -m pip install --upgrade pip
  if [[ "$(uname -s)" == "Linux" ]]; then
    # PyPI's default Linux torch wheel is the CUDA build. That makes the
    # AppImage about 3 GB, and GitHub rejects release files of 2 GB or more.
    "$PY" -m pip install torch --index-url https://download.pytorch.org/whl/cpu
  fi
  "$PY" -m pip install "demucs" "torchcodec" "soundfile"
  if [[ "$(uname -s)" == "Linux" ]]; then
    "$PY" -m pip install --force-reinstall --no-deps torch \
      --index-url https://download.pytorch.org/whl/cpu
    mapfile -t nvidia_pkgs < <("$PY" -m pip freeze | awk -F= '/^nvidia-/{print $1}')
    if (( ${#nvidia_pkgs[@]} > 0 )); then
      "$PY" -m pip uninstall -y "${nvidia_pkgs[@]}"
    fi
    site="$("$PY" -c 'import site; print(site.getsitepackages()[0])')"
    rm -rf "$site"/nvidia "$site"/nvidia-* "$site"/triton
  fi
fi

if ! "$PY" -c "import demucs, torch, torchcodec" >/dev/null 2>&1; then
  echo "Demucs failed to import from the bundled Python." >&2
  exit 1
fi

if [[ ! -d "$DEST/cache/huggingface/hub" ]]; then
  echo "==> Downloading the htdemucs_6s model into the package"
  mkdir -p "$DEST/cache/huggingface" "$DEST/cache/torch"
  HF_HOME="$DEST/cache/huggingface" \
  HF_HUB_CACHE="$DEST/cache/huggingface/hub" \
  TORCH_HOME="$DEST/cache/torch" \
  HF_HUB_DISABLE_TELEMETRY=1 \
  "$PY" -c "from demucs.pretrained import get_model; get_model('htdemucs_6s'); print('htdemucs_6s ready')"
fi

cp "$ROOT/scripts/jamstudio_demucs.py" "$DEST/jamstudio_demucs.py"

echo "==> Demucs runtime: $DEST"
du -sh "$DEST" "$DEST/python" "$DEST/cache" 2>/dev/null || true
"$PY" "$DEST/jamstudio_demucs.py" --help >/dev/null
echo "==> demucs --help ok"
