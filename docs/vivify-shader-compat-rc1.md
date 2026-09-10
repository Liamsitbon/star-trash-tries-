# Vivify shader compatibility RC1 — Quest 1.40.8_7379

Version: **0.6.14-rc.1**, isolated branch `vivify-shader-compat`. This is a candidate,
not a new recommended stable release. The existing main branch is unchanged.
The same `vivify` mod ID means it replaces, not co-installs alongside, other Vivify
versions. It has **not** been installed or gameplay-tested on the user's Quest.

## Implemented scope

- Preserve every device-supported authored RenderTexture format. For unsupported
  formats, use a supported compatible numeric domain with at least the authored
  channels/precision: float depth/HDR is not silently reduced to 8-bit color,
  integer data stays integer, and depth/shadow samplers are not color substitutes.
- Cache format resolution once per declared texture, eliminating per-frame
  capability queries, fallback warnings and context-string construction there.
- Clamp scaled texture dimensions **before** float-to-int conversion. Invalid
  ratios previously could cause undefined conversion before the size clamp.
- Log the actual shader level/compute capability, alongside existing graphics API,
  stereo and format diagnostics. Capability flags do not prove any shader works.

Widening can consume more memory and expose extra channels a shader might read.
It needs a map/device comparison; no appearance/performance equivalence is claimed.
If no suitable format exists, the resource stays unavailable with a one-time
warning, not a pretend successful 8-bit fallback. This is not proof of the root
cause of 42-flux's black scenes and does not restore its missing effects by itself.
No camera matrix overrides, forced per-eye passes, global shader replacement,
geometry-stage emulation, global depth bias or z-fighting hacks were added.

## Actual bundle inspection / pending device test

A read-only extraction of the supplied 42-flux Android bundle found 76 Shader
objects and 347 unique complete GLES program groups: 163 have multiview tokens,
20 include geometry stages. This includes the original BlackHole, Wormhole,
VolumetricFog and GeometryDissolve programs. Counts are **extraction**, not a
compilation-success result. 65 additional source-like/binary/unrecognized records
were reported as skipped; they are not silently counted as supported.

`tools/shader_probe` builds an isolated Android ARM64 EGL/GLES compile/link tool.
It does not inject into Beat Saber, draw/execute raymarch loops, patch the map or
run a performance benchmark. Its generated shader inputs are kept local and are
not uploaded here. The bundle remains unchanged. No Quest was visible to ADB at
the time of preparation, so driver compile/link and stereo visual checks remain
pending. Even driver link success will not validate Unity inputs, per-eye output,
Vulkan, depth textures, culling, effects or frame rate.

## Validation

- Native ARM64 mod build completed; QMOD contains only mod.json, libVivify.so and
  LICENSE, targeting Scotland2 / Beat Saber 1.40.8_7379, with no PC/macOS payloads.
- Lifecycle, performance-policy, metadata and RT-format/dimension host tests passed;
  the RT tests include ASan/UBSan/float-cast-overflow and extreme/nonfinite ratios.
- Shader extractor's stage-wrapper tests passed; isolated Android probe compiled.
- GitHub Actions independently builds the candidate and preserves artifacts. It
  does not publish a release, set latest or install anything on the headset.

## Source basis and limits

[Unity RenderTextureFormat](https://docs.unity3d.com/2021.3/Documentation/ScriptReference/RenderTextureFormat.html)
documents distinct color, float, integer and depth/shadow formats. Runtime
[SupportsRenderTextureFormat](https://docs.unity3d.com/2021.3/Documentation/ScriptReference/SystemInfo.SupportsRenderTextureFormat.html)
checks the current GPU rather than assuming platform-wide support.
[Unity shader targets](https://docs.unity3d.com/2021.3/Documentation/Manual/SL-ShaderCompileTargets.html)
describe feature requirements, not a promise that all PC shaders can run on Quest.
Raymarching is shader code; a mod-wide "enable all raymarching" flag cannot repair
missing compiled stages or incorrect depth/camera/stereo inputs.

Nexora's RGB/video shaders and its experimental RGBD projection are a separate
renderer. This Vivify change does not turn Nexora into a general PC shader host.
