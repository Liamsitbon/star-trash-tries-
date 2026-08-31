#!/usr/bin/env python3
"""Validate optional Quest interoperability and license boundaries."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PROJECTS = {
    "NoodleExtensions": ROOT / "NE-Fixed/Current-Source",
    "nexora": ROOT / "Nexora/Current-Source",
    "vivify": ROOT / "Vivify/Current-Source",
}
PEER_IDS = {"cinema", "noodleextensions", "nexora", "vivify"}


def fail(message: str) -> None:
    raise SystemExit(f"Interop contract validation failed: {message}")


def require(text: str, *tokens: str) -> None:
    for token in tokens:
        if token not in text:
            fail(f"required token is missing: {token}")


def main() -> int:
    headers: dict[str, str] = {}
    for mod_id, project in PROJECTS.items():
        manifest = json.loads((project / "mod.json").read_text(encoding="utf-8"))
        if manifest.get("id") != mod_id:
            fail(f"unexpected manifest id for {project}: {manifest.get('id')!r}")
        if manifest.get("packageVersion") != "1.40.8_7379":
            fail(f"{mod_id} targets the wrong Beat Saber version")
        dependencies = {
            str(item.get("id", "")).casefold()
            for item in manifest.get("dependencies", [])
        }
        forbidden = sorted((PEER_IDS - {mod_id.casefold()}) & dependencies)
        if forbidden:
            fail(f"{mod_id} has hard peer-mod dependencies: {forbidden}")

        header = (project / "include/QuestInterop.hpp").read_text(encoding="utf-8")
        headers[mod_id] = header
        require(
            header,
            "SPDX-License-Identifier: MIT",
            "SongCore::API::Capabilities::IsCapabilityRegistered",
            'kCinema = "Cinema"',
            'kNexora = "Nexora"',
            'kNoodleExtensions = "Noodle Extensions"',
            'kVivify = "Vivify"',
            "required.nexora && !required.cinema",
        )
        for forbidden_token in ("dlsym", "dlopen", "modloader_require_mod"):
            if forbidden_token in header:
                fail(f"{mod_id} interop uses private or hard-link API: {forbidden_token}")

    semantic_hashes = {
        hashlib.sha256(
            "\n".join(
                line for line in header.splitlines()
                if "Explicitly requiring both" not in line
                and "Nexora owns a map" not in line
            ).encode("utf-8")
        ).hexdigest()
        for header in headers.values()
    }
    if len(semantic_hashes) != 1:
        fail("the three active interoperability headers have drifted")

    noodle_license = (PROJECTS["NoodleExtensions"] / "LICENSE").read_text(
        encoding="utf-8"
    )
    vivify_license = (PROJECTS["vivify"] / "LICENSE").read_text(encoding="utf-8")
    nexora_license = (PROJECTS["nexora"] / "LICENSE").read_text(encoding="utf-8")
    require(noodle_license, "MIT License", "Copyright (c) 2020 Aeroluna")
    require(vivify_license, "MIT License", "Copyright (c) 2025 Aeroluna")
    require(nexora_license, "MIT License", "Copyright (c) 2026 Liam Sitbon")

    nexora_source = (PROJECTS["nexora"] / "src/NexoraRuntime.cpp").read_text(
        encoding="utf-8"
    )
    vivify_source = (PROJECTS["vivify"] / "src/VivifyAssets.cpp").read_text(
        encoding="utf-8"
    )
    noodle_source = (
        PROJECTS["NoodleExtensions"]
        / "src/Hooks/SceneTransition/SceneTransitionHelper.cpp"
    ).read_text(encoding="utf-8")
    require(nexora_source, "QuestModInterop::Inspect", "GetNexoraEnabled()")
    require(vivify_source, "Vivify interop:", "QuestModInterop::InstalledPeers")
    require(
        noodle_source,
        "NECaches::VivifyActive",
        "NECaches::NexoraActive",
        "NECaches::CinemaActive",
    )

    print(
        "Quest interoperability contract passed: three active independent QMODs, "
        "public SongCore capability discovery (including optional external Cinema "
        "detection), and MIT license boundaries preserved"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
