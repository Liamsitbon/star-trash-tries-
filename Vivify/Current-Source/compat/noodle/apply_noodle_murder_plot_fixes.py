#!/usr/bin/env python3
"""Apply narrowly scoped Murder Plot / Noodle Quest fixes.

Usage:
  python3 apply_noodle_murder_plot_fixes.py /path/to/NoodleExtensions

The script is intentionally conservative: it edits only exact known patterns,
creates .bak files, and refuses ambiguous source instead of guessing.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import shutil
import sys

NOTE_OLD = "bool isDissolving = offset.dissolve.value_or(0) > 0 || offset.dissolveArrow.value_or(0) > 0;"
NOTE_NEW = """// Noodle dissolve values are visibility: 1 = fully visible, 0 = fully hidden.\n  // Keep the cutout-capable material whenever either body or arrow is not fully visible.\n  bool isDissolving = offset.dissolve.value_or(1.0f) < 1.0f ||\n                      offset.dissolveArrow.value_or(1.0f) < 1.0f;"""

CALLBACK_OLD = "if (beatmapOpt && controller != selfController || selfController->_beatmapData != beatmapData) {"
CALLBACK_NEW = "if (beatmapOpt && (controller != selfController || selfController->_beatmapData != beatmapData)) {"


def backup(path: Path) -> None:
    bak = path.with_suffix(path.suffix + ".bak")
    if not bak.exists():
        shutil.copy2(path, bak)


def patch_exact(path: Path, old: str, new: str, label: str) -> str:
    if not path.exists():
        return f"WARN {label}: missing {path}"
    text = path.read_text(encoding="utf-8")
    if new in text:
        return f"OK   {label}: already patched"
    count = text.count(old)
    if count != 1:
        return f"WARN {label}: expected exactly one old pattern, found {count}; left unchanged"
    backup(path)
    path.write_text(text.replace(old, new, 1), encoding="utf-8")
    return f"FIX  {label}: patched {path}"


def audit_fake_preinjection(root: Path) -> list[str]:
    messages: list[str] = []
    transform = root / "src/Hooks/BeatmapDataTransformHelper.cpp"
    fallback = root / "src/Hooks/FakeNotes/BeatmapData.cpp"

    if transform.exists():
        t = transform.read_text(encoding="utf-8", errors="replace")
        parser_hook = "Parser::ParsedEvent" in t and "HandleFakeObjects" in t
        messages.append(
            ("OK   " if parser_hook else "WARN ")
            + "fake pre-injection: "
            + ("parser callback is present" if parser_hook else "parser callback not detected")
        )
    else:
        messages.append("WARN fake pre-injection: BeatmapDataTransformHelper.cpp not found")

    if fallback.exists():
        f = fallback.read_text(encoding="utf-8", errors="replace")
        if "compatibility fallback" in f or "not pre-injected" in f:
            messages.append(
                "INFO fake fallback: compatibility fallback logging is present; "
                "after building, check logcat and investigate if it still triggers on V3 fake-heavy maps"
            )
    return messages


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("root", nargs="?", default=".", help="NoodleExtensions source root")
    args = ap.parse_args()
    root = Path(args.root).expanduser().resolve()

    results = [
        patch_exact(
            root / "src/Hooks/NoteController.cpp",
            NOTE_OLD,
            NOTE_NEW,
            "Murder Plot dissolve material selection",
        ),
        patch_exact(
            root / "src/Hooks/BeatmapObjectCallBackController.cpp",
            CALLBACK_OLD,
            CALLBACK_NEW,
            "beatmap callback precedence",
        ),
    ]
    results.extend(audit_fake_preinjection(root))
    print("\n".join(results))

    # A WARN means the script deliberately avoided an uncertain edit.
    return 2 if any(line.startswith("WARN") for line in results[:2]) else 0


if __name__ == "__main__":
    sys.exit(main())
