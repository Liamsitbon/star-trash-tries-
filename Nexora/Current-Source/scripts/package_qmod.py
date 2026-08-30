#!/usr/bin/env python3
"""Build a deterministic Nexora QMOD and reject incomplete shader releases."""

from __future__ import annotations

import hashlib
import json
import stat
import struct
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "release" / "Nexora-Quest-0.2.7.qmod"
FIXED_TIME = (2026, 8, 20, 0, 0, 0)
FILES = {
    "mod.json": ROOT / "mod.json",
    "libNexora.so": ROOT / "build" / "libNexora.so",
    "nexoraassets.android": ROOT / "assets" / "nexoraassets.android",
    "README.md": ROOT / "README.md",
    "LICENSE": ROOT / "LICENSE",
    "THIRD_PARTY_NOTICES.md": ROOT / "THIRD_PARTY_NOTICES.md",
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

    library = FILES["libNexora.so"].read_bytes()[:64]
    if library[:4] != b"\x7fELF" or library[4] != 2 or struct.unpack("<H", library[18:20])[0] != 183:
        raise SystemExit("build/libNexora.so is not an ELF64 AArch64 library")

    bundle = FILES["nexoraassets.android"]
    header = bundle.read_bytes()[:16]
    if bundle.stat().st_size < 1024 or not header.startswith((b"UnityFS", b"UnityRaw", b"UnityWeb")):
        raise SystemExit("assets/nexoraassets.android is not a real Unity AssetBundle")

    manifest = json.loads(FILES["mod.json"].read_text(encoding="utf-8"))
    if (manifest.get("id") != "nexora" or
            manifest.get("version") != "0.2.7" or
            manifest.get("packageVersion") != "1.40.8_7379"):
        raise SystemExit("mod.json identity, version, or Beat Saber target is invalid")
    return manifest


def main() -> int:
    manifest = validate()
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(OUTPUT, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, path in FILES.items():
            info = zipfile.ZipInfo(name, FIXED_TIME)
            info.create_system = 3
            info.external_attr = (stat.S_IFREG | (0o755 if name.endswith(".so") else 0o644)) << 16
            info.compress_type = zipfile.ZIP_STORED if name.endswith((".so", ".android")) else zipfile.ZIP_DEFLATED
            archive.writestr(info, path.read_bytes())

    with zipfile.ZipFile(OUTPUT) as archive:
        if archive.testzip() is not None:
            raise SystemExit("QMOD ZIP integrity check failed")
        names = {name.casefold() for name in archive.namelist()}
        expected = {name.casefold() for name in FILES}
        if names != expected:
            raise SystemExit(f"QMOD payload changed: {sorted(names)}")
        forbidden = (".dll", ".exe", ".dylib", ".pdb", ".lib", "ffmpeg", "libav")
        if any(any(token in name for token in forbidden) for name in names):
            raise SystemExit("QMOD contains a PC/macOS codec or binary payload")

    report = {
        "file": OUTPUT.name,
        "version": manifest["version"],
        "bytes": OUTPUT.stat().st_size,
        "sha256": sha256(OUTPUT),
        "runtimeSha256": sha256(FILES["libNexora.so"]),
        "assetBundleSha256": sha256(FILES["nexoraassets.android"]),
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
