#!/usr/bin/env python3
"""Read-only shader program/material inventory; never patch or rebuild a bundle.

Only metadata/counts are exported, not the author's shader source. Nonempty
programs are NOT proof of driver compilation or rendering. Requires UnityPy 1.25.3.
"""
import argparse
import json
from pathlib import Path
import struct
from extract_gles import sha


def blocks(value):
    return list(value) if isinstance(value, (list, tuple)) else [value]


def inventory(bundle, shader_name=None):
    import UnityPy
    from UnityPy.export import ShaderConverter
    if not bundle.is_file() or bundle.stat().st_size > 1024 ** 3:
        raise ValueError("Expected an existing bundle no larger than 1 GiB")
    report = {"schema": "vivify.shader-program-inventory/1", "bundleSha256": sha(bundle),
              "unityPyVersion": UnityPy.__version__, "questRuntimeVerified": False,
              "shaders": [], "materials": [], "renderers": [], "errors": []}
    env = UnityPy.load(str(bundle))
    selected = set()
    for obj in env.objects:
        if obj.type.name != "Shader":
            continue
        shader = obj.read()
        name = shader.m_ParsedForm.m_Name
        if shader_name and name != shader_name:
            continue
        selected.add((id(obj.assets_file), obj.path_id))
        record = {"name": name, "pathId": obj.path_id, "unityVersion": str(obj.version),
                  "platforms": [], "passes": []}
        for subshader in shader.m_ParsedForm.m_SubShaders:
            for p in subshader.m_Passes:
                record["passes"].append({"name": p.m_Name,
                    "vertexVariants": len(p.progVertex.m_SubPrograms),
                    "fragmentVariants": len(p.progFragment.m_SubPrograms)})
        for platform_index, platform in enumerate(shader.platforms):
            item = {"platform": int(platform), "segments": []}
            record["platforms"].append(item)
            offsets, lengths, sizes = [blocks(v[platform_index]) for v in
                (shader.offsets, shader.compressedLengths, shader.decompressedLengths)]
            if not (len(offsets) == len(lengths) == len(sizes)):
                raise ValueError("Inconsistent program segment layout")
            # Segment zero begins with Unity's subprogram count. Later segments
            # may contain bytecode payload rather than another header: never
            # reinterpret those arbitrary bytes as a new count.
            for segment, (offset, length, size) in enumerate(zip(offsets, lengths, sizes)):
                entry = {"index": segment, "compressedBytes": length, "decodedBytes": size}
                item["segments"].append(entry)
                if segment != 0:
                    continue
                blob = bytes(shader.compressedBlob)
                if offset < 0 or length < 0 or offset + length > len(blob) or size < 4 or size > 64 * 1024 ** 2:
                    report["errors"].append({"shader": name, "platform": int(platform), "reason": "Invalid/oversized first program segment"})
                    continue
                data = ShaderConverter.CompressionHelper.decompress_lz4(blob[offset:offset+length], size)
                if len(data) != size:
                    raise ValueError("Decoded shader segment size mismatch")
                entry["subprogramCount"] = struct.unpack_from("<I", data)[0]
                entry["emptyProgramHeader"] = size == 4 and data == b"\x00\x00\x00\x00"
        report["shaders"].append(record)
    selected_materials = set()
    for obj in env.objects:
        if obj.type.name != "Material":
            continue
        material = obj.read()
        try:
            pointer = material.m_Shader.deref()
            if (id(pointer.assets_file), pointer.path_id) in selected:
                selected_materials.add((id(obj.assets_file), obj.path_id))
                report["materials"].append({"name": material.m_Name, "pathId": obj.path_id,
                                            "shaderPathId": pointer.path_id})
        except (ValueError, KeyError, AttributeError):
            report["errors"].append({"materialPathId": obj.path_id, "reason": "Unresolved shader pointer"})
    for obj in env.objects:
        if obj.type.name not in ("MeshRenderer", "SkinnedMeshRenderer", "ParticleSystemRenderer"):
            continue
        renderer = obj.read()
        matches = []
        for reference in renderer.m_Materials:
            try:
                pointer = reference.deref()
                if (id(pointer.assets_file), pointer.path_id) in selected_materials:
                    matches.append(pointer.path_id)
            except (ValueError, KeyError, AttributeError):
                continue
        if matches:
            report["renderers"].append({"pathId": obj.path_id, "type": obj.type.name,
                "gameObject": renderer.m_GameObject.deref_parse_as_object().m_Name,
                "enabled": renderer.m_Enabled, "materialPathIds": matches})
    if shader_name and not report["shaders"]:
        raise ValueError("Requested exact shader name not found")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--shader")
    args = parser.parse_args()
    result = inventory(args.bundle, args.shader)
    with args.output.open("x", encoding="utf-8") as stream:
        json.dump(result, stream, indent=2)
        stream.write("\n")
    print(json.dumps({"shaders": len(result["shaders"]), "materials": len(result["materials"]),
                      "errors": len(result["errors"]), "bundleSha256": result["bundleSha256"]}))
