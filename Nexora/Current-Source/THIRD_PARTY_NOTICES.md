# Third-party notices

Nexora is a separate mod and is not an official Vivify build or release.

BeatSaberCinema is a separate GPLv3 project. Nexora does not include or copy
BeatSaberCinema source, assets, downloader code, codecs, or UI. Its optional
`Cinema` capability detection and decoder-owned RenderTexture behavior use
public SongCore and Unity APIs and were implemented independently so Nexora
remains MIT-licensed. Cinema-compatible playback itself stays in the separate
GPLv3 Cinema Quest mod.

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
