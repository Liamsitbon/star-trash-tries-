#!/usr/bin/env python3
"""Source/package boundary checks; not a replacement for a Quest playtest."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def validate():
    manifest = json.loads((ROOT / "mod.json").read_text())
    qpm = json.loads((ROOT / "qpm.json").read_text())
    assert manifest["id"] == qpm["info"]["id"] == "ne-fixed"
    assert manifest["version"] == qpm["info"]["version"]
    assert manifest["packageVersion"] == "1.40.8_7379"
    assert manifest["packageId"] == "com.beatgames.beatsaber"
    assert manifest["modloader"] == "Scotland2"
    assert manifest["lateModFiles"] == ["libNEFixed.so"]
    assert qpm["info"]["additionalData"]["overrideSoName"] == "libNEFixed.so"
    for key in ("modFiles", "libraryFiles", "fileCopies", "copyExtensions"):
        assert manifest[key] == [], f"Unexpected writes/libraries: {key}"
    deps = {d["id"]: d for d in manifest["dependencies"]}
    assert deps["NoodleExtensions"]["version"] == "=1.6.3"
    assert deps["lapiz"]["version"] == "=0.2.23"
    assert deps["NoodleExtensions"]["downloadIfMissing"] == (
        "https://github.com/bsq-ports/NoodleExtensions/releases/download/v1.6.3/NoodleExtensions.qmod"
    )
    for dep in qpm["dependencies"]:
        if dep["id"] in deps:
            assert dep["versionRange"] == deps[dep["id"]]["version"]
    source = "\n".join(p.read_text() for p in (ROOT / "src").glob("*.cpp"))
    forbidden = ("dlsym(", "dlopen(", "modloader_force_unload(",
                 "RegisterCapability(", "AddBeatmapObjectDataOverride(", "NEHooks.h", "AssociatedData.h")
    for token in forbidden:
        assert token not in source, f"Add-on crossed a core ownership boundary: {token}"
    assert 'modloader_require_mod(&base, MatchType_IdVersion)' in source
    assert 'Lapiz::Zenject::Location::Player' in source
    assert '1000, false' in source
    assert 'RegisterRedecorator(container)' in source
    assert 'No NE-private' in source
    # One documented post-hook, never a wholesale second copy of NE hooks.
    assert source.count("INSTALL_HOOK(") == 1
    assert source.count("MAKE_HOOK_MATCH(") == 1
    assert 'INSTALL_HOOK(Logger, NEFixed_CutoutEffect_SetCutout)' in source
    assert 'controller->ApplyChanges()' in source
    print("Add-on contract passed: distinct ID/SO, explicit official base, no duplicate NE core/injector")
    return manifest


if __name__ == "__main__":
    validate()
