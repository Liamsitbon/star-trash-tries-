#!/usr/bin/env python3
"""Map-aware Quest compatibility checks for the known Vivify regressions.

The maps are intentionally supplied by path instead of committed into the mod
source. This makes the check validate the exact BeatSaver downloads used for a
release audit without redistributing mapper content.
"""

from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def fail(message: str) -> None:
    raise SystemExit(f"FAIL: {message}")


def load(path: Path) -> dict:
    if not path.is_file():
        fail(f"required map file is missing: {path}")
    return json.loads(path.read_text(encoding="utf-8-sig"))


def events(data: dict, event_type: str) -> list[dict]:
    return [
        event
        for event in data.get("customData", {}).get("customEvents", [])
        if event.get("t") == event_type
    ]


def at_beat(items: list[dict], beat: float, epsilon: float = 0.0001) -> list[dict]:
    return [item for item in items if abs(float(item.get("b", -9999)) - beat) <= epsilon]


def bundle_info(data: dict) -> dict:
    custom = data.get("customData") or data.get("_customData") or {}
    return custom.get("assetBundle") or custom.get("_assetBundle") or {}


def check_murder_plot(map_dir: Path) -> None:
    data = load(map_dir / "ExpertPlusLawless.dat")
    custom = data.get("customData", {})
    real = data.get("colorNotes", [])
    fake = custom.get("fakeColorNotes", [])
    fake_obstacles = custom.get("fakeObstacles", [])
    if (len(real), len(fake), len(fake_obstacles)) != (912, 3851, 51):
        fail("Murder Plot object counts changed")

    for start in (108.0, 204.0, 236.0):
        dense = [note for note in fake if start <= float(note.get("b", -1)) <= start + 0.35]
        if len(dense) != 251:
            fail(f"Murder Plot dense group at beat {start} is not 251 notes")
        for note in dense:
            note_custom = note.get("customData", {})
            animation = note_custom.get("animation", {})
            if note_custom.get("uninteractable") is not True:
                fail(f"Murder Plot fake at beat {start} became interactable")
            if not isinstance(animation.get("dissolve"), str) or not isinstance(
                animation.get("dissolveArrow"), str
            ):
                fail(f"Murder Plot fake at beat {start} lost authored dissolve references")

    points = custom.get("pointDefinitions", {})
    expected = [f"meTD{value:.2f}" for value in (0.06, 0.13, 0.19, 0.25, 0.31, 0.38, 0.44, 0.50)]
    for name in expected:
        curve = points.get(name)
        if not curve or curve[0][0] != 0 or curve[-1][0] != 0:
            fail(f"Murder Plot point definition {name} lost its hidden boundaries")
    print("Murder Plot contract: PASS (912 real, 3851 fake, three 251-note groups)")


def check_42_flux(map_dir: Path) -> None:
    data = load(map_dir / "ExpertPlus.dat")
    custom = data.get("customData", {})
    if (len(data.get("colorNotes", [])), len(custom.get("fakeColorNotes", []))) != (877, 1286):
        fail("42-flux note counts changed")

    cameras = events(data, "CreateCamera")
    first_camera = at_beat(cameras, 47.0)
    if not any(event.get("d", {}).get("id") == "NotesOneCam" for event in first_camera):
        fail("42-flux NotesOneCam creation at beat 47 is missing")

    camera_properties = events(data, "SetCameraProperty")
    cleanup = at_beat(camera_properties, 112.0)
    if not any(event.get("d", {}).get("properties", {}).get("culling") is None for event in cleanup):
        fail("42-flux does not clear main-camera culling at beat 112")

    assignments = at_beat(events(data, "AssignObjectPrefab"), 528.0)
    expected_asset = "assets/morenotefucky/geodissolvenote.prefab"
    if not any(event.get("d", {}).get("colorNotes", {}).get("asset") == expected_asset for event in assignments):
        fail("42-flux late note prefab assignment is missing")

    points = custom.get("pointDefinitions", {})
    if points.get("vis") != [1]:
        fail("42-flux vis point definition must be fully visible")
    visibility_restore = at_beat(events(data, "AnimateTrack"), 554.0)
    if not any(
        event.get("d", {}).get("track") == "realPlanetaryNotes"
        and event.get("d", {}).get("dissolve") == "vis"
        and event.get("d", {}).get("dissolveArrow") == "vis"
        for event in visibility_restore
    ):
        fail("42-flux does not restore the late cuttable notes at beat 554")
    late_real_notes = [
        note
        for note in data.get("colorNotes", [])
        if float(note.get("b", -1)) >= 554
        and note.get("customData", {}).get("track") == "realPlanetaryNotes"
    ]
    if len(late_real_notes) != 66:
        fail(f"42-flux expected 66 late realPlanetaryNotes, found {len(late_real_notes)}")

    hud_hide = at_beat(events(data, "AnimateTrack"), 570.0)
    if not any(
        event.get("d", {}).get("track") == "hud"
        and event.get("d", {}).get("position") == [0, -100, -100]
        for event in hud_hide
    ):
        fail("42-flux authored HUD removal at beat 570 is missing")
    print("42-flux contract: PASS (camera cleanup, HUD hide, 66 late visible notes)")


