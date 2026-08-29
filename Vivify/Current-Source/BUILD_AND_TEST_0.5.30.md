# Vivify Quest 0.5.30 — build, package, and device installation record

## Target

- Beat Saber: `1.40.8_7379`
- Mod loader: Scotland2
- Architecture: ELF64 little-endian AArch64 shared object
- Companion Noodle Extensions build: `1.8.3`

## Build and package validation

The complete native project configured and linked successfully with the QPM
NDK `27.3.13750724+preview-0`. The stripped and debug binaries share build ID
`a4e0669057fff08637cc76240634f297d7101fa9`. Both loader exports (`setup` and
`late_load`) are present. `scripts/validate-modjson.ps1`, QMOD manifest checks,
and `unzip -t` passed.

SHA-256:

```text
a72c57329d952b4871c3416aa8f20fcab26d901721c219a0c141710c9f0fa6f2  build/libVivify.so
d277612fb5d2193f2195ec29d51526311e7d6558e90270037bf047b2dec01658  build/debug/libVivify.so
b7fac288d5c7b4674b7da564d1b43959002bdd17861ab7eb9fb81399b7e8eef0  Vivify-Quest-0.5.30.qmod
```

The QMOD embeds `vivify` version `0.5.30`, targets Beat Saber `1.40.8_7379`,
and contains the hash-verified stripped library.

## Companion Noodle Extensions build

Noodle Extensions 1.8.3 passed its project validator, a clean 59-unit ARM64
build, loader-export/ELF checks, and QMOD integrity testing.

```text
faa62b9e4b339a9a1a67af8c706516027c6a6bf71ce175b27c1bdb69a5759c88  build/libnoodleextensions.so
e8b35ab82a859c63770dc3f723d1e15965ffcda06c92bfa244b6978f426817da  build/debug/libnoodleextensions.so
5e9bacecf0e587c70b057f18c4b90d4682edc5314066b7a9d790a36026c0c9c6  NoodleExtensions-Quest-1.8.3.qmod
```

## Device installation

The connected Quest 3S was not running Beat Saber during replacement. Old
internal and staging libraries were preserved under
`final/device-backup-2026-08-09-pre-0.5.30-1.8.3/` before installation.

Both the internal `files/mods` copies and the ModData loader staging copies were
updated and read back through ADB. Their hashes exactly match the new stripped
libraries above. The device configuration keeps Vivify debug logging and all
required blit/filmgrain/secondary-camera/prewarm paths enabled; Noodle note,
mirror-note, obstacle dissolve, lifecycle reset, and runtime diagnostics are
also enabled.

## Runtime status

An unattended launch reached Meta Horizon's
`LaunchCheckControllerRequiredDialogActivity` and did not start Beat Saber.
This is a controller/headset-presence gate before Unity or the mod loader, not a
mod crash. No new 0.5.30 runtime log was produced, so gameplay and visual parity
remain unverified. Use `STABILITY_TESTS_0.5.30.md` after waking the headset and
controller; do not release to a broad audience based on build/install success
alone.
