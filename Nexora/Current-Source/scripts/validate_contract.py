#!/usr/bin/env python3
"""Validate Nexora's Quest-only runtime, metadata, shader and event contract."""

from __future__ import annotations

import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
QPM_CONFIG_VERSION = "0.4.0"
MOD_VERSION = "0.3.0"
TARGET = "1.40.8_7379"


def load_json(relative: str) -> dict:
    return json.loads((ROOT / relative).read_text(encoding="utf-8"))


def fail(message: str) -> None:
    raise SystemExit(f"Nexora contract validation failed: {message}")


def dependency_versions(items: list[dict], key: str) -> dict[str, str]:
    result: dict[str, str] = {}
    for item in items:
        identifier = item.get("id")
        version = item.get(key)
        if identifier and version:
            result[identifier] = version
    return result


def main() -> int:
    header = (ROOT / "include/NexoraRuntime.hpp").read_text(encoding="utf-8")
    runtime = (ROOT / "src/NexoraRuntime.cpp").read_text(encoding="utf-8")
    runtime_folded = runtime.casefold()
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8").casefold()
    components = (ROOT / "include/NexoraComponents.hpp").read_text(encoding="utf-8")
    shader = (ROOT / "unity/Assets/Nexora/Shaders/NexoraDome.shader").read_text(
        encoding="utf-8"
    )
    builder = (ROOT / "unity/Assets/Nexora/Editor/BuildNexoraAssets.cs").read_text(
        encoding="utf-8"
    )

    known_events = set(re.findall(r'"(Nexora\.[A-Za-z0-9]+)"', header))
    example = load_json("examples/NexoraEvents.v3.json")
    used_events = {item["t"] for item in example["customEvents"]}
    unknown = sorted(used_events - known_events)
    if unknown:
        fail(f"example uses unknown events: {unknown}")
    if example.get("requirements") != ["Nexora"]:
        fail("example does not declare the exact Nexora capability")

    manifest = load_json("mod.json")
    qpm = load_json("qpm.json")
    shared = load_json("qpm.shared.json")
    if manifest.get("id") != "nexora" or manifest.get("packageVersion") != TARGET:
        fail("mod identity or Beat Saber target changed unexpectedly")
    if manifest.get("version") != MOD_VERSION:
        fail("mod.json version is inconsistent")
    if qpm.get("version") != QPM_CONFIG_VERSION:
        fail("qpm.json version must describe QPM format 0.4.0, not the mod")
    if shared.get("config", {}).get("version") != QPM_CONFIG_VERSION:
        fail("qpm.shared.json config version must be 0.4.0")
    info_versions = {
        qpm.get("info", {}).get("version"),
        shared.get("config", {}).get("info", {}).get("version"),
    }
    if info_versions != {MOD_VERSION}:
        fail(f"QPM mod version mismatch: {sorted(str(v) for v in info_versions)}")

    manifest_dependencies = dependency_versions(manifest.get("dependencies", []), "version")
    qpm_dependencies = dependency_versions(qpm.get("dependencies", []), "versionRange")
    for identifier in ("beatsaber-hook", "custom-types", "custom-json-data", "songcore"):
        if manifest_dependencies.get(identifier) != qpm_dependencies.get(identifier):
            fail(f"dependency range mismatch for {identifier}")

    copies = {item["name"]: item["destination"] for item in manifest.get("fileCopies", [])}
    expected_asset_path = (
        "/sdcard/ModData/com.beatgames.beatsaber/Mods/Nexora/Assets/"
        "nexoraassets.android"
    )
    if copies.get("nexoraassets.android") != expected_asset_path:
        fail("asset bundle install destination disagrees with runtime")

    forbidden_runtime_markers = (
        "bundleandroid2021.vivify",
        "mods/nexora/media",
        'readstring(json, "url")',
        "rendercameraeffect(",
        "graphics::blit(",
    )
    for marker in forbidden_runtime_markers:
        if marker in runtime_folded or marker in components.casefold():
            fail(f"Quest-only runtime contains forbidden marker: {marker}")
    if "declare_instance_method(void, onrenderimage" in components.casefold():
        fail("Quest component still declares the desktop framebuffer callback")
    for marker in (
        "getlevelwasselectedevent",
        "runtime.loadassets()",
        "questshaderassetfailure",
        "asset file exists, but unity could not load",
        "_selectedmaproot",
        "weakly_canonical",
        "ispathinside",
        "createproceduraldomemesh",
        "ensurequestsafecameraeffects",
        "tryprepareselectedbeatmapfromscene",
        "replaymissedevents",
        "if (!isnexoraevent && !_selectedmaprequiresnexora) return;",
        "containsnexoraevents",
        "songcore did not report its requirement",
        "beatmapcallbacksupdater",
        "unityengine::video::videoplayer",
        "videorendermode::materialoverride",
        "set_targetmaterialrenderer",
        'set_targetmaterialproperty(stringw("_maintex"))',
        "set_waitforfirstframe(true)",
        "set_sendframereadyevents(true)",
        "onvideoframeready",
        "safetyvisible",
        "s_propvideoready",
        "safety backdrop remains visible",
        "androidvideomedia",
        "nexora/media",
        "questmodinterop::inspect",
        "iscapabilityregistered(kcapability)",
        "unregistercapability(kcapability)",
        "if (!getnexoraenabled())",
    ):
        if marker not in runtime_folded:
            fail(f"map-local Quest contract is missing runtime marker: {marker}")

    retired_backend_markers = (
        "questnativevideo",
        "surfacetexture",
        "gl_texture_external_oes",
        "createexternaltexture",
        "issuepluginevent",
        "android/media/mediaplayer",
    )
    for marker in retired_backend_markers:
        if marker in runtime_folded or marker in header.casefold() or marker in cmake:
            fail(f"retired cross-API video backend is still reachable: {marker}")
    if "-lglesv3" in cmake:
        fail("Nexora still links the retired OpenGL ES video bridge")
    for marker in ("ffmpeg", "libavcodec", "exoplayer"):
        if marker in runtime_folded or marker in cmake:
            fail(f"runtime contains an unapproved codec/player stack: {marker}")

    for asset_type, field in (
        ("UnityEngine::AssetBundle", "_assetBundle"),
        ("UnityEngine::Material", "_domeTemplate"),
        ("UnityEngine::Shader", "_domeShader"),
    ):
        safe_declaration = f"SafePtrUnity<{asset_type}> {field};"
        raw_declaration = f"{asset_type}* {field}"
        if safe_declaration not in header or raw_declaration in header:
            fail(f"scene-persistent Unity asset is not GC-rooted safely: {field}")

    selection_start = runtime.find("GetLevelWasSelectedEvent")
    selection_end = runtime.find("Nexora runtime ready", selection_start)
    selection_callback = runtime[selection_start:selection_end]
    requirement_guard = selection_callback.find("if (!requiresNexora)")
    ordinary_enable = selection_callback.find("EnablePlayButton", requirement_guard)
    asset_retry = selection_callback.find("runtime.LoadAssets();")
    shader_check = selection_callback.find("runtime.HasQuestShaderAssets()", asset_retry)
    required_enable = selection_callback.find("EnablePlayButton", shader_check)
    if (
        requirement_guard < 0
        or ordinary_enable < 0
        or asset_retry < 0
        or shader_check < 0
        or required_enable < 0
        or requirement_guard > asset_retry
        or ordinary_enable > asset_retry
        or asset_retry > shader_check
        or shader_check > required_enable
    ):
        fail("ordinary SongCore selections can touch Nexora Unity assets")

    required_shader_tokens = (
        "STEREO_MULTIVIEW_ON",
        "STEREO_INSTANCING_ON",
        "UNITY_SETUP_STEREO_EYE_INDEX_POST_VERTEX",
        "ZTest LEqual",
        "_FlipX",
        "_FlipY",
        "_SwapEyes",
        "_CameraAmount",
        "_CameraFisheye",
        "_CameraGlitch",
        "_VideoReady",
        "frameReady callback",
    )
    for token in required_shader_tokens:
        if token not in shader:
            fail(f"Quest shader is missing: {token}")
        if token not in builder and token in {
            "STEREO_MULTIVIEW_ON", "STEREO_INSTANCING_ON", "ZTest LEqual",
            "_FlipY", "_SwapEyes", "_CameraAmount", "_VideoReady",
            "frameReady callback"
        }:
            fail(f"Unity builder does not assert shader contract token: {token}")
    if "NexoraCameraFX" in builder or "NexoraCameraFX" in runtime:
        fail("obsolete framebuffer camera shader path is still packaged")

    forbidden_suffixes = {".dll", ".exe", ".dylib", ".pdb", ".lib"}
    unity_generated_roots = {
        ("unity", "Library"),
        ("unity", "Temp"),
        ("unity", "Logs"),
        ("unity", "obj"),
    }
    pc_artifacts = sorted(
        str(path.relative_to(ROOT))
        for path in ROOT.rglob("*")
        if path.is_file()
        and path.suffix.casefold() in forbidden_suffixes
        and tuple(path.relative_to(ROOT).parts[:2]) not in unity_generated_roots
    )
    if pc_artifacts:
        fail(f"PC binary artifacts are present: {pc_artifacts}")

    print(
        "Nexora Quest contract validation passed: "
        f"{len(known_events)} events, target {TARGET}, scene-safe assets, "
        "Vulkan-safe Unity MaterialOverride video, no-black safety gate, "
        "no PC framebuffer path/artifacts"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
