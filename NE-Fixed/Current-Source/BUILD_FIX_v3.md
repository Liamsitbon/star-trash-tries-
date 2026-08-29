# Build/QMOD fix v3

- Fixed invalid `TrackParenting::ParentController` qualification. `ParentController` is declared in the global namespace.
- Fixed project validation after `qpm restore` / `qpm s build` by excluding generated `build`, `extern`, and `shared` directories.
- Allowed local ignored `ndkpath.txt` files during validation while continuing to exclude them from source archives.
- CMake deprecation messages printed from the Android NDK toolchain are warnings and do not block the build.
