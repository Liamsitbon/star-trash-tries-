#!/usr/bin/env python3
"""Validate the Quest Cinema runtime, toggle, package and local-video contract."""

from __future__ import annotations

import json
import struct
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUNDLE_ROOT = ROOT.parents[1]
SHARED_ROOT = BUNDLE_ROOT / "Shared/QuestNativeVideo"
if not (SHARED_ROOT / "src/Player.cpp").is_file():
    SHARED_ROOT = ROOT / "shared/QuestNativeVideo"
TARGET = "1.40.8_7379"
VERSION = "0.2.1"


def fail(message: str) -> None:
    raise SystemExit(f"FAIL: {message}")


def require(source: str, tokens: tuple[str, ...], contract: str) -> None:
    missing = [token for token in tokens if token not in source]
    if missing:
        fail(f"{contract} is missing {missing}")


def main() -> None:
    manifest = json.loads((ROOT / "mod.json").read_text(encoding="utf-8"))
    if (manifest.get("id"), manifest.get("version"), manifest.get("packageVersion")) != (
        "cinema", VERSION, TARGET
    ):
        fail("manifest identity/version/target")

    main_source = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
    runtime = (ROOT / "src/CinemaRuntime.cpp").read_text(encoding="utf-8")
    native_video = (
        SHARED_ROOT / "src/Player.cpp"
    ).read_text(encoding="utf-8")
    hooks = (ROOT / "src/CinemaHooks.cpp").read_text(encoding="utf-8")
    shader = (ROOT / "unity/Assets/Cinema/Shaders/CinemaVideoScreen.shader").read_text(
        encoding="utf-8"
    )
    require(
        main_source,
        (
            "BSML::Register::RegisterSettingsMenu",
            "BSML::Register::RegisterGameplaySetupTab",
            "BSML::Register::RegisterMainMenu",
            'u"Enabled"',
            "SetCinemaEnabled(value)",
            "Runtime::Instance().SetEnabled(enabled)",
            "CinemaQuest::InstallHooks();",
            "Cinema disabled startup complete",
            "safetyReset_0_2_1",
            'document.AddMember("safetyReset_0_2_1", true',
            'document.AddMember("enabled", false',
        ),
        "Mods settings toggle",
    )
    require(
        runtime,
        (
            '"cinema-video.json"',
            '"video.json"',
            "ResolveLocalVideo",
            "IsPathInside(root, candidate)",
            'ReadFloat(*value, "endVideoAt")',
            'ReadFloat(*value, "bloom")',
            "ApplyPlaybackFade",
            "QuestNativeVideo::Player::Create",
            "_nativeVideo->Open",
            "_nativeVideo->Tick",
            "_nativeVideo->HasFrame",
            "_nativeVideo->Texture",
            "_nativeVideo->Pause",
            "_nativeVideo->Seek",
            "_nativeVideo->Stop",
            "produced no first frame within",
            "SetScreensVisible(false)",
            "SetScreensVisible(true)",
            "if (!enabled)",
            "StopSession(true)",
            "StopSession(false)",
            "SetSelectedMapContext",
            "Cinema yielded map video ownership to required Nexora",
            "UnregisterCapability",
        ),
        "Quest local-video lifecycle",
    )
    require(
        hooks,
        (
            "AudioTimeSyncController_StartSong",
            "AudioTimeSyncController_Update",
            "AudioTimeSyncController_StopSong",
            "AudioTimeSyncController_OnDestroy",
            "AudioTimeSyncController_Pause",
            "AudioTimeSyncController_Resume",
            "if (GetCinemaEnabled())",
            "if (gHooksInstalled) return;",
        ),
        "song/pause synchronization",
    )
    if "GameScenesManager_ScenesTransitionCoroutine" in hooks:
        fail("unsafe global scene-transition hook is still installed")
    forbidden_startup = (
        "custom_types::Register::AutoRegister",
        "EnsureBehaviour",
        "RuntimeBehaviour",
    )
    combined = main_source + runtime
    if any(token in combined for token in forbidden_startup):
        fail("disabled startup still registers a custom Unity runtime type")
    dependency_ids = {item["id"] for item in manifest.get("dependencies", [])}
    if "custom-types" in dependency_ids:
        fail("Cinema still declares a direct custom-types dependency")
    require(
        runtime,
        (
            "if (_initialized)",
            "runtime armed without creating Unity objects",
            "RetireGameplay",
            "if (_nativeVideo) _nativeVideo->Stop();",
            "if (canTouchUnity) try",
        ),
        "scene-safe opt-in lifecycle",
    )
    require(
        native_video,
        (
            'FindClass("android/media/MediaPlayer")',
            'FindClass("android/graphics/SurfaceTexture")',
            "GL_TEXTURE_EXTERNAL_OES",
            "Texture2D::CreateExternalTexture",
            "GL::IssuePluginEvent",
            "backendFatal",
            "NewJavaString",
            "decoder surface creation timed out",
            "GL_DRAW_FRAMEBUFFER_BINDING",
            "GL_COLOR_CLEAR_VALUE",
        ),
        "Android native decoder boundary",
    )
    if "UnityEngine::Video" in runtime or "VideoPlayer" in runtime:
        fail("Cinema runtime regressed to Unity VideoPlayer")
    if any(token in native_video.casefold() for token in ("ffmpeg", "libavcodec", "exoplayer")):
        fail("native decoder bundles or references an unapproved codec/player stack")
    require(
        shader,
        (
            "STEREO_MULTIVIEW_ON",
            "STEREO_INSTANCING_ON",
            "UNITY_SETUP_STEREO_EYE_INDEX_POST_VERTEX",
            "ZTest LEqual",
        ),
        "Quest stereo shader",
    )
    forbidden_runtime = (
        "popen(",
        "std::system(",
        "UnityWebRequest::",
        "HttpClient::",
        "libcurl",
    )
    if any(token.casefold() in runtime.casefold() for token in forbidden_runtime):
        fail("network/downloader/PC runtime marker")

    library = (ROOT / "build/libCinema.so").read_bytes()[:64]
    if library[:4] != b"\x7fELF" or library[4] != 2 or struct.unpack("<H", library[18:20])[0] != 183:
        fail("runtime is not ELF64 AArch64")

    qmod = ROOT / f"release/Cinema-Quest-{VERSION}.qmod"
    with zipfile.ZipFile(qmod) as archive:
        if archive.testzip() is not None:
            fail("QMOD ZIP integrity")
        names = {name.casefold() for name in archive.namelist()}
        expected = {
            "mod.json",
            "libcinema.so",
            "cinemaassets.android",
            "license",
            "third_party_notices.md",
            "questmodinterop-mit.txt",
        }
        if names != expected:
            fail(f"QMOD payload changed: {sorted(names)}")
        if any(name.endswith((".dll", ".exe", ".dylib", ".pdb")) for name in names):
            fail("QMOD includes PC/macOS binaries")

    print(
        "Cinema Quest contract: PASS "
        f"(menu-only disabled startup; Android MediaPlayer surface; polling sync; pointer-only scene retirement; local-only video; stereo shader; {TARGET}; ELF64 AArch64)"
    )


if __name__ == "__main__":
    main()
