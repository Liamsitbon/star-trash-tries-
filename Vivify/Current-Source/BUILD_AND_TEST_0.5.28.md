# Vivify Quest 0.5.28 — build and validation record

Date: 2026-08-09 (Asia/Jerusalem)

## Release target

- Mod version: `0.5.28`
- Beat Saber package version: `1.40.8_7379`
- Android target: API 24, `arm64-v8a` / AArch64
- Toolchain: Android NDK `27.3.13750724+preview-0`, Clang 18.0.4
- CMake configuration: `RelWithDebInfo`, `-O3`, LTO, C++20

## Evidence reviewed

- The complete local `codex.txt` instruction file (2,067 lines).
- Flux recordings `676767` and the 2026-08-09 WhatsApp recording, plus log
  `777888` and the current Quest diagnostics.
- Aether recording `436152` and log `456456`.
- Algae PC/Quest comparison recordings `060606` and `090909`, plus log
  `097097`.
- Arrow-visibility recording `123123123123`.
- The current installed 0.5.27 binary and configuration on the connected Quest
  3S.

The captured 0.5.27 native crash at 2026-08-09 01:08:54 resolved
`libVivify.so+0x1661f8` to `Runtime::ShouldBypassImageEffect` at
`src/VivifyPostProcessing.cpp:1224`, called by the global
`ImageEffectController.OnRenderImage` hook. The failing callback happened
after the CameraApplier's Unity object had been destroyed during menu exit.

## Commands completed successfully

```text
pwsh -File scripts/validate-modjson.ps1
pwsh -File scripts/build.ps1
pwsh -File scripts/createqmod.ps1 -qmodName Vivify-Quest-0.5.28
unzip -t Vivify-Quest-0.5.28.qmod
```

The clean build compiled all 14 source/toolchain units and linked the ARM64
library. Final rebuilds after the owned per-eye depth-copy, culling timing,
Aether prefab-assignment index, and live stereo-keyword diagnostics also
passed. The only reported warnings were CMake minimum-version deprecation
warnings from the Android NDK files; no Vivify compiler or linker errors were
reported.

## Artifact checks

- `build/libVivify.so`: stripped ARM64 ELF, 2,176,480 bytes
- `build/debug/libVivify.so`: unstripped ARM64 ELF with debug info, 25,893,064
  bytes
- Both libraries have ELF Build ID
  `9fc2348e13f2fb337864eb453269fc7b1790a9dd`.
- `Vivify-Quest-0.5.28.qmod`: 715,822 bytes
- The QMOD contains exactly `mod.json` and `libVivify.so`.
- The embedded manifest reports `vivify` version `0.5.28` for Beat Saber
  `1.40.8_7379` and passed the project validator and JSON parsing.
- The QMOD library is byte-for-byte identical to `build/libVivify.so`.
- The unstripped symbol table contains
  `SecondaryCameraController::OnRenderImage`; it contains neither the removed
  `ImageEffectController_OnRenderImage` hook nor
  `Runtime::ShouldBypassImageEffect`.
- `qpm.json`, `qpm.shared.json`, `qpm_defines.cmake`, and `mod.json` all report
  version `0.5.28`.

## SHA-256

```text
5c468916fd5dfa2818b48a384c8c4d5cf6089debfb31cb4d35c716209807cf12  build/libVivify.so
bfc8820200a63b70bece45307cf932941e33a42fd72f6ec90d5880ff598b0078  build/debug/libVivify.so
d405808060870353a5f4f21d70acc2f15d517a036a3300e7abd6af78fa5aad3b  Vivify-Quest-0.5.28.qmod
```

## Quest installation and runtime boundary

The connected Quest 3S accepted the earlier 0.5.28 per-eye/lifecycle build at
`/sdcard/ModData/com.beatgames.beatsaber/Modloader/mods/libVivify.so`. The
previous `Vivify.log` was preserved as `Vivify.pre-0.5.28.log` after being
collected in the pre-install diagnostic ZIP. The headset disconnected from ADB
during the final Aether-index rebuild, so the final release hash above has not
yet been pushed to or verified on the device. Install the QMOD (or copy the
final release library) after reconnecting before performing the device
checklist.

Beat Saber could not be started unattended because Horizon OS displayed its
system `Controllers required` launch check while the physical controller was
inactive. The launch was intercepted before the Beat Saber process or mod
loader started, so this is not a 0.5.28 crash. No 0.5.28 gameplay, two-eye,
Flux, pause/restart, menu-exit, frame-pacing, or soak result is marked as
passed. Those checks remain mandatory in `STABILITY_TESTS_0.5.28.md` once a
controller is awake.

The diagnostics collector was syntax-checked and executed against the Quest.
It gathered the mod logs, configs, logcat/crash buffer, system state, hashes,
and performance dumps into a ZIP. Horizon OS can list but blocks direct ADB
reads of CrashReporter tombstones under scoped storage; use the script's
`-bugreport` switch after a crash to add the Android bugreport evidence.
