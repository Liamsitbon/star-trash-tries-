# Vivify Quest 0.5.27 — build and validation record

Date: 2026-08-09 (Asia/Jerusalem)

## Release target

- Mod version: `0.5.27`
- Beat Saber package version: `1.40.8_7379`
- Android target: API 24, `arm64-v8a` / AArch64
- Toolchain observed during the clean build: Android NDK
  `27.3.13750724+preview-0`, Clang 18.0.4
- CMake configuration: `RelWithDebInfo`, `-O3`, LTO, C++20

## Commands completed successfully

```text
qpm restore
pwsh -File scripts/build.ps1 -clean
pwsh -File scripts/createqmod.ps1 -qmodName Vivify-Quest-0.5.27
```

The clean build compiled all 14 source/toolchain units and linked
`libVivify.so`. The post-build step retained an unstripped diagnostics binary
and stripped the library placed in the QMOD. The only build warnings were
CMake minimum-version deprecation warnings emitted by the Android NDK CMake
files; no Vivify compiler or linker errors were reported.

## Artifact checks

- `build/libVivify.so`: ELF 64-bit ARM AArch64 shared object, stripped,
  2,160,224 bytes
- `build/debug/libVivify.so`: ELF 64-bit ARM AArch64 shared object with debug
  information, 25,296,424 bytes
- Both libraries have ELF Build ID
  `7f721c3f6770497658124b37081dfd56d8fee21f`.
- `Vivify-Quest-0.5.27.qmod`: 706,930 bytes
- The QMOD contains exactly `mod.json` and `libVivify.so`.
- The embedded manifest passed the project's QMOD schema validation, reports
  ID `vivify`, version `0.5.27`, package version `1.40.8_7379`, and installs
  `libVivify.so` through `lateModFiles`.
- The embedded `libVivify.so` is byte-for-byte identical to
  `build/libVivify.so`.
- `qpm.json`, `qpm.shared.json`, and `mod.json` all passed JSON parsing.

## SHA-256

```text
d85b1506dbfb2172740e0b4adf743acdce5ef95fb4f52a7523084789f4c2e50a  build/libVivify.so
f275be45abc6f8b4ab91840f67abafd1cd7463c83d16ae396e39b6d3d7345613  build/debug/libVivify.so
2e8863baf081bb010cda5c352e035874cb52abb03e44bd280c35ab4032ea8e1e  Vivify-Quest-0.5.27.qmod
```

## Validation boundary

No physical Quest was connected in this run. Installation, two-eye visual
parity, frame timing, pause/restart survival, map compatibility, thermal
behavior, and long-session crash resistance are therefore **not** marked as
passed. Complete `STABILITY_TESTS_0.5.27.md` on the intended headset and keep
the resulting log/tombstone evidence before distributing this build broadly.
