# Patch ownership and migration status

Do not interpret the new add-on version number as legacy 1.8.15 feature parity. The full fork remains unchanged in `../Current-Source`; none of its source fixes were silently deleted. This add-on is a different installation choice, alongside official NE.

| Area | Owner in this configuration | Migration status |
| --- | --- | --- |
| Noodle capabilities, mapping, movement, scoring and track execution | Official NE 1.6.3 | Not duplicated |
| Note-effect cosmetic compatibility | Add-on, via public Lapiz prefab registration | Implemented; Quest verification pending |
| V3 fake-note injection and NoScore behavior | Official NE 1.6.3 | Legacy early-loader/cached JSON fixes not migrated |
| Dense rows, note/slider AD caches, mirror hard-hide/pool restoration | Official NE 1.6.3 | Legacy deltas not migrated |
| Cutout precision and endpoint behavior | Add-on post-hook, keeping the original hook chain | Implemented via game material API; Quest verification pending |
| Parent/player-track callback lifetime, custom providers | Official NE 1.6.3 | Legacy deltas not migrated |
| Vivify renderer/shaders and Nexora media | Their own mods | Unchanged by this add-on |

The official 1.6.3 release already includes fixes for linked/stacked notes, pause restarts, player movement providers and static jump distance. It is not an empty loader that can safely have an entire second NE core put on top.

## Evidence and design boundary

- Reference source: `bsq-ports/NoodleExtensions`, tag `v1.6.3`, commit `d38b50cb72568e36b75110244d41795b877b498d`.
- Official QMOD SHA-256: `fde7d97f9b975928be3c6720f6549e27d1e6e6a01ec6bbca41d977a009fc5e2f`; downloaded manifest confirms game `1.40.8_7379`, ID `NoodleExtensions`, version `1.6.3` and `libnoodleextensions.so`.
- Official `src/Hooks/FakeNotes/BeatmapData.cpp` already converts and appends V3 fake arrays after loading. Registering the legacy early injector alongside it without a proven adapter can duplicate fake notes. This add-on does not do that.
- NE's internal `NEHooks.h`, AssociatedData and cache layouts are not an exported patch API. This add-on does not link to them, resolve private symbols, disable NE's hooks, or unload the original mod.
- The note classifier and prefab compatibility layer are adapted from this repository's MIT-licensed legacy source. MIT notices are retained. The official source was inspected as a behavioral reference; its full core is not redistributed inside the add-on.
- Lapiz 0.2.23 public `Registration<TPrefab,TInstaller>` registers the identity redecorator with priority 1000 and `chain=false` in the gameplay ancestor container. This is the specific override point, not a second beatmap loader. CustomModels 1.2.1 uses priority 300; another cosmetic mod that bypasses Lapiz may require its own adapter.

## Cutout adapter evidence

The captured Quest `libil2cpp.so` (SHA-256 `edae5059bd5a85ba354eb679e1cdcc3816116d2e2b29b3a16a70c05b56dd07cb`) agrees with the `bs-cordl 4008` method metadata:

- `CutoutEffect.SetCutout(float)` at `0x3b80144`, size `0xc4`, selects a configured/random offset and **tail-calls the two-argument overload** at `0x3b80450`. Calling it from the add-on post-hook would recurse; that approach was rejected.
- `SetCutout(float, Vector3)` at `0x3b80450`, size `0xe4`, updates `_cutout`, obtains the controller's material property block (`get_materialPropertyBlock` at `0x39e590c`), writes the supplied offset with w=0 and the requested cutout, then calls `ApplyChanges` at `0x39e5834`. It does not store the supplied offset into the configured `_cutoutOffset` field.
- The official NE 1.6.3 hook skips this entire update for differences <= 0.005, unconditionally (not only on active NE maps). The add-on calls NE first and repairs only an unapplied finite in-range difference within that threshold. Already-applied values, invalid values and larger discrepancies are not overridden. A changed offset with an identical cutout remains owned by the original hook; this candidate does not claim to fix every offset animation case.
- Method addresses above are audit evidence only, **not hard-coded runtime patch addresses**. The add-on resolves the supported game's two-argument method through generated metadata and uses public material APIs. It never hooks the 8-byte `CutoutAnimateEffect.Start`.
- This establishes the native update path and the source-level failure case, not an A/B headset rendering result. No proprietary game binary is included in the source or release.

Primary references: [official release](https://github.com/bsq-ports/NoodleExtensions/releases/tag/v1.6.3), [official fake-object loader](https://github.com/bsq-ports/NoodleExtensions/blob/d38b50cb72568e36b75110244d41795b877b498d/src/Hooks/FakeNotes/BeatmapData.cpp), [official cutout hook](https://github.com/bsq-ports/NoodleExtensions/blob/d38b50cb72568e36b75110244d41795b877b498d/src/Hooks/SmallFixes/CutoutEffect.cpp), [Lapiz prefab ordering](https://github.com/raineaeternal/Lapiz/blob/v0.2.23/src/Hooks/objects/Redecoration.cpp), [CustomModels registration](https://github.com/Metalit/CustomModels/blob/v1.2.1/src/registration.cpp).
