# Hold My Hand — public Android bundle finding (2026-09-10)

This is AssetBundle evidence, not a reproduction on the user's disconnected
Quest. It does not establish that the currently installed map has the same bytes.

## Inputs

- [Official RSIH map list](https://tvmg.dev/Rsih) links to
  [BeatSaver 512ae](https://beatsaver.com/maps/512ae).
- Public map version hash: `d109045201af797d614c79ee46030f83644ce2e9`.
- Its Info.dat identifies Android bundle CRC `78650609`.
- [Bundle repository metadata](https://repo.totalbs.dev/api/v1/bundles/78650609)
  resolves to [the public Android bundle](https://cdn.repo.totalbs.dev/78650609.vivify).
- Downloaded bundle SHA-256:
  `4e313def151e4cc10a93c4da0e9f584aace9d22d1ae27ba706dcbffdbc578e3f`.
- Read-only inspection with UnityPy 1.25.3; no bundle modification or shader
  substitution was performed. No map/art/shader source is included in this repo.

## Confirmed contents

Shader `Custom/PoofShaders/Audio_Kaleidoscope/World_AudioLink_2.0`, path ID
`-160488997592327566`, contains:

| Android program platform | Decoded program bytes | Subprogram count |
| --- | ---: | ---: |
| GLES3Plus (9) | 4, all zero | 0 |
| Vulkan (18) | 4, all zero | 0 |

The sole parsed pass also has zero vertex and zero fragment subprogram entries.
This is not a failed attempt to text-decompile a binary program: both platform
headers explicitly describe an empty program inventory.

Three materials reference that shader: `World_AudioLink`, `World_AudioLink - Copy`
and `World_AudioLink - Copy - Copy`. They are assigned to five enabled mesh
renderers: a `Cube (1)` and four `CRT Monitor` variants. Thus the empty shader is
not merely an unused bundled asset. Renderer enablement does not prove ancestor
activation or visibility in-game.

There is no Android vertex/fragment program here for Vivify to execute for those
materials. A renderer hook, AudioLink setting, render queue or blanket depth/cull
override cannot reconstruct that missing authored shader implementation. The
correct next asset-side step is to obtain the matching source, resolve its Android
compile/export failures, rebuild the Android bundle and test both eyes. The cause
of the empty export itself remains unknown; Unity version alone is not evidence.

The user's black-world/missing-display report is consistent with this finding,
but other missing animation/effect complaints may have additional causes. The
working orange light does not establish that every material has a working shader.
No claim is made that this one finding explains every RSIH map or all 42-flux bugs.

## Reproduce locally

```sh
python tools/shader_probe/inspect_program_inventory.py /path/to/bundleAndroid2021.vivify \
  /path/to/NEW-inventory.json \
  --shader Custom/PoofShaders/Audio_Kaleidoscope/World_AudioLink_2.0
```

The tool emits metadata and reference counts only, uses a fresh output file and
never changes the bundle. It checks segment-zero program headers; continuation
segments are not misinterpreted as new headers. Nonempty programs in another
bundle are **not** proof of successful Quest driver compilation or rendering.

The separate GLES text extractor's 1 MiB limit can reject very large Poiyomi/other
programs in this map. That is a diagnostic tool limit, **not** proof those shaders
failed to compile. It is unrelated to the four-byte empty world-shader headers.

No generic replacement shader or feature-hiding fallback was added to Vivify.
The runtime candidate remains 0.6.14-rc.1; this follow-up adds inspection tooling
and evidence, not a new gameplay fix or hardware-tested release.
