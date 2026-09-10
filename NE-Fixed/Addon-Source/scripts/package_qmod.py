#!/usr/bin/env python3
"""Package only the independent ARM64 add-on; never redistribute a renamed NE core."""
import hashlib
import os
from pathlib import Path
import shutil
import stat
import struct
import subprocess
import zipfile
from check_contract import ROOT, validate


def elf_dependencies(payload):
    assert len(payload) >= 64 and payload[:7] == b"\x7fELF\x02\x01\x01", "Not ELF64 LE"
    assert struct.unpack_from("<HHI", payload, 16) == (3, 183, 1), "Not AArch64 ET_DYN"
    ndk = os.environ.get("ANDROID_NDK_HOME") or os.environ.get("ANDROID_NDK_LATEST_HOME")
    candidates = sorted(Path(ndk).glob("toolchains/llvm/prebuilt/*/bin/llvm-readelf")) if ndk else []
    readelf = str(candidates[0]) if candidates else (shutil.which("llvm-readelf") or shutil.which("readelf"))
    if not readelf:
        raise SystemExit("Set ANDROID_NDK_HOME to inspect the final ELF dependencies")
    result = subprocess.check_output([readelf, "--dynamic", str(ROOT / "build/libNEFixed.so")], text=True)
    needed = {line.split("[", 1)[1].split("]", 1)[0] for line in result.splitlines() if "(NEEDED)" in line}
    allowed = {"liblog.so", "libbeatsaber-hook.so", "libcustom-json-data.so", "libcustom-types.so",
               "libpaper2_scotland2.so", "libsl2.so", "libbsml.so", "liblapiz.so", "libsongcore.so", "libm.so", "libdl.so", "libc.so"}
    assert needed <= allowed, f"Unexpected dependencies: {needed - allowed}"
    assert {"liblapiz.so", "libsl2.so", "libbsml.so"} <= needed
    assert "libnoodleextensions.so" not in needed
    print("ARM64 dynamic dependencies:", ", ".join(sorted(needed)))


def main():
    manifest = validate()
    runtime = (ROOT / "build/libNEFixed.so").read_bytes()
    elf_dependencies(runtime)
    files = {
        "libNEFixed.so": runtime,
        **{name: (ROOT / name).read_bytes() for name in ("mod.json", "LICENSE", "INSTALL.md", "MIGRATION.md", "THIRD_PARTY_NOTICES.md")},
    }
    output = ROOT / "release" / f"NE-Fixed-Addon-Quest-{manifest['version']}.qmod"
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, data in sorted(files.items()):
            entry = zipfile.ZipInfo(name, (2026, 9, 10, 0, 0, 0))
            entry.create_system = 3
            entry.external_attr = (stat.S_IFREG | 0o644) << 16
            entry.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(entry, data)
    with zipfile.ZipFile(output) as archive:
        assert set(archive.namelist()) == set(files)
        assert archive.testzip() is None
        assert archive.read("libNEFixed.so") == runtime
    digest = hashlib.sha256(output.read_bytes()).hexdigest()
    print(f"{digest}  {output}")


if __name__ == "__main__":
    main()
