# Vivify Quest 0.6.4 local build, package and staging record

Date: 2026-08-12  
Target: Beat Saber `1.40.8_7379`, Scotland2, Android ARM64  
Candidate: Vivify `0.6.4`

## Result

| Layer | Result |
| --- | --- |
| Authentic Axo 0.4.1 source-fix inventory | PASS; no missing authentic runtime fix |
| Aether 0.6.3 bridge result | USER-CONFIRMED PASS on Quest 3S; exact installed hash matched |
| Per-eye matrix source guard | PASS |
| Lifecycle host tests | PASS |
| Quest performance policy ASan/UBSan tests | PASS |
| Quest performance analyzer fixture | PASS |
| Clean RelWithDebInfo Android ARM64 build with LTO | PASS |
| Stripped/unstripped ELF inspection | PASS |
| QMOD content and embedded-library hash | PASS |
| Three-path Quest 3S staging | PASS; all hashes match |
| New-process 0.6.4 runtime load | BLOCKED by Meta's controller-required launch dialog |
| Physical left/right visual acceptance | PENDING |

## Source verification

`scripts/verify-port-source.sh` reported:

```text
PASS lifecycle state-machine test
PASS Quest performance policy tests (ASan/UBSan)
PASS Quest performance capture analyzer test
PASS metadata version consistency (0.6.4)
PASS all 11 Windows-master event identifiers are registered
PASS required lifecycle/parity implementation anchors are present
PASS Axo-style active-note refresh avoids global scans and targets affected tracks
PASS secondary cameras preserve separate left/right XR matrices with XR-managed culling
PASS pause/resume preserves authored render settings across XR suspend
PASS QMOD manifest target and payload declaration
```

The stereo guard verifies both eye enums, both projection assignments, both
view assignments, `ResetCullingMatrix`, and the absence of
`set_cullingMatrix` inside the Quest texture-array branch.

## Build and package

```text
$ pwsh -NoProfile -File scripts/build.ps1 -clean
[15/15] Linking CXX shared library libVivify.so; Preserving debug symbols and stripping the packaged Vivify library

$ pwsh -NoProfile -File scripts/createqmod.ps1 -qmodName Vivify-Quest-0.6.4
Creating qmod from mod.json
Validating mod.json...
```

Artifacts:

```text
build/libVivify.so        2,256,328 bytes  ARM64 ELF, stripped
build/debug/libVivify.so 26,530,848 bytes  ARM64 ELF, debug_info, unstripped
Vivify-Quest-0.6.4.qmod     745,576 bytes
```

Both ELFs have build ID `b01433fe8d8507e7388ecd02d5f20ea18f96ebdd`.
The dynamic section reports `SONAME libVivify.so` and `BIND_NOW`.

The QMOD contains exactly:

```text
mod.json
libVivify.so
```

Its manifest reports Vivify 0.6.4, Beat Saber `1.40.8_7379`, Scotland2 and
`lateModFiles: ["libVivify.so"]`. The library extracted from the QMOD is
byte-identical to `build/libVivify.so`.

SHA-256:

```text
e35e4269c39d8b8e939a502124b0f2005e4c514340c194b05802c847e79a7dee  build/libVivify.so
8c3ca9a61ed70920c6f9e6615b116eca7980b4256a9edec5ceea36ebba4be5a3  build/debug/libVivify.so
3fbd6c9f6f62f06e7b4dfccabcf128d2edee0b5f74f56570e55941a144039dd4  Vivify-Quest-0.6.4.qmod
```

## Quest 3S staging

Connected device: Quest 3S, serial `340YC10G8D07ZH`. Beat Saber reports
version `1.40.8_7379`. The old process was stopped before replacing the active
library. The candidate was staged at all three runtime/package locations:

```text
/sdcard/ModData/com.beatgames.beatsaber/Packages/1.40.8_7379/vivify_v0.6.4/libVivify.so
/sdcard/ModData/com.beatgames.beatsaber/Modloader/mods/libVivify.so
/data/user/0/com.beatgames.beatsaber/files/mods/libVivify.so
```

All three report the release-library SHA-256
`e35e4269c39d8b8e939a502124b0f2005e4c514340c194b05802c847e79a7dee`.
The 0.6.3 package folder was retained for rollback.

An ADB launch was intercepted by
`LaunchCheckControllerRequiredDialogActivity`; no Beat Saber process started
and the runtime log therefore still begins with the previous 0.6.3 session.
Staging is confirmed, but a new-process log and the physical per-eye test are
still required before claiming the visual issue fixed on hardware.
