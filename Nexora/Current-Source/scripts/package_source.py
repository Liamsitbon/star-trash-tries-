#!/usr/bin/env python3
"""Package Nexora source plus the locally built ARM64 core without build noise."""

from __future__ import annotations

import hashlib
import os
import stat
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "release" / "Nexora-0.2.5-source-and-arm64-core.zip"
PREFIX = "Nexora-0.2.5-source-and-arm64-core"
FIXED_TIME = (2026, 8, 20, 0, 0, 0)
ROOT_FILES = {
    "CMakeLists.txt", "extern.cmake", "qpm_defines.cmake", "qpm.json",
    "qpm.shared.json", "mod.json", "ndkpath.txt", "README.md", "LICENSE",
    "THIRD_PARTY_NOTICES.md",
}
ROOT_DIRS = {"include", "src", "scripts", "tests", "docs", "examples", "assets", "unity"}
SKIP_PARTS = {"Library", "Temp", "Logs", "obj", "UserSettings", "__pycache__"}
SKIP_NAMES = {".DS_Store", "packages-lock.json"}
SKIP_SUFFIXES = {".pyc", ".log"}


def wanted(path: Path) -> bool:
    relative = path.relative_to(ROOT)
    if relative.parts[0] in ROOT_DIRS:
        if any(part in SKIP_PARTS for part in relative.parts):
            return False
        if path.name in SKIP_NAMES or path.name.startswith("._"):
            return False
        if path.suffix.casefold() in SKIP_SUFFIXES:
            return False
        return path.is_file()
    return len(relative.parts) == 1 and relative.name in ROOT_FILES


def add(archive: zipfile.ZipFile, path: Path, name: str) -> None:
    info = zipfile.ZipInfo(f"{PREFIX}/{name}", FIXED_TIME)
    info.create_system = 3
    executable = path.suffix in {".sh", ".py"} or os.access(path, os.X_OK)
    info.external_attr = (stat.S_IFREG | (0o755 if executable else 0o644)) << 16
    info.compress_type = zipfile.ZIP_STORED if path.suffix in {".so", ".android"} else zipfile.ZIP_DEFLATED
    archive.writestr(info, path.read_bytes())


def main() -> int:
    runtime = ROOT / "build" / "libNexora.so"
    debug = ROOT / "build" / "debug" / "libNexora.so"
    if not runtime.is_file() or not debug.is_file():
        raise SystemExit("Run scripts/build.sh before packaging source")
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(OUTPUT, "w", compresslevel=9, allowZip64=True) as archive:
        for path in sorted(ROOT.rglob("*"), key=lambda value: value.as_posix().casefold()):
            if wanted(path):
                add(archive, path, path.relative_to(ROOT).as_posix())
        add(archive, runtime, "prebuilt/arm64-v8a/libNexora.so")
        add(archive, debug, "prebuilt/arm64-v8a/debug/libNexora.so")
    with zipfile.ZipFile(OUTPUT) as archive:
        if archive.testzip() is not None:
            raise SystemExit("source ZIP integrity check failed")
        if any("__MACOSX" in name or "/._" in name for name in archive.namelist()):
            raise SystemExit("source ZIP contains macOS metadata")
    digest = hashlib.sha256(OUTPUT.read_bytes()).hexdigest()
    print(f"{digest}  {OUTPUT.name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
