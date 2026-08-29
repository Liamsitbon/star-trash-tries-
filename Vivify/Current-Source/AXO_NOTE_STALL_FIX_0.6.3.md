# Vivify Quest 0.6.3 — Axo note-prefab stall fix

## Provenance boundary

The deleted original repository was `https://github.com/axo-lotl/Vivify-Quest`.
The public repository currently retaining the Git objects is owned by a
different account and is not treated as the author or a trusted source of new
work. The retained `b1401dd57197eab7b4e5d070c2727244669da2dc` commit is tied by
GitHub's API to the real `axo-lotl` user ID `281496494`. More importantly, the
next commit, `d86c4152b57f1cb288cb2e4c29b729e2b51ffde3`, is a valid GitHub
`web-flow`-signed commit authored by that account and names `b1401dd` as its
direct parent. This establishes the preserved 0.4.1 tree as the authentic
lineage while making no trust claim about its present host.

## Aether failure path

Aether intentionally pauses notes in its bridge section and switches their
assigned prefabs between gemstone and wireframe variants. The authored pause
is expected. A render/main-thread hitch while processing the assignments is
not.

The old hot path restored and instantiated a replacement again whenever an
assignment refresh reached a note. Axo 0.4.1 added:

- a fingerprint for the effective note or saber prefab assignment;
- replacement-integrity checks;
- reassertion of hidden original renderers without destroy/instantiate when
  the fingerprint is unchanged;
- refresh of the already-known note replacement pool instead of a scene-wide
  object search.

The 0.6.2 source already contained the fingerprint, integrity and reassertion
parts, upgraded to a fixed-width 64-bit fingerprint. A later compatibility
change had replaced Axo's bounded note-pool refresh with
`FindObjectsOfType<NoteController*>(true)` for every note assignment. That
avoided missing a first mid-song assignment, but made Aether's dense event
stream repeatedly scan the complete Unity scene.

## 0.6.3 implementation

0.6.3 keeps all of Axo's idempotent replacement behavior and restores the
bounded refresh principle without reintroducing the mid-song-first-assignment
bug:

1. The existing note Init hooks register every pooled `NoteController`, not
   only controllers that already have custom replacements.
2. `AssignObjectPrefab` records the tracks modified by the event.
3. Refresh visits the registered pool, discards dead controllers, ignores
   inactive pooled objects, and selects only notes whose tracks intersect the
   event's affected tracks.
4. The existing fingerprint guard then prevents destroy/instantiate when the
   effective prefab is unchanged.
5. No graphics quality, material, shader, camera or authored effect is removed
   or reduced.

The debug performance heartbeat now reports `noteRefreshEvents`,
`noteRefreshCandidates`, and `prefabLookupRebuilds`. These counters make the
remaining event cost observable on hardware without per-event release logging.

## Evidence boundary

Host tests, static hot-path checks, ARM64 compilation, QMOD inspection and
binary/hash inspection can prove that the implementation is present and
packaged. They cannot prove that a Quest GPU stall is gone. A normal-play pass
through Aether's bridge scene, captured with the 0.6.3 runtime and fresh logs,
is still required.
