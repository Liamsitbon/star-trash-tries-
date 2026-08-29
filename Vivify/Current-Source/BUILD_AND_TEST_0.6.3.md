# Vivify Quest 0.6.3 local build and package record

Date: 2026-08-12  
Target: Beat Saber `1.40.8_7379`, Scotland2, Android ARM64  
Candidate: Vivify `0.6.3`

## Evidence boundary

This file records local source, host-test, ARM64 build and package results.
Physical Quest gameplay is pending until the candidate is installed and the
matrix in `STABILITY_TESTS_0.6.3.md` is completed.

## Result

| Layer | Result |
| --- | --- |
| Authentic Axo 0.4.1 lineage check | PASS |
| Axo fingerprint/integrity behavior retained | PASS |
| No scene-wide note scan in assignment refresh | PASS |
| Track-targeted active-note registry present | PASS |
| Lifecycle host tests | PASS |
| Quest performance policy ASan/UBSan tests | PASS |
| Quest performance analyzer fixture | PASS |
| Clean RelWithDebInfo ARM64 build with LTO | PASS |
| Stripped/unstripped ELF inspection | PASS |
| QMOD content and payload-hash verification | PASS |
| Physical Aether bridge validation | PENDING |

## Required commands

```sh
scripts/verify-port-source.sh
pwsh -NoProfile -File scripts/build.ps1 -clean
pwsh -NoProfile -File scripts/createqmod.ps1 -qmodName Vivify-Quest-0.6.3
```

## Command results

```text
$ scripts/verify-port-source.sh
PASS lifecycle state-machine test
PASS Quest performance policy tests (ASan/UBSan)
PASS Quest performance capture analyzer test
PASS metadata version consistency (0.6.3)
PASS all 11 Windows-master event identifiers are registered
PASS required lifecycle/parity implementation anchors are present
PASS Axo-style active-note refresh avoids global scans and targets affected tracks
PASS pause/resume preserves authored render settings across XR suspend
PASS QMOD manifest target and payload declaration

$ pwsh -NoProfile -File scripts/build.ps1 -clean
[15/15] Linking CXX shared library libVivify.so; Preserving debug symbols and stripping the packaged Vivify library

$ pwsh -NoProfile -File scripts/createqmod.ps1 -qmodName Vivify-Quest-0.6.3
Creating qmod from mod.json
Validating mod.json...
```

## Binary and package inspection

```text
build/libVivify.so:
  ELF64 AArch64 shared object, stripped
  size: 2245128 bytes

build/debug/libVivify.so:
  ELF64 AArch64 shared object, debug_info, not stripped
  size: 26466280 bytes

SONAME: libVivify.so
```

`Vivify-Quest-0.6.3.qmod` is 741626 bytes and contains exactly:

```text
mod.json          2366 bytes
libVivify.so   2245128 bytes
```

The packaged manifest reports Vivify `0.6.3`, Beat Saber
`1.40.8_7379`, Scotland2, and `lateModFiles: ["libVivify.so"]`.

SHA-256:

```text
2617e8cd3e3de0b33e24de68560e3d936c85f17af95d0afb30b6ce3d8de79406  build/libVivify.so
b5b093718fbe9ae66db53d7239eba0fd3899e6b9b9739add78ef45979c919099  build/debug/libVivify.so
6a15955971c49e0a709784f3adf69592e630acc99094ea55e548118bbc78ce3d  Vivify-Quest-0.6.3.qmod
```

The `libVivify.so` extracted from the QMOD is byte-identical to
`build/libVivify.so` and has the same SHA-256. The stripped binary contains the
embedded `0.6.3` version and the new note-refresh performance-counter string.
