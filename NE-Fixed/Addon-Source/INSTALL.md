# NE Fixed Add-on 0.1.0-rc.1

An independent, limited-scope release candidate for **Beat Saber Quest 1.40.8_7379 / Scotland2**. Not a standalone Noodle Extensions implementation and not a complete migration of the legacy NE-Fixed 1.8.15 fork. No headset playtest has been completed for this package.

## Installation

Install the [official NoodleExtensions 1.6.3](https://github.com/bsq-ports/NoodleExtensions/releases/tag/v1.6.3), then this add-on and its dependencies. Both stay installed:

| Package | ID | Runtime | Role |
| --- | --- | --- | --- |
| Official NE 1.6.3 | `NoodleExtensions` | `libnoodleextensions.so` | Mapping, events, fake notes, scoring, movement |
| NE Fixed Add-on | `ne-fixed` | `libNEFixed.so` | Note prefab compatibility and cutout precision |

The older **1.8.x NE-Fixed package is a replacement fork**, despite its folder name. Do not install it as a second NE beside the official one. This add-on requires exactly 1.6.3, and declines to apply patches to 1.8.x or an unreviewed base version. It does not delete, downgrade or rewrite your installed mods itself.

Settings are under **Mod Settings → NE Fixed Add-on**. Disabling the add-on restores the usual prefab-selection behavior for the next gameplay scene; NE stays enabled. Removing only `ne-fixed` leaves regular NE installed. Settings are stored separately in `ne-fixed.json`, not in NoodleExtensions or CustomModels settings.

## What this candidate does

- Scans the actual gameplay difficulty once when its player installer runs, including note animation, fake markers, note-track/parent-track animations and Vivify note prefab assignments.
- For note-effect maps, prioritizes ordinary game note, bomb, chain, mirror and debris prefabs over cosmetic replacements. This addresses the custom-note incompatibility reported for Murder Plot and Pandemonium; automatic switching still needs a Quest test.
- Does not intercept later map-owned Vivify prefab assignments, replace custom sabers, force authored invisible objects visible, add fake objects, or change note positions/scoring.
- Repairs small requested cutout changes discarded by NE 1.6.3's 0.005 threshold, including the final step to fully hidden/visible. Calls the existing hook chain first, then uses the game's material-property update path only if the requested value was not applied. Partial dissolve values are preserved exactly. This has its own toggle; it does not resolve missing shaders or unrelated black backgrounds.
- Leaves ordinary maps, environment-only effects and color-only Chroma maps using the player's selected cosmetics.
- No add-on `Update`/per-frame scan, shader patch, NE-private ABI access or duplicate Noodle hook set.

## Validation and rollback

Host tests cover the classifier, malformed/cyclic track graphs, base version, enable policy, cutout endpoints, partial visibility and original-chain calls under AddressSanitizer/UndefinedBehaviorSanitizer. Package checks verify the unique manifest, ARM64 shared object, allowed dynamic dependencies, no original NE binary and no macOS/PC payload. These checks do **not** prove gameplay rendering.

Headset checklist: use official NE alone as the A baseline, then add `ne-fixed` as B; test ordinary cosmetic notes, Murder Plot/Pandemonium, fully hidden and partially dissolved notes, Vivify-assigned notes, custom sabers, restart, practice seek and returning to a normal map. Inspect logcat tag `NEFixed` for the scene-policy decision. The addon never alters map DATs. To roll back B, remove only the add-on or switch its toggle off and enter a new gameplay scene.

Build: `qpm restore`, `bash scripts/test_host.sh`, `bash scripts/build.sh`, `python3 scripts/package_qmod.py` with `ANDROID_NDK_HOME` set to a compatible Android NDK 27.3 installation. Source and migration scope are included in GitHub Actions releases.
