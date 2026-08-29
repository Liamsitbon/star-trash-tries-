// Beat Saber 1.40.8 / bs-cordl 4008 compatibility:
//
// The old BeatmapDataMirrorTransform hook depended on removed APIs:
// IReadonlyBeatmapData::get_beatmapEventsData and
// BeatmapEventTypeExtensions::Rot. The current Quest port keeps this hook
// disabled, so this file intentionally contains no hook registration.
//
// Intentionally empty translation unit.
