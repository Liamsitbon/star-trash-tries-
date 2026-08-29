# Packaging fix v2

This source archive uses a new top-level directory name so macOS Finder cannot silently reuse or merge an older extracted folder.

Verified required files:

- `mod.json`
- `qpm.json`
- `src/main.cpp`
- `src/NoodleExtensions.cpp`
- `src/SpawnDataHelper.cpp`

Run from this exact directory:

```bash
bash scripts/validateProject.sh
qpm restore
qpm s qmod
```
