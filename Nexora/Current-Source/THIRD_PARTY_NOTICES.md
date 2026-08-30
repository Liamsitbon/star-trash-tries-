# Third-party notices

Nexora is a separate mod and is not an official Vivify build or release.

BeatSaberCinema is a separate GPLv3 project. Nexora does not include or copy
BeatSaberCinema source, assets, downloader code, codecs, or UI. Its optional
`Cinema` capability detection uses the public SongCore API. Nexora's local video
path was implemented independently with Android MediaPlayer, SurfaceTexture,
OpenGL ES, and Unity's public external-texture API; it does not copy Cinema
source or bundle a codec/player library, so Nexora remains MIT-licensed.
Cinema-compatible playback itself stays in the separate GPLv3 Cinema Quest mod.

The generation-gated scene retirement, pause/resume handling, and safe
camera-pass-through structure were adapted from the local Quest port of
Vivify. Vivify is distributed under the MIT License; its original copyright
and license remain applicable to the portions adapted here. No Vivify name,
logo, shader asset, prefab, or proprietary map content is redistributed by
Nexora.

Runtime dependencies are packaged or downloaded under their own licenses:
beatsaber-hook, bs-cordl, custom-types, CustomJSONData, SongCore, Scotland2,
Paper2 and config-utils. Their upstream notices and package metadata remain
authoritative.
