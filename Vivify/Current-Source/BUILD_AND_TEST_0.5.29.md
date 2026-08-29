# Vivify Quest 0.5.29 — build, package, and installation record

Date: 2026-08-09 (Asia/Jerusalem)

## Release target

- Mod version: `0.5.29`
- Beat Saber package version: `1.40.8_7379`
- Device target: Quest 3S, Android API 24 / AArch64
- Toolchain: Android NDK `27.3.13750724+preview-0`
- CMake configuration: `RelWithDebInfo`, `-O3`, LTO, C++20

## 0.5.28 failure evidence

The supplied `2026-08-09-025033-flux-moon.zip` contains the exact final
0.5.28 binary installed on the Quest. Its installed and packaged hashes were
both:

`5c468916fd5dfa2818b48a384c8c4d5cf6089debfb31cb4d35c716209807cf12`

The run established three independent defects:

1. At 02:49:36 Unity crashed in its Material pointer resolver while executing
   `CommandBuffer.Blit`. The Vivify frames resolve to `Runtime::ApplyBlits`
   from `CameraApplier::OnRenderImage`. The log had already switched to
   `MenuMainCamera`, but gameplay Blits and their destroyed Materials remained
   live.
2. Quest Multiview supplied a 2016x2112, two-slice `Tex2DArray`, while every
   secondary capture was stored as `eye=0`. The map's depth shaders require
   array-compatible `_NotesOne_Depth`/`_Notes_Depth` bindings; zero or mismatched
   depth samples explain the eclipse hiding notes and later culling sections
   losing their composited cubes.
3. The log contained 1,558 culling-pass diagnostics and 588 measured culling
   stalls of at least 3 ms. Main-camera properties were reapplied every Update,
   resetting the controller's renderer cache and its log limits every frame.

The supplied video and local map data show that Murder Plot contains authored
time-offset fake-note sequences and dissolve point definitions, but the visible
horizontal rows are not intended; the copies should remain cut out so one cube
is presented at a time. That symptom crosses into Noodle Extensions
note-animation behavior and is not marked fixed by this Vivify release.

## Commands completed successfully

```text
pwsh -NoProfile -File scripts/validate-modjson.ps1
pwsh -NoProfile -File scripts/build.ps1
pwsh -NoProfile -File scripts/createqmod.ps1 -qmodName Vivify-Quest-0.5.29
unzip -t Vivify-Quest-0.5.29.qmod
```

The final official build compiled all 14 source/toolchain units with
`VERSION="0.5.29"` and linked successfully. The only final-build warnings were
the Android NDK CMake files' minimum-version deprecation notices. A preliminary
incremental link correctly exposed a missing generated `Scene` implementation
include; the include was added before the clean versioned build above.

## Artifact checks

- `build/libVivify.so`: stripped ARM64 ELF, 2,185,712 bytes
- `build/debug/libVivify.so`: ARM64 ELF with debug info, 26,005,608 bytes
- Both libraries have ELF Build ID
  `a34e2b5394a781925517c067ffcd8b15136c6215`.
- `Vivify-Quest-0.5.29.qmod`: 719,843 bytes
- The QMOD contains exactly `mod.json` and `libVivify.so`.
- The embedded manifest reports `vivify` version `0.5.29` for Beat Saber
  `1.40.8_7379`.
- The embedded library is byte-for-byte identical to
  `build/libVivify.so`.
- Runtime strings and the unstripped symbol table contain the new depth-layout,
  MainMenu-reset, secondary-capture, binding, and CameraApplier paths.
- `qpm.json`, `qpm.shared.json`, `qpm_defines.cmake`, and `mod.json` all report
  version `0.5.29`.

## SHA-256

```text
87a710d67ca9443d3976a0f24eb0fc1794a15d45625d86461ae4ea9a882a66d0  build/libVivify.so
a7f1d1b58131b88dd4113cb45fad717b1d6d7106ade9151bb70748ce3f4123c3  build/debug/libVivify.so
05791883cb6bd64f2386c9fc065b7389cafc653e4da866cf417eefb6db8a79fa  Vivify-Quest-0.5.29.qmod
```

## Quest installation and runtime boundary

The connected Quest 3S was idle, so the final stripped library was copied to:

`/sdcard/ModData/com.beatgames.beatsaber/Modloader/mods/libVivify.so`

The previous log was preserved as `Vivify.pre-0.5.29.log`. Reading the installed
file back with `sha256sum` returned
`87a710d67ca9443d3976a0f24eb0fc1794a15d45625d86461ae4ea9a882a66d0`,
identical to the packaged release binary.

An unattended launch was attempted after installation. Horizon OS intercepted
it with `common_system_dialog_app_launch_blocked_controller_required` before
the Beat Saber process or mod loader started. Therefore this record does not
claim that 0.5.29 loaded in Unity or passed gameplay, Flux depth, both-eye,
frame-pacing, result-screen, menu-exit, restart, Murder Plot, or soak tests.
Complete `STABILITY_TESTS_0.5.29.md` with an awake physical controller and
retain the resulting diagnostic ZIP before distribution to other users.
