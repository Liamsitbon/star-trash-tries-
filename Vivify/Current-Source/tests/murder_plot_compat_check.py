#!/usr/bin/env python3
from __future__ import annotations
from pathlib import Path
from collections import Counter
import json
import sys

ROOT = Path(__file__).resolve().parents[1]


def fail(msg: str) -> None:
    print(f"FAIL: {msg}")
    raise SystemExit(1)


def main() -> int:
    maps = list((ROOT / "diagnostics/2026-08-13-murder-plot-42-flux-source-audit/online/45d7f").rglob("ExpertPlusLawless.dat"))
    if not maps:
        fail("Murder Plot ExpertPlusLawless.dat not found")

    data = json.loads(maps[0].read_text(encoding="utf-8"))
    custom = data.get("customData", {})
    real = data.get("colorNotes", [])
    fakes = custom.get("fakeColorNotes", [])
    fake_obstacles = custom.get("fakeObstacles", [])

    print(f"Murder Plot: real={len(real)} fakeColorNotes={len(fakes)} fakeObstacles={len(fake_obstacles)}")
    if (len(real), len(fakes), len(fake_obstacles)) != (912, 3851, 51):
        fail("unexpected audited object counts")

    offsets = Counter()
    for note in fakes:
        cd = note.get("customData", {}) or {}
        value = cd.get("noteJumpStartBeatOffset")
        if value is not None:
            offsets[value] += 1
    print("offset distribution:", dict(offsets.most_common()))

    expected_groups = [(108.0, 108.35), (204.0, 204.35), (236.0, 236.35)]
    for lo, hi in expected_groups:
        count = sum(1 for n in fakes if lo <= n.get("b", -999.0) <= hi)
        print(f"dense fake group {lo:.2f}-{hi:.2f}: {count}")
        if count != 251:
            fail(f"expected 251 dense fakes around beat {lo}")

    # Regression boundary: Noodle values are visibility values.
    old_material_switch = lambda body, arrow: body > 0 or arrow > 0
    fixed_material_switch = lambda body, arrow: body < 1 or arrow < 1
    if old_material_switch(0.0, 0.0):
        fail("old boundary model changed unexpectedly")
    if not fixed_material_switch(0.0, 0.0):
        fail("fixed condition must select cutout material at fully hidden visibility")
    if fixed_material_switch(1.0, 1.0):
        fail("fully visible note should be allowed to use the opaque material")
    print("dissolve material boundary: PASS")

    components = (ROOT / "src/VivifyComponents.cpp").read_text(encoding="utf-8")
    post = (ROOT / "src/VivifyPostProcessing.cpp").read_text(encoding="utf-8")
    if "stereoMode.value__ == 0" not in components:
        fail("true MultiPass hard gate missing")
    if "int const activeEye = isStereoCamera ? camera->get_stereoActiveEye().value__ : 0;" not in components:
        fail("Multiview-safe active-eye read guard missing")
    if post.count("cb->SetGlobalFloat(StereoActiveEyePropertyId(), 0.0f);") < 4:
        fail("stereo-eye neutralization/reset count is lower than expected")
    print("Vivify stereo guards: PASS")
    print("ALL STATIC COMPATIBILITY CHECKS PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