def collect_asset_strings(value: object) -> set[str]:
    found: set[str] = set()
    if isinstance(value, dict):
        for key, child in value.items():
            if key in {"asset", "trailAsset", "debrisAsset", "anyDirectionAsset"} and isinstance(child, str):
                found.add(child.lower())
            found.update(collect_asset_strings(child))
    elif isinstance(value, list):
        for child in value:
            found.update(collect_asset_strings(child))
    return found


def check_aether(map_dir: Path) -> None:
    data = load(map_dir / "ExpertPlusLawless.dat")
    info = load(map_dir / "Info.dat")
    custom = data.get("customData", {})
    counts = Counter(event.get("t") for event in custom.get("customEvents", []))
    if (len(data.get("colorNotes", [])), len(custom.get("fakeColorNotes", []))) != (703, 16):
        fail("Aether note counts changed")
    expected_counts = {
        "AnimateTrack": 1806,
        "AssignObjectPrefab": 1363,
        "AssignPathAnimation": 236,
        "SetMaterialProperty": 65,
    }
    for name, expected in expected_counts.items():
        if counts[name] != expected:
            fail(f"Aether {name} count is {counts[name]}, expected {expected}")
    assets = collect_asset_strings(events(data, "AssignObjectPrefab"))
    if len(assets) != 18:
        fail(f"Aether expected 18 unique assigned asset paths, found {len(assets)}")
    info_bundle = bundle_info(info)
    if "_android2021" in info_bundle or "android2021" in info_bundle:
        fail("official Aether unexpectedly advertises an Android bundle; audit the new version")
    if not any("windows" in path.name.lower() for path in map_dir.glob("*.vivify")):
        fail("official Aether Windows reference bundle is missing")
    print("Aether contract: PASS (1363 prefab assignments; official map remains Windows-only)")


def check_aether_port(map_dir: Path) -> None:
    data = load(map_dir / "ExpertPlusLawless.dat")
    info = load(map_dir / "Info.dat")
    custom = data.get("customData", {})
    counts = Counter(event.get("t") for event in custom.get("customEvents", []))
    if (len(data.get("colorNotes", [])), len(custom.get("fakeColorNotes", []))) != (703, 16):
        fail("installed Aether port note counts changed")
    if counts["AssignObjectPrefab"] != 1363 or counts["SetMaterialProperty"] != 65:
        fail("installed Aether port lost authored prefab/material events")
    android_checksum = bundle_info(info).get("_android2021")
    android_bundle = map_dir / "bundleAndroid2021.vivify"
    if not isinstance(android_checksum, int) or android_checksum <= 0 or not android_bundle.is_file():
        fail("installed Aether port does not advertise and contain its Android 2021 bundle")
    if android_bundle.stat().st_size < 1_000_000:
        fail("installed Aether Android bundle is unexpectedly small")
    print(
        "Aether installed-port contract: PASS "
        f"(Android checksum={android_checksum}; size={android_bundle.stat().st_size})"
    )


