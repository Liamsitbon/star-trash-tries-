# Vivify Quest 0.5.4 build fix

The source archive does not include QPM's generated `extern/` directory. Previously, running `qpm s build` directly could fail with:

```text
fatal error: 'scotland2/shared/modloader.h' file not found
```

`scotland2` was already declared correctly in `qpm.json`; it simply had not been restored into `extern/includes`.

The updated `scripts/build.ps1` now:

1. Checks for required Scotland2, beatsaber-hook, and bs-cordl headers.
2. Runs `qpm restore` automatically only when those headers are missing.
3. Stops with a clear error if restore succeeds but a required header is still absent.
4. Continues with the normal CMake/Ninja build.

Recommended command:

```bash
qpm s build
```

A manual clean build remains available:

```bash
rm -rf build extern
qpm restore
qpm s build
```
