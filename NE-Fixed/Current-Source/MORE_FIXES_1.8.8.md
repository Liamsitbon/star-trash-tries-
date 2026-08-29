# Noodle Extensions Quest 1.8.8 — additional audit fixes

## Fixed

1. **Parent callback removal + teardown safety** — `RemoveCallback` now uses `find()` and does nothing when no callback exists. Runtime reset unregisters every tracked callback before clearing the callback map, preventing stale callbacks that capture old `ParentObject*` pointers.
2. **PlayerTrack event lifetime** — pause/resume delegates are per-instance and unsubscribed on destroy instead of two globals shared by Root/Head/LeftHand/RightHand.
3. **PlayerTrack null safety** — missing VR controller/origin transforms and failed Tracks controller creation now log and fail safely instead of dereferencing null.
4. **NE + Mapping Extensions config parity** — the gameplay activation path now honors `Block NE + ME conflicts`, matching level selection.
5. **Hitsound queue frame accounting** — frame budget is synchronized from both spawn and coroutine paths, so a 31+ note burst cannot leave the queue stuck until another note spawns. FIFO ordering is preserved and the old recursive double-count was removed.

## Investigated but intentionally not changed

- `ParentObject` V2 world-rotation also rotates its position vector: this matches current PC behavior.
- Tracks `HandleTrackData(..., overwrite=true)` reuses an existing `GameObjectTrackController`; it does not create competing controllers.
- Left-handed mode still has incomplete static Noodle custom-data mirroring in the current Quest/upstream port. A correct fix needs a dedicated parity pass because the old `NoteData::Mirror` hook runs before current associated-data parsing.
- Burst-slider head angle/scoring logic present on PC remains commented in Quest. It is a real parity gap, but is not enabled in 1.8.8 without a native 1.40.8 API compile/runtime test.
- The Qosmetics-disable config remains legacy/dead because its old integration code is commented and Qosmetics is not an active dependency.

## Validation scope

Static project/JSON/shell/Python checks and the Murder Plot compatibility checker are run before packaging. A real Quest native build and headset runtime test are still required for final confirmation.
