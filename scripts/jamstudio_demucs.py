"""Launch the Demucs copy shipped inside a JamStudio package.

The Python runtime and the htdemucs_6s weights live next to this file.
Weights are copied once into the user data folder so a read-only install
can still separate stems without a network connection.
"""
import os
import shutil
import stat
import sys
from pathlib import Path


def user_cache() -> Path:
    if sys.platform == "darwin":
        return Path.home() / "Library" / "JamStudio" / "demucs-cache"
    if sys.platform == "win32":
        base = os.environ.get("LOCALAPPDATA")
        root = Path(base) if base else Path.home() / "AppData" / "Local"
        return root / "JamStudio" / "demucs-cache"
    data = os.environ.get("XDG_DATA_HOME")
    root = Path(data) if data else Path.home() / ".local" / "share"
    return root / "jamstudio" / "demucs-cache"


def make_writable(path: Path) -> None:
    for dirpath, dirnames, filenames in os.walk(path):
        os.chmod(dirpath, stat.S_IRWXU)
        for name in dirnames:
            os.chmod(Path(dirpath) / name, stat.S_IRWXU)
        for name in filenames:
            os.chmod(Path(dirpath) / name, stat.S_IRUSR | stat.S_IWUSR)


def ensure_cache(bundled: Path, dest: Path) -> None:
    ready = dest / "huggingface" / ".jamstudio-ready"
    source = bundled / "huggingface"
    if not source.is_dir() or ready.exists():
        return

    target = dest / "huggingface"
    if target.exists():
        make_writable(target)
        shutil.rmtree(target)
    dest.mkdir(parents=True, exist_ok=True)
    shutil.copytree(source, target, symlinks=False)
    make_writable(target)

    torch_src = bundled / "torch"
    torch_dst = dest / "torch"
    if torch_src.is_dir() and not torch_dst.exists():
        shutil.copytree(torch_src, torch_dst, symlinks=False)
        make_writable(torch_dst)

    ready.parent.mkdir(parents=True, exist_ok=True)
    ready.write_text("ok\n", encoding="utf-8")


def main() -> None:
    root = Path(__file__).resolve().parent
    dest = user_cache()
    ensure_cache(root / "cache", dest)
    os.environ["HF_HOME"] = str(dest / "huggingface")
    os.environ["HF_HUB_CACHE"] = str(dest / "huggingface" / "hub")
    os.environ["TORCH_HOME"] = str(dest / "torch")
    os.environ["HF_HUB_OFFLINE"] = "1"
    os.environ["HF_HUB_DISABLE_TELEMETRY"] = "1"
    os.environ["TRANSFORMERS_OFFLINE"] = "1"
    os.environ["PYTHONNOUSERSITE"] = "1"

    from demucs.separate import main as demucs_main

    sys.argv = ["demucs", *sys.argv[1:]]
    demucs_main()


if __name__ == "__main__":
    main()
