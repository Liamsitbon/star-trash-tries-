# Vivify Quest 0.6.1 local build, test and package record

Date: 2026-08-11

Target: Beat Saber Quest `1.40.8_7379`, Scotland2, Android API 24,
`arm64-v8a` / AArch64.

## Evidence classification

| Layer | Result |
| --- | --- |
| Source/version verification | PASS |
| Host lifecycle unit test | PASS |
| Host Quest performance-policy test | PASS with ASan/UBSan |
| Performance-capture analyzer test | PASS with synthetic A/B data |
| Clean Android ARM64 compile/link | PASS |
| ELF identity/metadata inspection | PASS |
| QMOD structure, manifest, ZIP integrity and payload match | PASS |
| Physical Quest 3 install/gameplay/profile | **NOT RUN: no ADB device connected** |

This record does not convert source/build success into a gameplay or performance
claim. Device acceptance remains governed by `STABILITY_TESTS_0.6.1.md`.

## Host tests and source verifier

Command:

```sh
scripts/verify-port-source.sh
```

Result:

```text
PASS lifecycle state-machine test
PASS Quest performance policy tests (ASan/UBSan)
PASS Quest performance capture analyzer test
PASS metadata version consistency (0.6.1)
PASS all 11 Windows-master event identifiers are registered
PASS required lifecycle/parity implementation anchors are present
PASS QMOD manifest target and payload declaration
```

The host performance-policy binary is compiled as C++20 with `-Wall -Wextra
-Werror -fsanitize=address,undefined -fno-omit-frame-pointer`. It exercises:

- command-buffer cache match/invalidation across generation, owner and graph
  signature changes;
- deterministic culling refresh distribution across the 45–75 frame window;
- native-rate write suppression and stable practice-speed selection, including
  pause, seek/backward time and non-finite inputs.

The PowerShell analyzer test creates isolated baseline/candidate CSV fixtures,
verifies median/p95 and a `-25%` comparison result, and removes only its unique
temporary directory.

All three PowerShell files used by the Quest 3 capture flow were also parsed by
the PowerShell AST parser without errors:

- `scripts/collect-quest3-performance.ps1`
- `scripts/analyze-quest3-performance.ps1`
- `tests/Quest3PerformanceAnalysisTests.ps1`

## Clean Android build

Command:

```sh
pwsh -NoProfile -File scripts/build.ps1 -clean
```

Result: CMake configured a clean tree and Ninja completed all 15 compile/link
steps. The only configuration output of note was upstream CMake deprecation
warning text from the configured NDK toolchain; no compiler or linker warning
failed the build.

Toolchain/configuration evidence:

- NDK `27.3.13750724+preview-0`
- Clang/Clang++ `18.0.4`
- CMake build type `RelWithDebInfo`
- target triple `aarch64-none-linux-android24`
- C++20, final compile command includes `-O3`
- interprocedural optimization / ThinLTO link enabled
- generic ARM64 target; no Quest-3-only `-mcpu` and no `-ffast-math`

## ELF inspection

`file` and the NDK `llvm-objdump` report:

```text
build/libVivify.so:       ELF 64-bit LSB shared object, ARM aarch64, stripped
build/debug/libVivify.so: ELF 64-bit LSB shared object, ARM aarch64,
                          with debug_info, not stripped
architecture: aarch64
SONAME: libVivify.so
Build ID: c8407437bb2dcd5a8040142e7038c7375af94f89
```

Sizes:

```text
build/libVivify.so        2,240,728 bytes
build/debug/libVivify.so 26,327,904 bytes
```

The stripped library contains the embedded version `0.6.1`, the Quest
performance-heartbeat format string, and exports the required Scotland2 entry
points:

```text
000000000020f740 T setup
0000000000210188 T late_load
```

The dynamic section has `SONAME libVivify.so`, Android system dependencies and
the declared mod dependencies (`beatsaber-hook`, BSML, CustomJSONData,
CustomTypes, MetaCore, Paper/Scotland2, SongCore, Tracks and WebUtils). The
linker used fatal warnings, section garbage collection and no undefined-symbol
allowance.

## QMOD verification

Command:

```sh
pwsh -NoProfile -File scripts/createqmod.ps1 -qmodName Vivify-Quest-0.6.1
```

The archive contains exactly:

```text
mod.json
libVivify.so
```

Embedded manifest checks:

```json
{
  "id": "vivify",
  "version": "0.6.1",
  "modloader": "Scotland2",
  "packageVersion": "1.40.8_7379",
  "lateModFiles": ["libVivify.so"],
  "modFiles": [],
  "libraryFiles": []
}
```

`zip -T` reports the archive as OK. `cmp` confirms the embedded
`libVivify.so` is byte-for-byte identical to `build/libVivify.so`.

## Release hashes

```text
593ce646babdd25d0765e91f776dc409818a614f62156fb84097df5edca532b2  Vivify-Quest-0.6.1.qmod
6d2ae2dc19ffd8760ae050ec19b7a66496411ff05e098b6efb7f7d4f31953c11  build/libVivify.so
9f97b726b5fd673794b4eeaaff6f5b4a41709dbf790da3888edc4161b9f66519  build/debug/libVivify.so
```

The source archive hash is added after the release source snapshot is created;
the final release directory carries the authoritative `SHA256SUMS` file.

## Hardware status

`adb devices -l` returned an empty device list. Therefore this turn did not:

- install the QMOD or library;
- launch Beat Saber;
- exercise either eye on a Meta Quest 3;
- gather 0.6.1 CPU/GPU/RSS/thermal/frame evidence;
- close the gameplay-to-menu, restart, seek or repeated-map regressions.

The retained older Quest 3S logs are useful workload evidence only. They cannot
validate this 0.6.1 binary. The exact QMOD/hash above must be used for the next
device run.
