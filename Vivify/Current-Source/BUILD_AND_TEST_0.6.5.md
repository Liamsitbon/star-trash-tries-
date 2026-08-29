# Vivify Quest 0.6.5 + Noodle Extensions 1.8.5 build record

Build date: 2026-08-13

Target: Beat Saber `1.40.8_7379`, Scotland2, Android API 24, ARM64/AArch64.

## Toolchain

Both final clean builds used the QPM-RS Android NDK
`27.3.13750724+preview-0` and Clang 18.0.4. CMake emitted only its existing
minimum-version deprecation warnings; both link steps exited successfully.

## Vivify validation

The following completed successfully:

- lifecycle state-machine host test;
- Quest performance-policy host test under ASan/UBSan;
- Quest performance-capture analyzer test;
- metadata version consistency for all 0.6.5 sources;
- all 11 Windows-master custom-event identifiers;
- active-note refresh/no-global-scan guard;
- separate left/right XR matrix and XR-managed culling guard;
- authored render-setting preservation through pause;
- menu/application/focus pause freeze-before-suspend ordering guard;
- removal of Vivify's own MetaCore score-submission block;
- negative-time opening prewarm and gameplay renderer-order guards;
- QMOD manifest target/payload validation;
- clean ARM64 build and ZIP integrity test.

Artifacts:

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `build/libVivify.so` | 2,269,048 | `3ea9599ac8c90ae137aae468db077595c877f6ee26372522d7758335fc55fd3d` |
| `build/debug/libVivify.so` | 26,601,816 | `9fc32a927129d219e2d07c47d29d2ba04f768dbf71b57986547423980ffbb7f1` |
| `Vivify-Quest-0.6.5.qmod` | 748,764 | `223062011bc8bc9f0b3d34697816af9d6c5dbb23fd4ffc369b84c592cda951f2` |

The `libVivify.so` extracted from the QMOD has the same SHA-256 as the clean
build payload. `file` identifies it as a stripped 64-bit ARM AArch64 ELF shared
object. The archive contains only `mod.json` and the declared
`libVivify.so` payload, and `unzip -t` reports no errors.

## Noodle Extensions validation

The final clean Noodle build compiled all 59 translation units. Its validator
passed metadata, loader entry-point, dependency and script checks, plus new
regression guards for:

- all five V3 fake arrays entering the early parser path;
- `NE_fake` marking before deserialization;
- insertion into the normal typed save-data lists;
- per-item custom-data normalization and the idempotent
  `NE_fakeObjectsInjected` duplicate guard;
- `NoScore` and uninteractable semantics;
- the corrected fake-obstacle type guard.

Artifacts in the paired Noodle source checkout:

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `build/libnoodleextensions.so` | 2,807,728 | `f7d8de0e9d2f966cc165a4bb8b59d75252671424344a9e45ffbb63498fa560d8` |
| `build/debug/libnoodleextensions.so` | 28,893,352 | `618a39fcf45b53aa931a5c28b7247e3b75e0a02b572f5677a02f8afde639da25` |
| `NoodleExtensions.qmod` | 5,065,717 | `ba699f5856a59264c7b13caf02fb113c270047cc48f9aaccb95b4203fd2d27fe` |

The Noodle payload extracted from its QMOD has the same SHA-256 as the clean
build payload. It is also a stripped 64-bit ARM AArch64 ELF shared object. The
archive contains `mod.json`, `cover.png` and the declared
`libnoodleextensions.so`; `unzip -t` reports no errors.

## Hardware boundary

`adb devices -l` returned no attached device at final validation time.
Therefore this record does not claim that Murder Plot, 42 Flux, pause/Continue,
Quest Home/sleep, per-eye output or BeatLeader/ScoreSaber upload was confirmed
on a headset. Use `STABILITY_TESTS_0.6.5.md` with the exact hashes above before
calling those runtime outcomes verified.
