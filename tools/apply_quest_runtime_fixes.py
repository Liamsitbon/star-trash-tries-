#!/usr/bin/env python3
"""Apply the Quest runtime fixes required by the 42-flux/Murder Plot release audit.

This script is intentionally idempotent. The large runtime translation units are kept
close to their upstream-derived layout, while the build entrypoints apply these small,
reviewable Quest-specific deltas before CMake compiles them.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import re


REPO = Path(__file__).resolve().parents[1]
CINEMA = REPO / "Cinema/Current-Source/src/CinemaRuntime.cpp"
NEXORA = REPO / "Nexora/Current-Source/src/NexoraRuntime.cpp"

CINEMA_MARKER = "Quest cold-start guard: never seek the Android decoder to exactly 0 before first Play"
NEXORA_MARKER = "Quest black-dome guard: use VideoPlayer.texture (APIOnly) as the primary decoded surface"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one source match, found {count}")
    return text.replace(old, new, 1)


def patch_cinema() -> bool:
    text = CINEMA.read_text(encoding="utf-8")
    if CINEMA_MARKER in text:
        return False

    text = replace_once(
        text,
        """    float const songTime = _audioController->get_songTime();
    double const desired = DesiredVideoTime(songTime);
    double const length = _videoPlayer->get_length();
""",
        """    float const songTime = _audioController->get_songTime();
    double const desired = DesiredVideoTime(songTime);
    if (!std::isfinite(songTime) || !std::isfinite(desired)) {
      SetScreensVisible(false);
      PaperLogger.warn("Cinema ignored a non-finite startup clock value");
      return;
    }
    double const length = _videoPlayer->get_length();
""",
        "Cinema finite-clock guard",
    )

    text = replace_once(
        text,
        """    if (desired < 0.0) {
      if (_videoPlayer->get_isPlaying()) _videoPlayer->Pause();
      if (_videoPlayer->get_canSetTime()) _videoPlayer->set_time(0.0);
      _playIssued = false;
      SetScreensVisible(false);
      return;
    }
""",
        f"""    if (desired < 0.0) {{
      if (_videoPlayer->get_isPlaying()) _videoPlayer->Pause();
      // {CINEMA_MARKER}.
      // During pre-roll the video is hidden, so resetting decoder time is unnecessary.
      // Android/Quest VideoPlayer is allowed to finish Prepare() untouched and receives
      // its first timestamp only when a real non-zero seek is actually required.
      _playIssued = false;
      SetScreensVisible(false);
      return;
    }}
