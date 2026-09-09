#!/usr/bin/env python3
"""Summarize a Nexora pause probe; never install mods or change Quest settings."""

import argparse
import json
from pathlib import Path
import re
import subprocess

LOG_PATH = "/sdcard/ModData/com.beatgames.beatsaber/Logs/Nexora.log"
PIXELS = re.compile(
    r"Nexora video diagnostic pixels dome='([^']+)' samples=(\d+) "
    r"minRGB=\((\d+),(\d+),(\d+)\) maxRGB=\((\d+),(\d+),(\d+)\)"
)
BINDING_IDS = ("target", "playerTarget", "materialTexture", "material", "rendererMaterial")


def summarize(text):
    reports = []
    bindings = {}
    for line in text.splitlines():
        if "Nexora video diagnostic binding " in line:
            name = re.search(r"dome='([^']+)'", line)
            if name:
                bindings[name[1]] = dict(re.findall(r"\b(\w+)=(-?\d+)\b", line))
        match = PIXELS.search(line)
        if not match:
            continue
        name = match[1]
        minimum = [int(value) for value in match.group(3, 4, 5)]
        maximum = [int(value) for value in match.group(6, 7, 8)]
        ids = {key: int(bindings.get(name, {}).get(key, 0)) for key in BINDING_IDS}
        consistent = (
            ids["target"] != 0
            and ids["target"] == ids["playerTarget"] == ids["materialTexture"]
            and ids["material"] != 0
            and ids["material"] == ids["rendererMaterial"]
        )
        if min(minimum) >= 250:
            appearance = "near_white_samples_compare_source_frame"
        elif max(maximum) <= 5:
            appearance = "near_black_samples_compare_source_frame"
        elif max(high - low for low, high in zip(minimum, maximum)) > 5:
            appearance = "varied_samples_compare_with_headset_view"
        else:
            appearance = "nearly_uniform_samples_compare_source_frame"
        reports.append({
            "dome": name, "samples": int(match[2]),
            "min_rgb": minimum, "max_rgb": maximum,
            "bindings_consistent": consistent, "binding_ids": ids,
            "observation": appearance,
        })
    return {
        "probe_status": "samples_available" if reports else "no_pixel_evidence",
        "reports": reports,
        "runtime_fix_proven": False,
        "note": "Samples do not describe the entire frame or prove correct headset rendering.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    inputs = parser.add_mutually_exclusive_group(required=True)
    inputs.add_argument("--log", type=Path, help="Previously collected Nexora.log")
    inputs.add_argument("--adb", metavar="EXECUTABLE", help="Read current Quest log through this adb executable")
    args = parser.parse_args()
    if args.log:
        content = args.log.read_text(encoding="utf-8", errors="replace")
    else:
        result = subprocess.run(
            [args.adb, "exec-out", "cat", LOG_PATH], check=True,
            capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=20,
        )
        content = result.stdout
    print(json.dumps(summarize(content), indent=2))


if __name__ == "__main__":
    main()
