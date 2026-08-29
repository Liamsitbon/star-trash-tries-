#!/usr/bin/env python3
"""Create decoder-conscious Nexora mono, OU or SBS equirectangular MP4 media."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
from pathlib import Path


# Dimensions are per eye. Packing derives the final frame shape so stereo is
# not silently stretched into the mono 2:1 layout.
PROFILES = {
    "quest2-safe": {
        "eye": (2048, 1024), "fps": 60, "codec": "libx264",
        "bitrate": "18M", "maxrate": "24M",
    },
    "quest-pro-balanced": {
        "eye": (2880, 1440), "fps": 60, "codec": "libx265",
        "bitrate": "28M", "maxrate": "38M",
    },
    "quest3-quality": {
        "eye": (3840, 1920), "fps": 60, "codec": "libx265",
        "bitrate": "42M", "maxrate": "55M",
    },
    "quest3-120-experimental": {
        "eye": (3840, 1920), "fps": 120, "codec": "libx265",
        "bitrate": "70M", "maxrate": "90M",
    },
}


def packed_size(per_eye: tuple[int, int], projection: str) -> tuple[int, int]:
    width, height = per_eye
    if projection == "ou":
        return width, height * 2
    if projection == "sbs":
        return width * 2, height
    return width, height


def expected_aspect(projection: str) -> float:
    return {"mono": 2.0, "ou": 1.0, "sbs": 4.0}[projection]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--profile", choices=PROFILES, default="quest2-safe")
    parser.add_argument("--projection", choices=("mono", "ou", "sbs"), default="mono")
    parser.add_argument(
        "--mbf-root-copy", action="store_true",
        help="also copy the output to the map root when output is under Nexora/Media",
    )
    parser.add_argument("--print-only", action="store_true")
    args = parser.parse_args()
    if not args.input.is_file():
        parser.error(f"input does not exist: {args.input}")
    ffmpeg = shutil.which("ffmpeg")
    ffprobe = shutil.which("ffprobe")
    if not ffmpeg or not ffprobe:
        parser.error("ffmpeg and ffprobe are required")

    probe = subprocess.run(
        [
            ffprobe, "-v", "error", "-select_streams", "v:0", "-show_entries",
            "stream=width,height,codec_name,pix_fmt,r_frame_rate", "-of", "json",
            str(args.input),
        ],
        capture_output=True, text=True, check=False,
    )
    try:
        streams = json.loads(probe.stdout).get("streams", []) if probe.returncode == 0 else []
    except json.JSONDecodeError:
        streams = []
    if not streams:
        parser.error("input does not contain a readable video stream")
    source = streams[0]
    width, height = int(source.get("width") or 0), int(source.get("height") or 0)
    if width < 16 or height < 16:
        parser.error("input video dimensions are invalid")

    profile = PROFILES[args.profile]
    output_width, output_height = packed_size(profile["eye"], args.projection)
    codec_args = ["-c:v", profile["codec"]]
    if profile["codec"] == "libx264":
        codec_args += ["-profile:v", "high", "-preset", "slow"]
    else:
        codec_args += [
            "-preset", "slow", "-tag:v", "hvc1", "-x265-params", "repeat-headers=1"
        ]
    command = [
        ffmpeg, "-hide_banner", "-y", "-i", str(args.input),
        "-map", "0:v:0", "-an",
        "-vf", f"scale={output_width}:{output_height}:flags=lanczos,fps={profile['fps']}",
        *codec_args, "-pix_fmt", "yuv420p", "-b:v", profile["bitrate"],
        "-maxrate", profile["maxrate"], "-bufsize", profile["maxrate"],
        "-g", str(profile["fps"] * 2), "-movflags", "+faststart", str(args.output),
    ]

    actual_aspect = width / height
    wanted_aspect = expected_aspect(args.projection)
    print(f"Profile: {args.profile}; projection: {args.projection}")
    print(
        f"Source: {width}x{height} {source.get('codec_name', 'unknown')} "
        f"{source.get('pix_fmt', 'unknown')}"
    )
    print(f"Output: {output_width}x{output_height} ({args.projection})")
    if abs(actual_aspect - wanted_aspect) > wanted_aspect * 0.08:
        print(
            "WARNING: source aspect does not match the selected packing "
            f"(expected about {wanted_aspect:.2f}:1); conversion may distort the sphere."
        )
    if args.projection != "mono":
        print(
            "WARNING: stereo doubles decoded pixels versus the per-eye profile; "
            "validate decoder stability on the exact target Quest."
        )
    print("WARNING: performance remains device/map-specific and needs a headset test.")
    print("Command:", " ".join(command))
    if args.print_only:
        return 0

    args.output.parent.mkdir(parents=True, exist_ok=True)
    result = subprocess.run(command, check=False)
    if result.returncode != 0:
        return result.returncode

    if args.mbf_root_copy:
        output = args.output.resolve()
        if output.parent.name.casefold() != "media" or output.parent.parent.name.casefold() != "nexora":
            parser.error("--mbf-root-copy requires an output under <map>/Nexora/Media/")
        map_root = output.parents[2]
        root_copy = map_root / output.name
        shutil.copy2(output, root_copy)
        print(f"MBF root duplicate: {root_copy}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
