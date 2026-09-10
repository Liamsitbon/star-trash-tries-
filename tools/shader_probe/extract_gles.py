#!/usr/bin/env python3
"""Extract existing GLES stage text for an isolated driver compile/link probe.

Never edits/repackages the bundle. Output contains the map author's shader code:
keep it local unless its license permits distribution. Requires UnityPy 1.25.3.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import re
from pathlib import Path

STAGES = {"VERTEX": "vert", "FRAGMENT": "frag", "GEOMETRY": "geom"}


def stage_blocks(text: str) -> dict[str, str]:
    """Remove only Unity's outer stage selector; preserve nested preprocessor code.

    In particular, never prepend a #define before GLSL's required #version line.
    Refuse unfamiliar wrappers instead of rewriting the shader to make it pass.
    """
    result: dict[str, str] = {}
    stage = None
    depth = 0
    body: list[str] = []
    for line in text.rstrip("\x00").splitlines(keepends=True):
        directive = re.match(r"\s*#\s*(\w+)\b(.*)", line)
        op, args = directive.groups() if directive else ("", "")
        if stage is None:
            if not line.strip():
                continue
            if op != "ifdef" or args.strip() not in STAGES:
                raise ValueError("unrecognized outer stage wrapper")
            stage = args.strip()
            depth = 1
            body = []
            continue
        if op in ("if", "ifdef", "ifndef"):
            depth += 1
        elif op == "endif":
            depth -= 1
        elif op in ("else", "elif") and depth == 1:
            raise ValueError("outer stage else is not supported")
        if depth == 0:
            source = "".join(body).lstrip()
            if not source.startswith("#version ") or stage in result:
                raise ValueError("missing version or duplicate stage")
            result[stage] = source
            stage = None
        else:
            body.append(line)
    if stage is not None:
        raise ValueError("unterminated stage")
    if not {"VERTEX", "FRAGMENT"}.issubset(result):
        raise ValueError("unpaired stages: cannot claim a complete link test")
    return result


def sha(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def scalar(v):
    while isinstance(v, list):
        v = v[0]
    return int(v)


def export(bundle: Path, output: Path) -> dict:
    import UnityPy
    from UnityPy.export import ShaderConverter
    if output.exists():
        raise ValueError("output must be a NEW directory; existing data is never overwritten")
    if not bundle.is_file() or bundle.stat().st_size > 1024 * 1024 * 1024:
        raise ValueError("bundle must be a file no larger than 1 GiB")
    report = {"bundleSha256": sha(bundle), "unityPyVersion": UnityPy.__version__,
              "proof": "extracted source only; not compiled on Quest", "shaderCount": 0,
              "programs": [], "skipped": []}
    env = UnityPy.load(str(bundle))
    collected = {}
    for obj in env.objects:
        if obj.type.name != "Shader":
            continue
        report["shaderCount"] += 1
        shader = obj.read()
        name = shader.m_ParsedForm.m_Name
        for p, platform in enumerate(shader.platforms):
            if int(platform) != 9:  # Unity ShaderCompilerPlatform.GLES3Plus
                continue
            try:
                offset, length, size = (scalar(field[p]) for field in
                                        (shader.offsets, shader.compressedLengths, shader.decompressedLengths))
                if size > 64 * 1024 * 1024:
                    raise ValueError("shader decompressed program limit")
                data = ShaderConverter.CompressionHelper.decompress_lz4(bytes(shader.compressedBlob)[offset:offset+length], size)
                program = ShaderConverter.ShaderProgram(ShaderConverter.EndianBinaryReader(data, endian="<"), obj.version)
            except Exception as exc:
                report["skipped"].append({"shader": name, "reason": type(exc).__name__ + ": " + str(exc)[:160]})
                continue
            for index, sub in enumerate(program.m_SubPrograms):
                code = bytes(sub.m_ProgramCode or b"")
                if b"#version" not in code:  # Binary reflection records are NOT shader text.
                    continue
                if len(code) > 1024 * 1024:
                    raise ValueError("individual GLSL program exceeds 1 MiB")
                try:
                    stages = stage_blocks(code.decode("utf-8"))
                except (ValueError, UnicodeError) as exc:
                    report["skipped"].append({"shader": name, "blob": index, "reason": str(exc)})
                    continue
                digest = hashlib.sha256(code).hexdigest()
                if digest not in collected:
                    if len(collected) >= 4096:
                        raise ValueError("too many unique programs")
                    collected[digest] = {"stages": stages, "origins": []}
                collected[digest]["origins"].append({"shader": name, "blob": index, "pathId": obj.path_id})
    output.mkdir(parents=True)
    manifest = []
    for index, (digest, data) in enumerate(collected.items()):
        label = f"p{index:04d}"
        files = {}
        for stage, text in data["stages"].items():
            filename = f"{label}.{STAGES[stage]}"
            (output / filename).write_text(text, encoding="utf-8")
            files[stage] = filename
        manifest.append(f"{label} {files['VERTEX']} {files['FRAGMENT']} {files.get('GEOMETRY', '-')}\n")
        all_code = "\n".join(data["stages"].values())
        report["programs"].append({"id": label, "compiledBlobSha256": digest, "origins": data["origins"],
            "files": files, "multiview": "GL_OVR_multiview2" in all_code,
            "eyeIndex": "gl_ViewID_OVR" in all_code, "geometry": "GEOMETRY" in files,
            "textureArray": "sampler2DArray" in all_code, "depthTexture": "_CameraDepthTexture" in all_code})
    (output / "manifest.txt").write_text("".join(manifest), encoding="ascii")
    (output / "extraction.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    result = export(args.bundle, args.output)
    print(json.dumps({"shaders": result["shaderCount"], "programs": len(result["programs"]),
                      "multiview": sum(p["multiview"] for p in result["programs"]),
                      "geometry": sum(p["geometry"] for p in result["programs"]),
                      "skipped": len(result["skipped"]), "proof": result["proof"]}, indent=2))
