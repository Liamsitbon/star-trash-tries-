# Compatibility notes — Beat Saber 1.40.8 Quest

## Runtime stack

Noodle Extensions Quest uses the Quest-native equivalents of the shared PC modchart stack:

- CustomJSONData for custom beatmap data and events
- Tracks for track properties, point definitions, and parenting
- Chroma for color/environment interoperability
- SongCore for requirements, capabilities, and level-selection integration

## Vivify

Vivify and Noodle Extensions can be required by the same difficulty. Noodle Extensions does not block or replace Vivify, and v1.8.0 reports the Vivify requirement in runtime diagnostics so combined-map failures are easier to separate.

This is intentionally a loose integration:

- Noodle handles note, obstacle, player, and track transforms.
- Vivify handles its AssetBundles, prefabs, cameras, materials, and Vivify-specific events.
- Neither mod should own or destroy the other mod's objects.
- Shared scene transitions must leave both mods able to clear their own caches.

A direct C++ API bridge should only be added once the Quest Vivify port exposes a stable public header/API. Guessing private symbols would make both mods more fragile.

## Heck

The official Heck repository is the PC framework that hosts Noodle Extensions, Chroma, and shared systems. It is not a Quest QMOD dependency. Linking the PC assembly/framework into this native Quest port is not valid.

The safe compatibility path is behavioral parity: use the same JSON property names and event semantics while keeping Quest dependencies native. CustomJSONData + Tracks already provide most of the shared data/animation layer used by this port.

## Known unsupported combination

A difficulty that requires both Mapping Extensions and Noodle Extensions can conflict because both alter placement semantics. The configuration option `Block NE + ME conflicts` defaults to enabled. In v1.8.0, disabling it now consistently allows Noodle runtime activation as an explicit tester choice.

## Suggested combined-map test order

1. Load a normal custom song, then a Noodle-only song.
2. Restart the Noodle song twice.
3. Switch to a Vivify-only song.
4. Switch to a song requiring both Vivify and Noodle.
5. Return to the menu and load a non-Noodle song.
6. Check logs for `Gameplay activation` and `Runtime reset` lines.

Record the map, difficulty, timestamp, first broken object/event, and whether the failure survives a restart.


## Merged Quest interoperability layer

This source keeps **Tracks** and **CustomJSONData** as the shared runtime used by Noodle Extensions, Chroma and Vivify. It does not copy or embed the PC Heck runtime.

- A map requiring Vivify sets a per-map compatibility flag.
- Noodle avoids resetting pooled note materials when Vivify may own the visual renderer.
- Runtime state is cleared on stable scene transitions and before the next map.
- Parent-track transforms now respect left-handed mode.
- Mapping Extensions conflicts remain configurable and are blocked by default.

This is cooperative interoperability, not a claim that Vivify rendering or Chroma fog is implemented by Noodle Extensions.
