#!/usr/bin/env python3
"""Validate the Quest Cinema runtime, toggle, package and local-video contract."""

from __future__ import annotations

import json
import struct
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TARGET = "1.40.8_7379"
VERSION = "0.1.2"


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
    hooks = (ROOT / "src/CinemaHooks.cpp").read_text(encoding="utf-8")
    shader = (ROOT / "unity/Assets/Cinema/Shaders/CinemaVideoScreen.shader").read_text(
        encoding="utf-8"
    )
    require(
        main_source,
        (
            "BSML::Register::RegisterSettingsMenu",
            'u"Enabled"',
            "SetCinemaEnabled(value)",
            "Runtime::Instance().SetEnabled(enabled)",
            "if (enabled) CinemaQuest::InstallHooks();",
            "Cinema hooks remain uninstalled",
            "safetyReset_0_1_1",
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
            "VideoRenderMode::RenderTexture",
            "set_targetTexture(_videoTexture)",
            "add_frameReady(_frameReadyDelegate)",
            "add_prepareCompleted(_prepareCompletedDelegate)",
            "add_errorReceived(_errorReceivedDelegate)",
            "SetScreensVisible(false)",
            "SetScreensVisible(true)",
            "if (!enabled)",
            "StopSession();",
            "_videoPlayer->set_targetTexture(nullptr)",
            "_videoTexture->Release()",
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
            "AudioTimeSyncController_Pause",
            "AudioTimeSyncController_Resume",
            "if (GetCinemaEnabled())",
            "if (gHooksInstalled) return;",
        ),
        "song/pause synchronization",
    )
    if "GameScenesManager_ScenesTransitionCoroutine" in hooks:
        fail("unsafe global scene-transition hook is still installed")
    require(
        runtime,
        (
            'scene.get_name() == u"MainMenu"',
            "retiring the previous gameplay session on a stable frame",
            "if (GetCinemaEnabled())",
            "no runtime GameObject, AssetBundle, RenderTexture or VideoPlayer was created",
        ),
        "scene-safe opt-in lifecycle",
    )
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
        f"(disabled-by-default recovery; no transition hook; local-only video; stereo shader; {TARGET}; ELF64 AArch64)"
    )


if __name__ == "__main__":
    main()