def check_you(map_dir: Path) -> None:
    info = load(map_dir / "Info.dat")
    for filename in ("HardStandard.dat", "NormalStandard.dat"):
        data = load(map_dir / filename)
        if len(data.get("colorNotes", [])) != 160:
            fail(f"You {filename} note count changed")
        if len(events(data, "AssignObjectPrefab")) != 10:
            fail(f"You {filename} prefab-assignment count changed")
        if len(collect_asset_strings(events(data, "AssignObjectPrefab"))) != 17:
            fail(f"You {filename} assigned asset set changed")
        outro = data.get("colorNotes", [])[-8:]
        if len(outro) != 8 or not all(
            note.get("customData", {}).get("animation", {}).get("dissolve") == [0]
            and "outroNote" in note.get("customData", {}).get("track", [])
            for note in outro
        ):
            fail(f"You {filename} no longer has eight stock-hidden outro replacement notes")
        outro_assignments = at_beat(events(data, "AssignObjectPrefab"), 0.0)
        if not any(
            event.get("d", {}).get("colorNotes", {}).get("track") == "outroNote"
            and event.get("d", {}).get("colorNotes", {}).get("asset")
            == "assets/prefabs/notes/glassnote.prefab"
            for event in outro_assignments
        ):
            fail(f"You {filename} outroNote glass prefab assignment is missing")
    checksum = bundle_info(info).get("_android2021")
    if checksum != 1563550775:
        fail(f"You Android checksum changed: {checksum}")
    print("You contract: PASS (both difficulties; Android checksum and prefab set)")


def check_source() -> None:
    assets = (ROOT / "src/VivifyAssets.cpp").read_text(encoding="utf-8")
    core = (ROOT / "src/VivifyCore.cpp").read_text(encoding="utf-8")
    internal = (ROOT / "include/VivifyRuntimeInternal.hpp").read_text(encoding="utf-8")
    events_source = (ROOT / "src/VivifyEvents.cpp").read_text(encoding="utf-8")
    hooks = (ROOT / "src/VivifyHooks.cpp").read_text(encoding="utf-8")
    post = (ROOT / "src/VivifyPostProcessing.cpp").read_text(encoding="utf-8")
    prefabs = (ROOT / "src/VivifyObjectPrefabs.cpp").read_text(encoding="utf-8")
    requirements = {
        "desktop bundle exclusion": "IsDesktopBundleName(p)" in assets,
        "custom-to-official bundle retirement": "incomingLevelPath != _selectedLevelPath" in assets,
        "cross-map bundle assets released": assets.count("_mainBundle->Unload(true)") >= 2
        and "_mainBundle->Unload(false);" not in assets,
        "HUD renderer ownership": "_alwaysVisibleQuadDisabledRenderers" in internal
        and "renderer->set_enabled(false)" in core,
        "full stereo-array copy": "QueueStereoArrayCopy" in post
        and "commandBuffer->CopyTexture" in post
        and "CopyStereoRenderTexture(tempPtr, cam.colorRT)" in post,
        "active note-pool refresh": "RefreshActiveNoteVisuals" in prefabs,
        "replacement fallback keeps stock notes": "preserving original" in prefabs,
        "Noodle replacement ownership marker": "__VivifyReplacement__" in prefabs,
        "XR descriptor gates startup post-processing":
            "if (!_hasMainDescriptor)" in events_source
            and "startup post-processing is waiting for the first XR render descriptor" in events_source
            and "needsStartupPostProcessing" in post,
        "Vivify-scoped VRCenterAdjust null isolation":
            "ShouldSkipBrokenVivifyVRCenterAdjust" in hooks
            and "runtime.GetCurrentBeatmapData() == nullptr || runtime.IsResetting()" in hooks
            and "VRCenterAdjust_SetRoomTransformOffset" in hooks,
    }
    missing = [name for name, present in requirements.items() if not present]
    if missing:
        fail("Vivify source guards missing: " + ", ".join(missing))
    print("Vivify source contract: PASS")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--maps-root", required=True, type=Path)
    parser.add_argument("--aether-port", type=Path)
    args = parser.parse_args()
    check_source()
    check_murder_plot(args.maps_root / "45d7f")
    check_42_flux(args.maps_root / "43999")
    check_aether(args.maps_root / "4968d")
    if args.aether_port:
        check_aether_port(args.aether_port)
    check_you(args.maps_root / "43a1f")
    print("ALL FOUR LIVE MAP CONTRACTS PASSED")


if __name__ == "__main__":
    main()
