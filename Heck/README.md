# Heck source integration

`Heck/Upstream` is a pinned git submodule of Aeroluna's official Heck repository.

- Upstream: `https://github.com/Aeroluna/Heck.git`
- Branch: `master`
- Pinned commit: `3bbcaf6733f6faa688a70b296b4d83c92c1f5dc8`
- License: MIT (upstream license remains authoritative)

## Why it is included this way

Heck is the upstream PC/C# framework used by the Noodle Extensions / Chroma / Vivify ecosystem. The Quest projects in this repository are native ARM64 C++ QMODs, so copying a Heck PC DLL into a Quest package would be incorrect and would violate the repository's Quest-only package contract.

The pinned source gives the Quest ports an auditable semantic reference for tracks, point definitions, custom events and compatibility work without pretending that the PC binary is a Quest build. The release workflow continues to reject `.dll`, `.exe`, `.dylib` and `.pdb` payloads from QMOD artifacts.

Clone with submodules when auditing parity:

```bash
git clone --recurse-submodules <repo-url>
```

or, in an existing checkout:

```bash
git submodule update --init --recursive Heck/Upstream
```

## Map regression targets

The current Vivify live-map contract explicitly covers:

- Murder Plot — BSR `45d7f`
- 42-flux — BSR `43999`

Keep Quest-specific fixes in the native ports and use `Heck/Upstream` as the reference behavior. Do not package the upstream PC assemblies into Quest QMODs.
