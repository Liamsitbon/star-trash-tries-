# Noodle Extensions Quest 1.8.0 Merged

## Merged from both user-provided sources
- Preserves the 1.40.8 compatibility fixes from `Noodle-Extensions-Quest-Fixed`.
- Includes the enhanced source build scripts, restored `src/main.cpp`, runtime reset logic and validation tooling.

## Completed in this merge
- Added per-map Vivify/shared-Tracks compatibility state.
- Prevented Noodle pooled-material reset from overwriting Vivify-owned note renderers.
- Added null-safe material reset for vanilla pooled notes.
- Applied left-handed mode to parent-track transforms and `GameObjectTrackController`.
- Kept automatic cache, parent and movement-provider cleanup across scene transitions.
- Kept Mapping Extensions conflict handling configurable.
- Added compatibility documentation and a build/source validation pass.

## Intentionally not claimed as complete
- Chroma fog/lighting rewrite.
- Vivify shader/render pipeline implementation.
- A full embedded Heck runtime. Tracks and CustomJSONData remain the shared Quest equivalents.
- Legacy mirror TODOs that require upstream parser/gameplay semantics and runtime map tests.
