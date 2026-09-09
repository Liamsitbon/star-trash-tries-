#!/usr/bin/env python3
"""Pack authored, radial RGB + depth videos into one frame-synchronous stream.

No depth estimation and no Vivify rendering are performed. Inputs are never
modified. Depth code 0=near, 1=far; input must be full-range grayscale code values.
"""
import argparse
import json
import math
from pathlib import Path
import subprocess
import tempfile
from fractions import Fraction


def probe(path):
    raw = subprocess.check_output([
        "ffprobe", "-v", "error", "-select_streams", "v:0", "-count_frames",
        "-show_entries", "stream=width,height,avg_frame_rate,r_frame_rate,duration,nb_read_frames,start_time,pix_fmt",
        "-of", "json", str(path)], timeout=600)
    streams = json.loads(raw)["streams"]
    if len(streams) != 1:
        raise ValueError("one readable video stream is required")
    s = streams[0]
    s["fps"] = float(Fraction(s["avg_frame_rate"]))
    s["duration"] = float(s["duration"])
    s["frames"] = int(s["nb_read_frames"])
    if not all(math.isfinite(s[k]) and s[k] > 0 for k in ("fps", "duration", "frames")):
        raise ValueError("invalid FPS, duration or decoded frame count")
    if abs(s["fps"] - float(Fraction(s["r_frame_rate"]))) > .001:
        raise ValueError("variable-frame-rate media must be normalized together before packing")
    if abs(float(s.get("start_time", 0))) > .001:
        raise ValueError("both input video timelines must start at zero")
    return s


def validate_pair(rgb, depth):
    for label, stream in (("RGB",rgb), ("depth",depth)):
        if stream["width"] != 2*stream["height"]:
            raise ValueError(f"{label} must be mono 2:1 equirectangular, not planar camera Z depth")
    if abs(rgb["fps"] - depth["fps"]) > .001:
        raise ValueError("RGB/depth FPS mismatch")
    if rgb["frames"] != depth["frames"] or abs(rgb["duration"]-depth["duration"]) > .5/rgb["fps"]:
        raise ValueError("RGB/depth duration or decoded frame count mismatch")


def validate_timestamps(path, expected):
    # Equal average FPS does not rule out VFR or frame offsets. Validate every
    # presentation timestamp against the shared constant-frame-rate timeline.
    raw = subprocess.check_output(["ffprobe","-v","error","-select_streams","v:0",
        "-show_entries","frame=best_effort_timestamp_time","-of","json",str(path)],timeout=600)
    frames=json.loads(raw)["frames"]
    if len(frames)!=expected["frames"]: raise ValueError("timestamp/frame count mismatch")
    for index, frame in enumerate(frames):
        timestamp=float(frame["best_effort_timestamp_time"])
        if not math.isfinite(timestamp) or abs(timestamp-index/expected["fps"])>.001:
            raise ValueError(f"non-aligned/VFR timestamp in {path.name} at frame {index}")


def pack(args):
    rgb_path, depth_path, output = map(lambda p: Path(p).resolve(), (args.rgb,args.depth,args.output))
    if output.exists() or output.with_suffix(".nexora2.json").exists():
        raise ValueError("refusing to overwrite existing output/media metadata")
    if output in (rgb_path,depth_path) or rgb_path == depth_path:
        raise ValueError("RGB, depth and output paths must be distinct")
    if output.suffix.lower() != ".mp4": raise ValueError("output must be .mp4")
    if not (2 <= args.near < args.far <= 500): raise ValueError("range must be 2 <= near < far <= 500 metres")
    rgb, depth = probe(rgb_path), probe(depth_path)
    validate_pair(rgb,depth)
    if rgb["fps"] > 120 or rgb["duration"] > 3600:
        raise ValueError("Alpha authoring limit: 120 FPS and one hour")
    validate_timestamps(rgb_path,rgb)
    validate_timestamps(depth_path,depth)
    width, height = rgb["width"], rgb["height"]
    depth_width = max(2, round(width/4/2)*2)
    if width+depth_width > 4096 or height > 2048:
        raise ValueError("Alpha packed-media budget is at most 4096x2048; resize both captures deliberately")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="nexora2-pack-",dir=output.parent) as tmp:
        target = Path(tmp)/"packed.mp4"
        # No shortest/fps filter: they can silently hide mismatches by duplicating
        # frames. Single input timestamps, equal frame counts, one decoder output.
        filters = (f"[0:v]setpts=PTS-STARTPTS,format=yuv420p[c];"
                   f"[1:v]setpts=PTS-STARTPTS,scale={depth_width}:{height}:flags=bilinear,"
                   "format=yuv420p[d];[c][d]hstack=inputs=2[v]")
        subprocess.run(["ffmpeg","-v","error","-nostdin","-n","-i",str(rgb_path),"-i",str(depth_path),
            "-filter_complex",filters,"-map","[v]","-an","-c:v","libx264","-crf","16",
            "-preset","medium","-pix_fmt","yuv420p","-movflags","+faststart",str(target)],check=True)
        actual = probe(target)
        validate_timestamps(target,actual)
        if actual["frames"] != rgb["frames"] or abs(actual["fps"]-rgb["fps"]) > .001:
            raise ValueError("packed output frame identity failed validation")
        metadata = {"schema":"nexora.rgbd/2-alpha", "media":rgb_path.name,
            "projection":"mono", "followPlayer":False,
            "rgbd":{"media":output.name,"layout":"color-left-depth-right",
                "encoding":"linear-radial-meters-code","nearMeters":args.near,"farMeters":args.far,
                "colorWidth":width/(width+depth_width),"strength":1,"worldScale":1,
                "quality":"balanced","fps":actual["fps"],"durationSeconds":actual["duration"],
                "capturePoses":[]}}
        # Exclusive create prevents clobbering a concurrently created deliverable.
        with output.open("xb") as dest, target.open("rb") as src:
            import shutil
            shutil.copyfileobj(src,dest)
        with output.with_suffix(".nexora2.json").open("x") as dest:
            json.dump(metadata,dest,indent=2); dest.write("\n")
    print(json.dumps({"packed":str(output),"frames":actual["frames"],
        "rgbFallbackMustBeIncluded":rgb_path.name,"questPerformanceProven":False},indent=2))


if __name__ == "__main__":
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("rgb"); p.add_argument("depth"); p.add_argument("output")
    p.add_argument("--near",type=float,required=True); p.add_argument("--far",type=float,required=True)
    try: pack(p.parse_args())
    except (ValueError,KeyError,subprocess.SubprocessError,OSError) as e: p.exit(1,f"Nexora RGBD: {e}\n")
