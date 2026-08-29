# Vivify Quest 0.6.2 local build, package, and staging record

Date: 2026-08-11  
Target: Beat Saber `1.40.8_7379`, Scotland2, Android ARM64  
Candidate: Vivify `0.6.2`

## Result

| Layer | Result |
| --- | --- |
| Lifecycle host tests | PASS |
| Quest timing/performance policy tests under ASan/UBSan | PASS |
| Quest performance analyzer fixture | PASS |
| Source and metadata verification | PASS |
| Clean RelWithDebInfo ARM64 build with LTO | PASS |
| Stripped/unstripped artifact inspection | PASS |
| QMOD content and payload-hash verification | PASS |
| External device staging on Quest 3S | PASS |
| Private runtime file staged for Beat Saber | PASS |
| 0.6.2 observed loading in a new Beat Saber process | PENDING — Meta controller-required launch dialog |
| Aether gameplay validation of 0.6.2 | PENDING |

Build/package success is not gameplay proof. The reproduced 0.6.1 failure and
the 0.6.2 acceptance procedure are documented in
`AETHER_RESUME_FIX_0.6.2.md` and `STABILITY_TESTS_0.6.2.md`.

## Commands and test output

```text
$ scripts/verify-port-source.sh
PASS lifecycle state-machine test
PASS Quest performance policy tests (ASan/UBSan)
PASS Quest performance capture analyzer test
PASS metadata version consistency (0.6.2)
PASS all 11 Windows-master event identifiers are registered
PASS required lifecycle/parity implementation anchors are present
PASS pause/resume preserves authored render settings across XR suspend
PASS QMOD manifest target and payload declaration

$ pwsh -NoProfile -File scripts/build.ps1 -clean
[15/15] Linking CXX shared library libVivify.so; Preserving debug symbols and stripping the packaged Vivify library

$ pwsh -NoProfile -File scripts/createqmod.ps1 -qmodName Vivify-Quest-0.6.2
Creating qmod from mod.json
Validating mod.json...
```

## Binary inspection

```text
build/libVivify.so:
  ELF64, little endian, AArch64, shared object, stripped
  size: 2242296 bytes

build/debug/libVivify.so:
  ELF64, little endian, AArch64, shared object, debug_info, not stripped
  size: 26338296 bytes

SONAME: libVivify.so
```

Dynamic dependencies are the expected Android/Quest runtime libraries:
`liblog`, `libbeatsaber-hook`, `libbsml`, `libcustom-json-data`,
`libcustom-types`, `libmetacore`, `libpaper2_scotland2`, `libsl2`,
`libsongcore`, `libtracks`, `libweb-utils`, `libm`, `libdl`, and `libc`.

The stripped library contains the embedded version `0.6.2` and the new
debug-only timing/resume markers.

## Package inspection

`Vivify-Quest-0.6.2.qmod` contains exactly:

```text
mod.json                 2363 bytes
libVivify.so          2242296 bytes
```

The manifest reports:

```json
{
  "id": "vivify",
  "version": "0.6.2",
  "packageId": "com.beatgames.beatsaber",
  "packageVersion": "1.40.8_7379",
  "modloader": "Scotland2",
  "lateModFiles": ["libVivify.so"]
}
```

SHA-256:

```text
a971414a686f8f7498958edb1c5288b5912ac5bc24172d2f61385b127d31926c  build/libVivify.so
44e8e17d477a503702d557a7695809df6ef3de760442b55b747f68610ea1a5aa  build/debug/libVivify.so
387c231c7784e202804d92bfe637406214ffefb9210142fb200c9b3e2c3a73f3  Vivify-Quest-0.6.2.qmod
```

The library extracted from the QMOD has the same
`a971414a686f8f7498958edb1c5288b5912ac5bc24172d2f61385b127d31926c`
hash as `build/libVivify.so`.

## Device staging status

Connected hardware identifies as Quest 3S. The following staged files both
match the 0.6.2 build hash:

```text
/sdcard/ModData/com.beatgames.beatsaber/Packages/1.40.8_7379/vivify_v0.6.2/libVivify.so
/sdcard/ModData/com.beatgames.beatsaber/Modloader/mods/libVivify.so
```

The old `vivify_v0.6.1` package directory was retained for rollback. The
app-private runtime copy was also staged and hashes to the 0.6.2 value:

```text
/data/user/0/com.beatgames.beatsaber/files/mods/libVivify.so
```

Meta's `Controllers required` launch check still prevented a new Beat Saber
process from starting at the time this record was written. Matching a file on
disk proves staging, not that the runtime loaded it. Confirm a new-process log
reports Vivify 0.6.2 before beginning headset acceptance.
