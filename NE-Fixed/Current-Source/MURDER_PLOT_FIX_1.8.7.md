# Noodle Extensions Quest 1.8.7 — Murder Plot / V3 fake-object fix

## What this changes

Murder Plot (BSR 45d7f) contains thousands of V3 fake objects. The 1.8.6 source
already had zero-dissolve renderer hard-hiding, but the runtime log still showed:

`V3 fake objects were not pre-injected; using the compatibility fallback`

The reason is lifecycle ordering: custom songs may already be parsed/cached by
SongCore before Noodle's `CustomJSONData::v3::Parser::ParsedEvent` callback is
registered. The cached save data therefore never receives the early injection marker.

1.8.7 calls the same idempotent fake-object injector directly from the V3
BeatmapDataLoader hook *before* the original CJD loader runs. This makes the
normal CJD conversion/sort path independent of whether the song was cached.

## Additional fixes

- Per-object NJS / noteJumpStartBeatOffset follows PC/upstream BeatOffset
  semantics instead of accidentally inheriting JumpDuration interpretation.
- Authored note `interactable` animation is no longer forced on for every
  non-fake note.
- Fake obstacle filtering no longer mutates its input list while iterating.
- Obstacle dissolve gracefully skips custom obstacle models that do not contain
  ObstacleDissolve/CutoutAnimateEffect.

## Expected log on a V3 fake-object map

The preferred path should log an injection message before/around CJD conversion
and should *not* log:

`V3 fake objects were not pre-injected; using the compatibility fallback`

The late fallback is intentionally retained as a last-resort compatibility path.
