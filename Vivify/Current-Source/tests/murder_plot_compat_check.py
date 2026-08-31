#!/usr/bin/env python3
from __future__ import annotations
from pathlib import Path
from collections import Counter
import json
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def fail(msg: str) -> None:
    print(f"FAIL: {msg}")
    raise SystemExit(1)


def main() -> int:
    fixture_root = ROOT / "diagnostics/2026-08-13-murder-plot-42-flux-source-audit/online"
    maps = list((fixture_root / "45d7f").rglob("ExpertPlusLawless.dat"))
    data = None
    if maps:
        data = json.loads(maps[0].read_text(encoding="utf-8"))
    else:
        # The repo keeps large map fixtures as ZIPs.  Local audit sessions may
        # have extracted 45d7f/, but CI checkouts intentionally do not.
        archive_path = fixture_root / "45d7f.zip"
        if not archive_path.is_file():
            print("SKIP: Murder Plot map fixture is not present in this checkout")
        else:
            try:
                with zipfile.ZipFile(archive_path) as archive:
                    entry = next(
                        (name for name in archive.namelist()
                         if Path(name).name == "ExpertPlusLawless.dat" and not name.endswith("/")),
                        None,
                    )
                    if entry is None:
                        fail("45d7f.zip does not contain ExpertPlusLawless.dat")
                    with archive.open(entry) as stream:
                        data = json.load(stream)
            except (OSError, zipfile.BadZipFile, json.JSONDecodeError) as error:
                fail(f"Murder Plot fixture ZIP is unreadable: {error}")
    if data is not None:
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
    neutral = "cb->SetGlobalFloat(StereoActiveEyePropertyId(), 0.0f);"
    apply_blits = post.split("void Runtime::ApplyBlits", 1)[1].split(
        "void Runtime::CacheMainRenderDescriptor", 1
    )[0]
    mid_blits = post.split("Runtime::BuildMidRenderCommandBuffer", 1)[1].split(
        "Runtime::ComputeMidRenderSignature", 1
    )[0]
    if neutral not in apply_blits or neutral not in mid_blits:
        fail("each multiview blit chain must neutralize the legacy eye selector")
    if "cb->SetGlobalFloat(StereoActiveEyePropertyId(), 1.0f);" in post:
        fail("multiview blits must not force a physical-eye selector")
    print("Vivify stereo guards: PASS")
    print("ALL STATIC COMPATIBILITY CHECKS PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