""",
        "Cinema negative pre-roll zero seek",
    )

    text = replace_once(
        text,
        """    if (!_playIssued) {
      if (_videoPlayer->get_canSetTime()) _videoPlayer->set_time(desired);
      float const speed = DesiredPlaybackSpeed();
""",
        """    if (!_playIssued) {
      // Starting an already-prepared Android decoder with set_time(0) before Play can
      // enter a broken seek/start path. Exact/near-zero starts need no seek at all.
      bool const needsInitialSeek = desired > 0.05;
      if (needsInitialSeek && _videoPlayer->get_canSetTime()) {
        _videoPlayer->set_time(desired);
      }
      float const speed = DesiredPlaybackSpeed();
""",
        "Cinema first-play seek",
    )

    text = replace_once(
        text,
        """      PaperLogger.info("Cinema playback issued target={:.3f}s speed={:.3f} stagedSlowStart={}",
                       desired, speed, _slowStartPending);
""",
        """      PaperLogger.info("Cinema playback issued target={:.3f}s speed={:.3f} stagedSlowStart={} initialSeek={}",
                       desired, speed, _slowStartPending, needsInitialSeek);
""",
        "Cinema startup log",
    )

    CINEMA.write_text(text, encoding="utf-8")
    return True


def patch_nexora() -> bool:
    text = NEXORA.read_text(encoding="utf-8")
    if NEXORA_MARKER in text:
        return False

    # Unity's APIOnly mode exposes the decoder-owned VideoPlayer.texture directly.
    # This avoids the fixed-size RenderTexture copy path that can stay black on Quest.
    pattern = re.compile(
        r"  // Cinema's reliable path is a decoder-owned RenderTexture, not a direct\n"
        r".*?(?=  video->set_audioOutputMode\(UnityEngine::Video::VideoAudioOutputMode::None\);)",
        re.DOTALL,
    )
    replacement = f"""  // {NEXORA_MARKER}.
  // Unity documents APIOnly specifically for assigning VideoPlayer.texture from code.
  // Avoiding the fixed 2048x1024 RenderTexture also removes a Quest/Android copy path
  // that can report prepared/frame>=0 while the sampled target remains black.
  UnityEngine::RenderTexture* videoTexture = nullptr;
  bool const renderTexturePipeline = false;
  video->set_renderMode(UnityEngine::Video::VideoRenderMode::APIOnly);
  PaperLogger.info("Nexora dome '{{}}' selected APIOnly decoder texture path", id);

"""
    text, count = pattern.subn(replacement, text, count=1)
    if count != 1:
        raise RuntimeError(f"Nexora video pipeline: expected one source match, found {count}")

    text = replace_once(
        text,
        """    if (dome.pendingPlay) {
      if (dome.video->get_canSetTime()) {
        double const desired =
            dome.syncToSong
                ? std::max(0.0f,
                           songTime - dome.eventStartSongTime + dome.videoOffset)
                : std::max(0.0f, dome.videoOffset);

        dome.video->set_time(desired);
      }

      dome.video->Play();
      dome.pendingPlay = false;
      PaperLogger.info("Nexora started playback on dome '{}'", dome.id);
    }
""",
        """    if (dome.pendingPlay) {
      double const desired =
          dome.syncToSong
              ? std::max(0.0f, songTime - dome.eventStartSongTime + dome.videoOffset)
              : std::max(0.0f, dome.videoOffset);
      if (!std::isfinite(desired)) {
        dome.prepareFailed = true;
        dome.pendingPlay = false;
        if (Alive(dome.renderer)) dome.renderer->set_enabled(false);
        PaperLogger.error("Nexora refused non-finite initial video time on dome '{}'", dome.id);
        return;
      }

      // Do not issue set_time(0) before the first Play on Android. Non-zero practice
      // starts still seek normally, while map starts at 0 enter the decoder cleanly.
      bool const needsInitialSeek = desired > 0.05;
      if (needsInitialSeek && dome.video->get_canSetTime()) {
        dome.video->set_time(desired);
      }

      dome.video->Play();
      dome.pendingPlay = false;
      PaperLogger.info("Nexora started playback on dome '{}' initialSeek={} target={:.3f}s",
                       dome.id, needsInitialSeek, desired);
    }
""",
        "Nexora first-play seek",
    )

    # Keep polling authoritative enough to recover if frameReady is omitted by Android.
    text = replace_once(
        text,
        """    bool const decodedFrame = dome.video->get_frame() >= 0;

    bool const hadTexture = dome.textureBound;
    dome.textureBound = dome.textureBound || (Alive(texture) && decodedFrame);
""",
        """    bool const decodedFrame = dome.video->get_frame() >= 0;

    bool const hadTexture = dome.textureBound;
    dome.textureBound = dome.textureBound || (Alive(texture) && decodedFrame);
    if (!hadTexture && dome.textureBound) {
      // Once polling proves a decoder frame exists, the callback is no longer needed.
      // This avoids repeated JNI/event traffic on Android and makes polling a complete
      // fallback rather than merely a second signal.
      dome.video->set_sendFrameReadyEvents(false);
    }
""",
        "Nexora polling fallback",
    )

    NEXORA.write_text(text, encoding="utf-8")
    return True


def verify() -> None:
    cinema = CINEMA.read_text(encoding="utf-8")
    nexora = NEXORA.read_text(encoding="utf-8")
    required = {
        "Cinema cold-start marker": CINEMA_MARKER in cinema,
        "Cinema zero-seek gate": "bool const needsInitialSeek = desired > 0.05;" in cinema,
        "Cinema finite time guard": "!std::isfinite(songTime) || !std::isfinite(desired)" in cinema,
        "Nexora APIOnly marker": NEXORA_MARKER in nexora,
        "Nexora decoder texture": "VideoRenderMode::APIOnly" in nexora,
        "Nexora zero-seek gate": "bool const needsInitialSeek = desired > 0.05;" in nexora,
        "Nexora frame polling": "dome.video->get_frame() >= 0" in nexora,
    }
    missing = [name for name, ok in required.items() if not ok]
    if missing:
        raise RuntimeError("Quest runtime fix verification failed: " + ", ".join(missing))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--component", choices=("all", "cinema", "nexora"), default="all")
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()

    changed: list[str] = []
    if not args.verify_only:
        if args.component in ("all", "cinema") and patch_cinema():
            changed.append("Cinema")
        if args.component in ("all", "nexora") and patch_nexora():
            changed.append("Nexora")

    verify()
    print("Quest runtime fixes: PASS" + (f" (patched {', '.join(changed)})" if changed else " (already applied)"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
