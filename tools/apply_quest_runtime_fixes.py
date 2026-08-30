#!/usr/bin/env python3
"""Verify the stable Quest-native video boundary used by Cinema and Nexora.

This used to rewrite the runtime sources immediately before every build. That was
fragile and could silently restore Unity VideoPlayer code. It is deliberately
read-only now: the checked-in source is the single build input and a stale video
pipeline fails the build instead of being patched behind the developer's back.
"""

from __future__ import annotations

import argparse
from pathlib import Path


REPO = Path(__file__).resolve().parents[1]
SHARED = REPO / "Shared/QuestNativeVideo/src/Player.cpp"
SHARED_HEADER = REPO / "Shared/QuestNativeVideo/include/QuestNativeVideo/Player.hpp"
CINEMA = REPO / "Cinema/Current-Source/src/CinemaRuntime.cpp"
CINEMA_CMAKE = REPO / "Cinema/Current-Source/CMakeLists.txt"
NEXORA = REPO / "Nexora/Current-Source/src/NexoraRuntime.cpp"
NEXORA_CMAKE = REPO / "Nexora/Current-Source/CMakeLists.txt"


def require_tokens(label: str, text: str, tokens: tuple[str, ...]) -> list[str]:
    return [f"{label}: {token}" for token in tokens if token not in text]


def reject_tokens(label: str, text: str, tokens: tuple[str, ...]) -> list[str]:
    return [f"{label}: forbidden {token}" for token in tokens if token in text]


def verify(component: str) -> None:
    shared = SHARED.read_text(encoding="utf-8")
    shared_header = SHARED_HEADER.read_text(encoding="utf-8")
    failures = require_tokens(
        "shared native backend",
        shared,
        (
            'FindClass("android/media/MediaPlayer")',
            'FindClass("android/graphics/SurfaceTexture")',
            "GL_TEXTURE_EXTERNAL_OES",
            "Texture2D::CreateExternalTexture",
            "GL::IssuePluginEvent",
            "backendFatal",
            "prepare is intentional here",
            "NewJavaString",
            "Android Java VM attachment timed out",
            "decoder surface creation timed out",
            "GL_DRAW_FRAMEBUFFER_BINDING",
            "GL_COLOR_CLEAR_VALUE",
        ),
    )
    failures += require_tokens(
        "shared native header",
        shared_header,
        ("SafePtrUnity<UnityEngine::Texture2D>",),
    )
    failures += reject_tokens(
        "shared native backend",
        shared.lower(),
        (
            "unityengine/video/videoplayer",
            "ffmpeg",
            "libavcodec",
            "exoplayer",
        ),
    )

    if component in ("all", "cinema"):
        cinema = CINEMA.read_text(encoding="utf-8")
        cmake = CINEMA_CMAKE.read_text(encoding="utf-8")
        failures += require_tokens(
            "Cinema runtime",
            cinema,
            (
                "QuestNativeVideo::Player::Create",
                "_nativeVideo->Open",
                "_nativeVideo->Tick",
                "_nativeVideo->HasFrame",
                "_nativeVideo->Stop",
            ),
        )
        failures += require_tokens(
            "Cinema build",
            cmake,
            ("QuestNativeVideo", "-lGLESv3"),
        )
        failures += reject_tokens(
            "Cinema runtime",
            cinema,
            (
                "UnityEngine::Video",
                "VideoRenderMode",
                "UnityEngine::Video::VideoPlayer",
            ),
        )

    if component in ("all", "nexora"):
        nexora = NEXORA.read_text(encoding="utf-8")
        cmake = NEXORA_CMAKE.read_text(encoding="utf-8")
        failures += require_tokens(
            "Nexora runtime",
            nexora,
            (
                "AcquireNativeVideo",
                "QuestNativeVideo::Player::Create",
                "dome.video->Open",
                "dome.video->Tick",
                "dome.video->HasFrame",
                "videoPipeline=AndroidSurface",
            ),
        )
        failures += require_tokens(
            "Nexora build",
            cmake,
            ("QuestNativeVideo", "-lGLESv3"),
        )
        failures += reject_tokens(
            "Nexora runtime",
            nexora,
            (
                "UnityEngine::Video",
                "VideoRenderMode",
                "UnityEngine::Video::VideoPlayer",
            ),
        )

    if failures:
        raise RuntimeError(
            "Quest native-video verification failed:\n  - " + "\n  - ".join(failures)
        )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--component", choices=("all", "cinema", "nexora"), default="all")
    # Retained for compatibility with existing CI/build invocations. Verification
    # is now the only mode, even when this flag is omitted.
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()

    verify(args.component)
    print(f"Quest native-video boundary: PASS ({args.component}, read-only)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
