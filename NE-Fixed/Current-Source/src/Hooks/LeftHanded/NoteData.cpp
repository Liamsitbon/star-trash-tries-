// Beat Saber 1.40.8 / bs-cordl 4008 compatibility:
//
// The old NoteData::Mirror hook referenced Noodle associated-data fields that
// no longer exist (`flip` and `objectData.cutDirection`) and also used the old
// hook installer signature. The current Quest port keeps this hook disabled;
// left-handed handling that is still compatible remains in the other runtime
// paths (animation, player tracks, spawn movement and obstacle mirroring).
//
// Intentionally empty translation unit.
