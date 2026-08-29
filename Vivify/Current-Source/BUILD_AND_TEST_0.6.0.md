# Vivify Quest 0.6.0 local build and package record

Date: 2026-08-11 (Asia/Jerusalem)

## Scope of this evidence

This record proves host-side source checks, a clean Android ARM64 compilation,
ELF inspection and QMOD integrity. It does **not** prove installation, gameplay,
visual parity, frame timing or crash freedom on a physical Quest.

## Source checks

Command:

```sh
scripts/verify-port-source.sh
```

Result:

```text
PASS lifecycle state-machine test
PASS metadata version consistency (0.6.0)
PASS all 11 Windows-master event identifiers are registered
PASS required lifecycle/parity implementation anchors are present
PASS QMOD manifest target and payload declaration
```

The lifecycle test compiles `VivifyLifecycle.cpp` independently with C++20,
`-Wall -Wextra -Werror`, then tests preparation, activation, suspend/resume,
generation invalidation and render-safe deferred retirement.

All JSON metadata also passed `jq empty`.

## Clean ARM64 build

Command:

```sh
pwsh -NoProfile -File scripts/build.ps1 -clean
```

Result: success, 15/15 Ninja steps completed. Toolchain:

- Android API 24
- ABI `arm64-v8a` / AArch64
- Clang 18.0.4
- NDK `27.3.13750724+preview-0`
- CMake build type `RelWithDebInfo`
- optimized/LTO packaged library plus unstripped diagnostic copy

Only CMake/NDK compatibility deprecation warnings were emitted; no source,
compiler or linker warning/error was reported.

ELF inspection:

```text
build/libVivify.so:       ELF 64-bit LSB shared object, ARM aarch64, stripped
build/debug/libVivify.so: ELF 64-bit LSB shared object, ARM aarch64, with debug_info, not stripped
SONAME:                   libVivify.so
Build ID:                 0f70147460a1a232fad9ea80e7ef692a64f4931b
Exports:                  setup, late_load
```

The embedded version string is `0.6.0`. Dynamic dependencies include the
manifest's Scotland2/Beat Saber mod libraries plus Android system libraries.

## QMOD creation and inspection

Command:

```sh
pwsh -NoProfile -File scripts/createqmod.ps1 -qmodName Vivify-Quest-0.6.0
```

The standard script completed its QMOD schema check. `unzip -t` reported no
errors. The archive contains exactly:

```text
mod.json       2,372 bytes
libVivify.so   2,237,096 bytes
```

The packed manifest resolves to:

```json
{
  "id": "vivify",
  "version": "0.6.0",
  "modloader": "Scotland2",
  "packageVersion": "1.40.8_7379",
  "lateModFiles": ["libVivify.so"]
}
```

The SHA-256 of the packaged `libVivify.so` exactly matches the clean-build
library.

## SHA-256

```text
db43f9891742b6a5b4e3c906bbb1cc240649133eb1fdb64154b4d9fb48a6b07b  Vivify-Quest-0.6.0.qmod
3d7f39d39acaf1d476719c0a1929f83382ea37f0543a4f633e1e2097d3501029  build/libVivify.so
1c7c99b17d0ef5c882c7094bf5eab28680f30a61ab2cd2555aafd4d53e7c466f  build/debug/libVivify.so
```

## Required remaining proof

Install on the intended Quest, confirm the installed library SHA-256, then run
every P0 case in `STABILITY_TESTS_0.6.0.md`. The historical post-gameplay menu
crash is not considered fixed until the same path survives repeated device
runs with logs showing no stale Vivify render callback after Main Menu becomes
active.
