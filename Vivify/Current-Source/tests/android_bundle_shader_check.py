#!/usr/bin/env python3
"""Compare a Quest Vivify bundle with its desktop reference bundle.

UnityPy is intentionally an audit-only dependency. The mod does not ship it.
"""

from __future__ import annotations

import argparse
from collections import Counter
from pathlib import Path

try:
    import UnityPy
except ImportError as exc:
    raise SystemExit("FAIL: install UnityPy in an audit environment") from exc


MOBILE_PLATFORMS = {9, 18}  # GLES3Plus and Vulkan in Unity 2021.
STEREO_KEYWORDS = {"STEREO_MULTIVIEW_ON", "STEREO_INSTANCING_ON"}


def inspect(path: Path) -> tuple[Counter[str], dict[str, tuple[set[int], set[str]]]]:
    if not path.is_file():
        raise SystemExit(f"FAIL: bundle is missing: {path}")
    environment = UnityPy.load(str(path))
    object_counts: Counter[str] = Counter(obj.type.name for obj in environment.objects)
    shaders: dict[str, tuple[set[int], set[str]]] = {}
    for obj in environment.objects:
        if obj.type.name != "Shader":
            continue
        tree = obj.read_typetree()
        parsed = tree["m_ParsedForm"]
        name = parsed["m_Name"]
        if not name:
            raise SystemExit(f"FAIL: unnamed shader in {path}")
        shaders[name] = (set(tree.get("platforms", [])), set(parsed.get("m_KeywordNames", [])))
    return object_counts, shaders


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--android", required=True, type=Path)
    parser.add_argument("--desktop", required=True, type=Path)
    args = parser.parse_args()

    android_counts, android_shaders = inspect(args.android)
    desktop_counts, desktop_shaders = inspect(args.desktop)
    if android_counts != desktop_counts:
        missing = desktop_counts - android_counts
        extra = android_counts - desktop_counts
        raise SystemExit(f"FAIL: Android object inventory differs: missing={missing}, extra={extra}")
    if android_shaders.keys() != desktop_shaders.keys():
        missing = sorted(desktop_shaders.keys() - android_shaders.keys())
        extra = sorted(android_shaders.keys() - desktop_shaders.keys())
        raise SystemExit(f"FAIL: Android shader inventory differs: missing={missing}, extra={extra}")

    for name, (platforms, keywords) in sorted(android_shaders.items()):
        if not MOBILE_PLATFORMS.issubset(platforms):
            raise SystemExit(f"FAIL: {name} lacks GLES3/Vulkan programs: {sorted(platforms)}")
        absent = STEREO_KEYWORDS - keywords
        if absent:
            raise SystemExit(f"FAIL: {name} lacks Quest stereo variants: {sorted(absent)}")

    print(
        "Android Vivify bundle shader contract: PASS "
        f"({len(android_shaders)} shaders; {sum(android_counts.values())} objects; "
        "GLES3+Vulkan; Multiview+Instancing variants)"
    )


if __name__ == "__main__":
    main()
