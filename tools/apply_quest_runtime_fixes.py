#!/usr/bin/env python3
"""Verify Nexora's stable Quest/Vulkan video boundary.

This used to rewrite the runtime sources immediately before every build. That was
fragile. It is deliberately read-only now: the checked-in source is the single
build input and the retired OpenGL/SurfaceTexture bridge fails the build instead
of being patched behind the developer's back.
"""

from __future__ import annotations

import argparse
from pathlib import Path


REPO = Path(__file__).resolve().parents[1]
NEXORA = REPO / "Nexora/Current-Source/src/NexoraRuntime.cpp"
NEXORA_HEADER = REPO / "Nexora/Current-Source/include/NexoraRuntime.hpp"
NEXORA_CMAKE = REPO / "Nexora/Current-Source/CMakeLists.txt"
NEXORA_SHADER = (
    REPO / "Nexora/Current-Source/unity/Assets/Nexora/Shaders/NexoraDome.shader"
)


def require_tokens(label: str, text: str, tokens: tuple[str, ...]) -> list[str]:
    return [f"{label}: {token}" for token in tokens if token not in text]


def reject_tokens(label: str, text: str, tokens: tuple[str, ...]) -> list[str]:
    return [f"{label}: forbidden {token}" for token in tokens if token in text]


def verify(component: str) -> None:
    del component  # Retained as a CLI compatibility argument.
    nexora = NEXORA.read_text(encoding="utf-8")
    header = NEXORA_HEADER.read_text(encoding="utf-8")
    cmake = NEXORA_CMAKE.read_text(encoding="utf-8")
    shader = NEXORA_SHADER.read_text(encoding="utf-8")
    failures = require_tokens(
        "Nexora runtime",
        nexora,
        (
            "UnityEngine::Video::VideoPlayer",
            "VideoRenderMode::MaterialOverride",
            "set_targetMaterialRenderer",
            'set_targetMaterialProperty(StringW("_MainTex"))',
            "set_waitForFirstFrame(true)",
            "set_sendFrameReadyEvents(true)",
            "OnVideoFrameReady",
            "safetyVisible",
            "s_propVideoReady",
            "safety backdrop remains visible",
            "AndroidVideoMedia",
        ),
    )
    failures += require_tokens(
        "Nexora header",
        header,
        ("UnityEngine/Video/VideoPlayer.hpp", "bool safetyVisible = false;"),
    )
    failures += require_tokens(
        "Nexora shader",
        shader,
        ("_VideoReady", "frameReady callback", "if (_VideoReady < 0.5)"),
    )
    retired = (
        "QuestNativeVideo",
        "SurfaceTexture",
        "GL_TEXTURE_EXTERNAL_OES",
        "CreateExternalTexture",
        "IssuePluginEvent",
        "android/media/MediaPlayer",
        "-lGLESv3",
    )
    failures += reject_tokens("Nexora runtime", nexora, retired)
    failures += reject_tokens("Nexora header", header, retired)
    failures += reject_tokens("Nexora build", cmake, retired)
    failures += reject_tokens(
        "Nexora runtime/build",
        (nexora + "\n" + cmake).lower(),
        ("ffmpeg", "libavcodec", "exoplayer"),
    )

    if failures:
        raise RuntimeError(
            "Quest Vulkan-video verification failed:\n  - " + "\n  - ".join(failures)
        )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--component", choices=("all", "nexora"), default="all")
    # Retained for compatibility with existing CI/build invocations. Verification
    # is now the only mode, even when this flag is omitted.
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()

    verify(args.component)
    print(f"Quest Vulkan-video boundary: PASS ({args.component}, read-only)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
