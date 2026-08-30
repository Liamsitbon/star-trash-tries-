#!/usr/bin/env python3
"""Package and validate the Quest-standalone Cinema release."""

from __future__ import annotations

import hashlib
import json
import stat
import struct
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "release" / "Cinema-Quest-0.2.0.qmod"
FIXED_TIME = (2026, 8, 29, 0, 0, 0)
FILES = {
    "mod.json": ROOT / "mod.json",
    "libCinema.so": ROOT / "build" / "libCinema.so",
    "cinemaassets.android": ROOT / "assets" / "cinemaassets.android",
    "LICENSE": ROOT / "LICENSE",
    "THIRD_PARTY_NOTICES.md": ROOT / "THIRD_PARTY_NOTICES.md",
    "QuestModInterop-MIT.txt": ROOT / "LICENSES" / "QuestModInterop-MIT.txt",
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 * 1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def validate() -> dict[str, object]:
    missing = [str(path) for path in FILES.values() if not path.is_file()]
    if missing:
        raise SystemExit("Missing required release files:\n  " + "\n  ".join(missing))

    library = FILES["libCinema.so"].read_bytes()[:64]
    if (library[:4] != b"\x7fELF" or library[4] != 2 or
            struct.unpack("<H", library[18:20])[0] != 183):
        raise SystemExit("build/libCinema.so is not ELF64 AArch64")

    bundle = FILES["cinemaassets.android"]
    if (bundle.stat().st_size < 1024 or
            not bundle.read_bytes()[:16].startswith((b"UnityFS", b"UnityRaw", b"UnityWeb"))):
        raise SystemExit("cinemaassets.android is not a Unity AssetBundle")

    manifest = json.loads(FILES["mod.json"].read_text(encoding="utf-8"))
    if (manifest.get("id") != "cinema" or
            manifest.get("version") != "0.2.0" or
            manifest.get("packageVersion") != "1.40.8_7379"):
        raise SystemExit("Cinema manifest identity/version target is invalid")
    return manifest


def main() -> int:
    manifest = validate()
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(OUTPUT, "w", compression=zipfile.ZIP_DEFLATED,
                         compresslevel=9) as archive:
        for name, path in FILES.items():
            info = zipfile.ZipInfo(name, FIXED_TIME)
            info.create_system = 3
            info.external_attr = (stat.S_IFREG |
                                  (0o755 if name.endswith(".so") else 0o644)) << 16
            info.compress_type = (zipfile.ZIP_STORED if
                                  name.endswith((".so", ".android")) else
                                  zipfile.ZIP_DEFLATED)
            archive.writestr(info, path.read_bytes())

    with zipfile.ZipFile(OUTPUT) as archive:
        if archive.testzip() is not None:
            raise SystemExit("Cinema QMOD ZIP integrity failed")
        names = [name.lower() for name in archive.namelist()]
        forbidden = (".dll", ".exe", "ffmpeg", "__macosx", "/._")
        if any(any(token in name for token in forbidden) for name in names):
            raise SystemExit("Cinema QMOD contains a PC/macOS-only payload")

    report = {
        "file": OUTPUT.name,
        "version": manifest["version"],
        "bytes": OUTPUT.stat().st_size,
        "sha256": sha256(OUTPUT),
        "runtimeSha256": sha256(FILES["libCinema.so"]),
        "assetBundleSha256": sha256(FILES["cinemaassets.android"]),
        "target": manifest["packageVersion"],
        "architecture": "ELF64-AArch64",
        "pcPayloads": False,
        "runtimeProof": "source-build-and-package-only",
    }
    report_path = OUTPUT.with_suffix(".report.json")
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
