#!/usr/bin/env python3
"""Create decoder-conscious Nexora mono, OU or SBS equirectangular MP4 media."""

from __future__ import annotations

import argparse
from fractions import Fraction
import json
import os
import shlex
import shutil
import subprocess
from pathlib import Path


# Packed output sizes are explicit because decoder limits apply to the complete
# frame, not to an abstract per-eye resolution. The default stays under the
# conservative 4096-pixel AVC boundary. HEVC profiles require an explicit opt-in
# because the exact decoder ceiling is headset- and firmware-dependent.
PROFILES = {
    "quest2-safe": {
        "sizes": {
            "mono": (2048, 1024),
            "ou": (1920, 1920),
            "sbs": (3840, 960),
        },
        "fps": 60, "codec": "libx264", "decodedCodec": "h264",
        "bitrate": "18M", "maxrate": "24M", "experimental": False,
    },
    "quest-hevc-balanced": {
        "sizes": {
            "mono": (3840, 1920),
            "ou": (2880, 2880),
            "sbs": (5760, 1440),
        },
        "fps": 60, "codec": "libx265", "decodedCodec": "hevc",
        "bitrate": "34M", "maxrate": "48M", "experimental": True,
    },
    "quest3-hevc-quality": {
        "sizes": {
            "mono": (5760, 2880),
            "ou": (4320, 4320),
            "sbs": (5760, 1440),
        },
        "fps": 60, "codec": "libx265", "decodedCodec": "hevc",
        "bitrate": "52M", "maxrate": "70M", "experimental": True,
    },
}


def expected_aspect(projection: str) -> float:
    return {"mono": 2.0, "ou": 1.0, "sbs": 4.0}[projection]


def probe_video(ffprobe: str, path: Path) -> dict[str, object]:
    result = subprocess.run(
        [
            ffprobe, "-v", "error", "-select_streams", "v:0", "-show_entries",
            "stream=width,height,codec_name,pix_fmt,r_frame_rate,avg_frame_rate",
            "-of", "json", str(path),
        ],
        capture_output=True, text=True, check=False,
    )
    try:
        streams = json.loads(result.stdout).get("streams", []) if result.returncode == 0 else []
    except json.JSONDecodeError:
        streams = []
    if not streams:
        raise ValueError(f"{path} does not contain a readable video stream")
    return streams[0]


