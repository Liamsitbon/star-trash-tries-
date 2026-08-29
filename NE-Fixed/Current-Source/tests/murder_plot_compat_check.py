#!/usr/bin/env python3
import argparse
import json
from pathlib import Path

def fail(msg):
    raise SystemExit(f"FAIL: {msg}")

def check_source(root: Path):
    fake_helper = (root / "src/Hooks/FakeNotes/FakeNoteHelper.cpp").read_text()
    late = (root / "src/Hooks/FakeNotes/BeatmapData.cpp").read_text()
    movement = (root / "src/Animation/NoodleMovementDataProvider.cpp").read_text()
    row_processor = (root / "src/Hooks/BeatmapObjectsInTimeRowProcessor.cpp").read_text()
    spawn = (root / "include/SpawnDataHelper.h").read_text()
    notes = (root / "src/Hooks/NoteController.cpp").read_text()
    jump = (root / "src/Hooks/NoteJump.cpp").read_text()
    saber_culling = (root / "src/Hooks/SmallFixes/SaberCullingFix.cpp").read_text()
    saber_movement = (root / "src/Hooks/SmallFixes/SaberPlayerMovementFix.cpp").read_text()
    corrected_compute = saber_movement.split(
        "if (self->_validCount > 0)", 1
    )[1].split("MAKE_HOOK_MATCH(SaberSwingRatingCounter_ProcessNewData", 1)[0]

    required = [
        ("shared V3 injector", "EnsureV3FakeObjectsInjected" in fake_helper),
        ("JSON-stage fake-object injection", "InjectV3FakeObjectsIntoJson" in late),
        ("injection before CustomJSONData conversion",
         "V3_BeatmapDataLoader_GetBeatmapDataFromSaveDataJson" in late
         and "before CustomJSONData conversion" in late),
        ("per-object BeatOffset semantics", "auto valueType = noteJumpSpeedOverride.has_value()" in movement),
        ("spawn helper override guard", "!inputNjs.has_value() && !inputOffset.has_value()" in spawn),
        ("dense animated fake notes keep authored start row",
         "authoredAnimatedVisual" in row_processor
         and "ad.objectData.disableNoteGravity.value_or(false)" in row_processor
         and "ad.animationData.definitePosition" in row_processor
         and "ad.objectData.startY.value_or(list[m]->noteLineLayer.value__)" in row_processor),
        ("authored interactability", "if (!isFakeNote)" not in notes),
        ("no unsafe renderer hard-hide",
         "SetNoteRenderersHardHidden" not in notes
         and "renderer->set_enabled" not in notes),
        ("pooled arrow-controller null guard", "nc != nullptr && nc->get_gameObject() != nullptr" in notes),
        ("post-miss endpoint interpolation", "if (self->_missedMarkReported)" in jump
         and "movement.moveEndPosition" in jump
         and "movement.jumpEndPosition" in jump),
        ("player-track saber trail culling",
         "SaberTrailRenderer_UpdateMesh" in saber_culling
         and "getStaticF_positiveInfinityVector" in saber_culling
         and "isNoodleHookEnabled" in saber_culling),
        ("player-track saber movement space conversion",
         "SaberMovementData_ComputeAdditionalData" in saber_movement
         and "SaberSwingRatingCounter_ProcessNewData" in saber_movement
         and "InverseComputeWorld" in saber_movement
         and "if (!_origin) return SaberTrail_Setup" in saber_movement
         and "SaberMovementData_ComputeAdditionalData(" not in corrected_compute
         and "SaberTrail_OnDestroy" in saber_movement
         and "_worldMovementData.erase(it)" in saber_movement),
    ]
    for name, ok in required:
        if not ok:
            fail(name)
    print("Source compatibility checks: PASS")

def check_map(path: Path):
    with path.open(encoding="utf-8-sig") as f:
        data = json.load(f)
    cd = data.get("customData", {})
    fake_notes = cd.get("fakeColorNotes", [])
    fake_obstacles = cd.get("fakeObstacles", [])
    real_notes = data.get("colorNotes", [])
    print(f"Map counts: real={len(real_notes)} fakeColorNotes={len(fake_notes)} fakeObstacles={len(fake_obstacles)}")
    if len(fake_notes) < 3000:
        fail("expected a dense V3 fake-note map")

    if (len(real_notes), len(fake_notes), len(fake_obstacles)) != (912, 3851, 51):
        fail("audited Murder Plot counts changed")

    for start in (108.0, 204.0, 236.0):
        group = [n for n in fake_notes if start <= float(n.get("b", -999)) <= start + 0.35]
        print(f"Dense fake group {start:.2f}-{start+0.35:.2f}: {len(group)}")
        if len(group) < 200:
            fail(f"dense fake group around beat {start} is missing")
        if not all(n.get("customData", {}).get("uninteractable") is True for n in group):
            fail(f"not all fake notes around beat {start} are uninteractable")
        if not all("dissolve" in n.get("customData", {}).get("animation", {}) for n in group):
            fail(f"fake notes around beat {start} are missing dissolve animation")
        if not all("dissolveArrow" in n.get("customData", {}).get("animation", {}) for n in group):
            fail(f"fake notes around beat {start} are missing arrow dissolve animation")
        if not all("definitePosition" in n.get("customData", {}).get("animation", {}) for n in group):
            fail(f"fake notes around beat {start} are missing definite-position animation")
        if not all(n.get("customData", {}).get("noteJumpStartBeatOffset", 0) >= 36 for n in group):
            fail(f"fake notes around beat {start} lost their long jump lifetime")
    points = cd.get("pointDefinitions", {})
    for value in ("0.06", "0.13", "0.19", "0.25", "0.31", "0.38", "0.44", "0.50"):
        name = f"meTD{value}"
        curve = points.get(name)
        if not curve or curve[0][0] != 0 or curve[-1][0] != 0:
            fail(f"point definition {name} is missing its hidden boundaries")
    print("Murder Plot map-shape checks: PASS")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", default=Path(__file__).resolve().parents[1], help="Noodle source root")
    parser.add_argument("--map", help="Path to Murder Plot ExpertPlusLawless.dat")
    args = parser.parse_args()
    root = Path(args.source).resolve()
    check_source(root)
    if args.map:
        check_map(Path(args.map))
    print("ALL COMPATIBILITY CHECKS PASSED")

if __name__ == "__main__":
    main()
