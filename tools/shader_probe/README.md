# Isolated Quest shader compile/link probe

Purpose: distinguish **actual GLES driver compile/link failures** from missing
textures, wrong camera/depth/stereo inputs and Unity material/event behavior.
This is **not** a QMOD, renderer patch, benchmark or universal shader converter.
There are no draw calls: even a shader containing an unbounded raymarch loop is
only compiled/linked, never executed by this tool. A 60s process timeout limits
each invocation. Do not run it alongside gameplay; arrange a deliberate device test.

1. Use an isolated Python environment with `UnityPy==1.25.3`.
2. `python extract_gles.py bundleAndroid2021.vivify NEW_OUTPUT_DIRECTORY`
3. Build with the Android NDK CMake toolchain, `ANDROID_ABI=arm64-v8a` and
   `ANDROID_PLATFORM=24`: `cmake -S . -B build ...`, then `cmake --build build`.
4. On an authorized connected Quest, place this executable and generated directory
   in a new task-specific `/data/local/tmp` directory and invoke
   `quest_shader_probe EXTRACTED_DIRECTORY`. No installation is needed. Preserve
   stdout plus `extraction.json`/bundle SHA-256 so results map to exact inputs.

The extractor only unwraps Unity's outer `#ifdef VERTEX/FRAGMENT/GEOMETRY` blocks;
it does not change GLSL versions, force extensions, invent missing stages, or patch
shader logic. `#version` remains first. Reflection-only records and unpaired stages
are not presented as successful tests. GLES geometry support is queried/compiled
on the actual device, not inferred from "Quest" or from a PC shader target number.

Linking successfully does **not** prove correct array samplers, per-eye matrices,
render queue, depth conventions, resource bindings, Vulkan or visual quality.
Extracted files are map-author assets: keep them local, outside GitHub/source
archives, unless their license separately permits distribution. Input bundles
are never changed. Current output is preparation for a device test, not device proof.