def frame_rate(stream: dict[str, object]) -> float:
    value = str(stream.get("avg_frame_rate") or stream.get("r_frame_rate") or "0/1")
    try:
        rate = float(Fraction(value))
    except (ValueError, ZeroDivisionError):
        return 0.0
    return rate if rate > 0.0 else 0.0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--profile", choices=PROFILES, default="quest2-safe")
    parser.add_argument("--projection", choices=("mono", "ou", "sbs"), default="mono")
    parser.add_argument(
        "--allow-experimental", action="store_true",
        help="allow a HEVC profile that still requires exact-headset decoder testing",
    )
    parser.add_argument(
        "--allow-aspect-mismatch", action="store_true",
        help="allow rescaling input whose packing does not match the selected projection",
    )
    parser.add_argument(
        "--mbf-root-copy", action="store_true",
        help="also copy the output to the map root when output is under Nexora/Media",
    )
    parser.add_argument("--print-only", action="store_true")
    args = parser.parse_args()
    if not args.input.is_file():
        parser.error(f"input does not exist: {args.input}")
    if args.output.suffix.casefold() != ".mp4":
        parser.error("output must use the .mp4 extension")
    if args.input.resolve() == args.output.resolve():
        parser.error("input and output must be different files")
    if args.mbf_root_copy:
        resolved_output = args.output.resolve()
        if (
            resolved_output.parent.name.casefold() != "media"
            or resolved_output.parent.parent.name.casefold() != "nexora"
        ):
            parser.error(
                "--mbf-root-copy requires an output under <map>/Nexora/Media/"
            )
    ffmpeg = shutil.which("ffmpeg")
    ffprobe = shutil.which("ffprobe")
    if not ffmpeg or not ffprobe:
        parser.error("ffmpeg and ffprobe are required")

    try:
        source = probe_video(ffprobe, args.input)
    except ValueError as error:
        parser.error(str(error))
    width, height = int(source.get("width") or 0), int(source.get("height") or 0)
    if width < 16 or height < 16:
        parser.error("input video dimensions are invalid")

    profile = PROFILES[args.profile]
    if profile["experimental"] and not args.allow_experimental:
        parser.error(
            f"{args.profile} is device-dependent; pass --allow-experimental only "
            "after deciding to test it on the exact target Quest"
        )
    output_width, output_height = profile["sizes"][args.projection]
    codec_args = ["-c:v", profile["codec"]]
    if profile["codec"] == "libx264":
        codec_args += ["-profile:v", "high", "-preset", "slow"]
    else:
        codec_args += [
            "-preset", "slow", "-tag:v", "hvc1", "-x265-params", "repeat-headers=1"
        ]
    actual_aspect = width / height
    wanted_aspect = expected_aspect(args.projection)
    if (
        abs(actual_aspect - wanted_aspect) > wanted_aspect * 0.08
        and not args.allow_aspect_mismatch
    ):
        parser.error(
            "source aspect does not match the selected packing "
            f"(got {actual_aspect:.3f}:1, expected about {wanted_aspect:.3f}:1); "
            "choose the correct projection or explicitly allow distortion"
        )

    temporary = args.output.with_name(
        f".{args.output.stem}.nexora-partial-{os.getpid()}{args.output.suffix}"
    )
    command_output = args.output if args.print_only else temporary
    command = [
        ffmpeg, "-hide_banner", "-y", "-i", str(args.input),
        "-map", "0:v:0", "-an", "-sn", "-dn", "-map_metadata", "-1",
        "-vf", f"scale={output_width}:{output_height}:flags=lanczos,fps={profile['fps']}",
        *codec_args, "-pix_fmt", "yuv420p", "-b:v", profile["bitrate"],
        "-maxrate", profile["maxrate"], "-bufsize", profile["maxrate"],
        "-g", str(profile["fps"] * 2), "-movflags", "+faststart", str(command_output),
    ]

    print(f"Profile: {args.profile}; projection: {args.projection}")
    print(
        f"Source: {width}x{height} {source.get('codec_name', 'unknown')} "
        f"{source.get('pix_fmt', 'unknown')}"
    )
    print(f"Output: {output_width}x{output_height} ({args.projection})")
    if profile["experimental"]:
        print("WARNING: this HEVC profile is an explicit exact-device experiment.")
    print("WARNING: performance remains device/map-specific and needs a headset test.")
    print("Command:", shlex.join(command))
    if args.print_only:
        return 0

    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary.unlink(missing_ok=True)
    try:
        result = subprocess.run(command, check=False)
        if result.returncode != 0:
            return result.returncode
        try:
            encoded = probe_video(ffprobe, temporary)
        except ValueError as error:
            parser.error(str(error))
        encoded_width = int(encoded.get("width") or 0)
        encoded_height = int(encoded.get("height") or 0)
        encoded_rate = frame_rate(encoded)
        if (encoded_width, encoded_height) != (output_width, output_height):
            parser.error(
                f"encoded dimensions are {encoded_width}x{encoded_height}, expected "
                f"{output_width}x{output_height}"
            )
        if encoded.get("codec_name") != profile["decodedCodec"]:
            parser.error(
                f"encoded codec is {encoded.get('codec_name')}, expected "
                f"{profile['decodedCodec']}"
            )
        if encoded.get("pix_fmt") != "yuv420p":
            parser.error(f"encoded pixel format is {encoded.get('pix_fmt')}, expected yuv420p")
        if abs(encoded_rate - profile["fps"]) > 0.05:
            parser.error(
                f"encoded frame rate is {encoded_rate:.3f}, expected {profile['fps']}"
            )
        if temporary.stat().st_size < 1024:
            parser.error("encoded MP4 is implausibly small")
        os.replace(temporary, args.output)
    finally:
        temporary.unlink(missing_ok=True)

    if args.mbf_root_copy:
        output = args.output.resolve()
        map_root = output.parents[2]
        root_copy = map_root / output.name
        shutil.copy2(output, root_copy)
        print(f"MBF root duplicate: {root_copy}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
