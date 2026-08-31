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
MOD_VERSION = "0.3.1"
TARGET = "1.40.8_7379"
UNITY_VERSION = "2021.3.16f1"
OUTPUT = ROOT / "release" / f"Nexora-Quest-{MOD_VERSION}.qmod"
FIXED_TIME = (2026, 8, 20, 0, 0, 0)
FILES = {
    "mod.json": ROOT / "mod.json",
    "libNexora.so": ROOT / "build" / "libNexora.so",
    "nexoraassets.android": ROOT / "assets" / "nexoraassets.android",
    "nexoraassets.android.provenance.json": (
        ROOT / "assets" / "nexoraassets.android.provenance.json"
    ),
    "README.md": ROOT / "README.md",
    "LICENSE": ROOT / "LICENSE",
    "THIRD_PARTY_NOTICES.md": ROOT / "THIRD_PARTY_NOTICES.md",
}
EXPECTED_FILE_COPIES = {
    "nexoraassets.android": (
        "/sdcard/ModData/com.beatgames.beatsaber/Mods/Nexora/Assets/"
        "nexoraassets.android"
    ),
    "nexoraassets.android.provenance.json": (
        "/sdcard/ModData/com.beatgames.beatsaber/Mods/Nexora/Assets/"
        "nexoraassets.android.provenance.json"
    ),
}
EXPECTED_NEEDED = {
    "liblog.so", "libbeatsaber-hook.so", "libcustom-json-data.so",
    "libcustom-types.so", "libpaper2_scotland2.so", "libsl2.so",
    "libsongcore.so", "libm.so", "libdl.so", "libc.so",
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 * 1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def validate_elf(path: Path) -> list[str]:
    library = path.read_bytes()
    if len(library) < 64 or library[:4] != b"\x7fELF":
        raise SystemExit("build/libNexora.so is not an ELF object")
    if library[4] != 2 or library[5] != 1 or library[6] != 1:
        raise SystemExit("build/libNexora.so is not ELF64 little-endian version 1")
    if library[7] not in (0, 3):
        raise SystemExit("build/libNexora.so uses an unexpected ELF OS ABI")
    file_type, machine, version = struct.unpack_from("<HHI", library, 16)
    if file_type != 3 or machine != 183 or version != 1:
        raise SystemExit("build/libNexora.so is not an AArch64 ET_DYN shared object")
    program_offset = struct.unpack_from("<Q", library, 32)[0]
    header_size, program_entry_size, program_count = struct.unpack_from("<HHH", library, 52)
    if (
        header_size != 64
        or program_entry_size != 56
        or program_count == 0
        or program_offset + program_entry_size * program_count > len(library)
    ):
        raise SystemExit("build/libNexora.so has an invalid ELF program-header table")
    load_segments: list[tuple[int, int, int]] = []
    dynamic_offset = 0
    dynamic_size = 0
    for index in range(program_count):
        offset = program_offset + index * program_entry_size
        segment_type, _, file_offset, virtual_address, _, file_size, _, _ = (
            struct.unpack_from("<IIQQQQQQ", library, offset)
        )
        if segment_type == 1:
            load_segments.append((virtual_address, file_offset, file_size))
        elif segment_type == 2:
            dynamic_offset, dynamic_size = file_offset, file_size
    if dynamic_offset == 0 or dynamic_offset + dynamic_size > len(library):
        raise SystemExit("build/libNexora.so has no valid dynamic segment")

    needed_offsets: list[int] = []
    string_table_address = 0
    string_table_size = 0
    soname_offset: int | None = None
    forbidden_path_tag = False
    for offset in range(dynamic_offset, dynamic_offset + dynamic_size, 16):
        tag, value = struct.unpack_from("<QQ", library, offset)
        if tag == 0:
            break
        if tag == 1:
            needed_offsets.append(value)
        elif tag == 5:
            string_table_address = value
        elif tag == 10:
            string_table_size = value
        elif tag == 14:
            soname_offset = value
        elif tag in (15, 29):
            forbidden_path_tag = True
    if forbidden_path_tag:
        raise SystemExit("build/libNexora.so contains an RPATH/RUNPATH")
    string_table_offset = None
    for virtual_address, file_offset, file_size in load_segments:
        if virtual_address <= string_table_address < virtual_address + file_size:
            string_table_offset = file_offset + string_table_address - virtual_address
            break
    if (
        string_table_offset is None
        or string_table_size == 0
        or string_table_offset + string_table_size > len(library)
    ):
        raise SystemExit("build/libNexora.so has an invalid dynamic string table")

    def dynamic_string(relative_offset: int) -> str:
        if relative_offset >= string_table_size:
            raise SystemExit("build/libNexora.so has an invalid dynamic string offset")
        start = string_table_offset + relative_offset
        end = library.find(b"\0", start, string_table_offset + string_table_size)
        if end < 0:
            raise SystemExit("build/libNexora.so has an unterminated dynamic string")
        try:
            return library[start:end].decode("ascii")
        except UnicodeDecodeError as error:
            raise SystemExit("build/libNexora.so has a non-ASCII dynamic library name") from error

    soname = dynamic_string(soname_offset) if soname_offset is not None else ""
    if soname != "libNexora.so":
        raise SystemExit(f"build/libNexora.so has an unexpected SONAME: {soname!r}")
    needed = {dynamic_string(offset) for offset in needed_offsets}
    if needed != EXPECTED_NEEDED:
        raise SystemExit(
            "build/libNexora.so dynamic dependency set changed: "
            f"expected {sorted(EXPECTED_NEEDED)}, got {sorted(needed)}"
        )
    return sorted(needed)


def validate_provenance() -> dict[str, object]:
    provenance_path = FILES["nexoraassets.android.provenance.json"]
    try:
        provenance = json.loads(provenance_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise SystemExit(f"Unity AssetBundle provenance is unreadable: {error}") from error
    expected_keys = {
        "schemaVersion", "unityVersion", "buildTarget", "graphicsApis",
        "shaderSha256", "shaderMetaSha256", "materialSha256",
        "materialMetaSha256", "builderSha256", "bundleSha256",
    }
    if set(provenance) != expected_keys:
        raise SystemExit("Unity AssetBundle provenance schema changed unexpectedly")
    if (
        provenance.get("schemaVersion") != 1
        or provenance.get("unityVersion") != UNITY_VERSION
        or provenance.get("buildTarget") != "Android"
        or provenance.get("graphicsApis") != "Vulkan,OpenGLES3"
    ):
        raise SystemExit("Unity AssetBundle provenance target is invalid")
    source_hashes = {
        "shaderSha256": ROOT / "unity/Assets/Nexora/Shaders/NexoraDome.shader",
        "shaderMetaSha256": ROOT / "unity/Assets/Nexora/Shaders/NexoraDome.shader.meta",
        "materialSha256": ROOT / "unity/Assets/Nexora/Materials/NexoraDome.mat",
        "materialMetaSha256": ROOT / "unity/Assets/Nexora/Materials/NexoraDome.mat.meta",
        "builderSha256": ROOT / "unity/Assets/Nexora/Editor/BuildNexoraAssets.cs",
        "bundleSha256": FILES["nexoraassets.android"],
    }
    for field, source in source_hashes.items():
        if provenance.get(field) != sha256(source):
            raise SystemExit(f"Unity AssetBundle provenance mismatch: {field}")
    return provenance


def validate() -> tuple[dict[str, object], dict[str, object], list[str]]:
    missing = [str(path) for path in FILES.values() if not path.is_file()]
    if missing:
        raise SystemExit("Missing required release files:\n  " + "\n  ".join(missing))

    needed = validate_elf(FILES["libNexora.so"])

    bundle = FILES["nexoraassets.android"]
    header = bundle.read_bytes()[:16]
    if bundle.stat().st_size < 1024 or not header.startswith((b"UnityFS", b"UnityRaw", b"UnityWeb")):
        raise SystemExit("assets/nexoraassets.android is not a real Unity AssetBundle")

    manifest = json.loads(FILES["mod.json"].read_text(encoding="utf-8"))
    if (manifest.get("id") != "nexora" or
            manifest.get("version") != MOD_VERSION or
            manifest.get("packageVersion") != TARGET):
        raise SystemExit("mod.json identity, version, or Beat Saber target is invalid")
    if manifest.get("modFiles") != [] or manifest.get("libraryFiles") != []:
        raise SystemExit("mod.json contains an unexpected early mod or library payload")
    if manifest.get("lateModFiles") != ["libNexora.so"]:
        raise SystemExit("mod.json lateModFiles does not exactly match the Nexora runtime")
    file_copies = {
        item.get("name"): item.get("destination")
        for item in manifest.get("fileCopies", [])
    }
    if file_copies != EXPECTED_FILE_COPIES:
        raise SystemExit("mod.json fileCopies does not exactly match the asset payload")
    return manifest, validate_provenance(), needed


def main() -> int:
    manifest, provenance, needed = validate()
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
        "assetProvenanceSha256": sha256(
            FILES["nexoraassets.android.provenance.json"]
        ),
        "assetUnityVersion": provenance["unityVersion"],
        "assetGraphicsApis": provenance["graphicsApis"],
        "target": manifest["packageVersion"],
        "architecture": "ELF64-AArch64",
        "dynamicLibraries": needed,
        "pcPayloads": False,
        "runtimeProof": "source-build-and-package-only",
    }
    report_path = OUTPUT.with_suffix(".report.json")
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
