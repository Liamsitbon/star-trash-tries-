# Noodle Extensions Quest — Enhanced 1.40.8 fork

## v1.8.8 lifecycle + queue + activation hardening

This build keeps all 1.8.7 Murder Plot/V3 fake-object fixes and adds a second static audit pass:

- Fixed `AssignTrackParent` attempting to unregister a callback that was never registered. The old `unordered_map::operator[]` path created an empty callback and passed it to `RemoveGameObjectCallback`. Runtime reset now also unregisters tracked parent callbacks before clearing them, so callbacks cannot retain stale `ParentObject*` pointers.
- Player-track pause/resume delegates are now stored per `PlayerTrack` instance and both are unsubscribed in `OnDestroy`, preventing stale callbacks after restart/map teardown.
- Added null guards for `PlayerVRControllersManager`, hand controller transforms, player origin transforms, and failed `GameObjectTrackController` creation.
- `Block NE + ME conflicts` is now respected by gameplay activation as well as the song-selection play button.
- Fixed the fake-note hitsound throttle so its 30-note budget resets even when no new note spawns on the next frame; queued notes are processed FIFO and no longer double-count the budget.
- Left-handed static custom-data mirroring remains a known open compatibility area and is deliberately not guessed in this build.
- `Disable Qosmetics models on NE maps` is currently a legacy/dead setting because the old Qosmetics integration block is disabled; 1.8.8 does not pretend otherwise.

## v1.8.7 cached-song fake-object + parity fix

- V3 fake arrays are now injected immediately inside the V3 BeatmapDataLoader hook,
  before CustomJSONData converts/sorts the save data. This closes the cache-order
  hole where SongCore could parse a custom song before Noodle registered its
  `ParsedEvent` callback, which previously produced the
  `V3 fake objects were not pre-injected` compatibility fallback.
- The parser callback and loader hook share one idempotent injector, so freshly
  parsed and already-cached songs use the same fake-object path without duplicates.
- Restored PC/upstream per-object jump-value semantics for NJS/start-beat-offset
  overrides.
- Restored authored `interactable` animation for normal notes while keeping
  statically `uninteractable` notes non-cuttable.
- Fake-obstacle collision filtering now builds a filtered list instead of removing
  entries while iterating the source list.
- Missing obstacle dissolve components now fail safely instead of dereferencing
  a null `ObstacleDissolve`/`CutoutAnimateEffect`.

Quest port of Noodle Extensions for Beat Saber 1.40.8.

Original PC mod by Aeroluna. Original Quest port work by StackDoubleFlow / bsq-ports. This fork is just the "make more charts stop exploding on current Quest" branch, so keep logs around when testing.

## v1.8.6 dense fake-object stability

- Movement providers are returned from the actual note, obstacle, and slider
  controllers that own them. The previous manager-level lookup never returned
  those providers, causing allocation growth on charts with thousands of fake
  objects such as Murder Plot and 42 Flux.
- Pool returns clear per-object overrides and reject duplicate returns. Pooled
  controllers are rebound to Beat Saber's normal provider before reuse.
- Real and fake-note cuttable state is reset on every controller initialization,
  including derived game-note controllers. Explicit `uninteractable: false`
  remains cuttable, while `uninteractable: true` remains visual-only and NoScore.
- Missing pools, source data, custom-data wrappers, slider movement components,
  and note-cut effect spawners now fail safely instead of dereferencing or
  deliberately terminating the game.
- The late V3 compatibility loader validates source arrays and parsed items before
  conversion, while the preferred idempotent early CustomJSONData path is kept.

## v1.8.5 V3 fake-object loader fix

- V3 `fakeColorNotes`, `fakeBombNotes`, fake obstacles, arcs and chains are now
  appended to CustomJSONData's normal save-data lists during `ParsedEvent`.
  They are converted and sorted in the same pass as normal beatmap objects,
  before Vivify prefab replacement and BeatLeader replay bookkeeping run.
- The old parser callback incorrectly required `levelCustomData`, which CJD has
  not attached yet at that point, and its list `Add` statement was disconnected
  from the macro. Both defects are fixed.
- Every early-injected object receives `NE_fake`; V3 `uninteractable` is also a
  conservative fallback for `fake` and `NoScore` if a legacy loader loses the
  internal marker. Uninteractable notes cannot become cuttable regular notes.
- The late conversion path remains as a compatibility fallback, but it detects
  successful early injection and does not add the same fake objects twice.
- The early callback is also idempotent, and missing or non-object per-item
  `customData` is normalized before `NE_fake` is written.
- Fake-obstacle filtering now tests the CustomJSONData type in the correct
  direction instead of skipping every custom obstacle.

## v1.8.4 Murder Plot one-note visibility fix

- Fully dissolved notes now have only their currently enabled renderers disabled
  until the authored body and arrow visibility becomes non-zero. This closes the
  Quest material edge case that exposed future animated notes as solid rows.
- Renderer state is restored from an ownership-aware list, so pooled notes do
  not stay invisible and renderers intentionally disabled by Vivify or custom
  model mods are not accidentally re-enabled.
- Mirrored notes use the same fully-hidden fallback.
- Missing CutoutEffect, arrow, material-switcher, and renderer components are
  handled without crashing.

## v1.8.3 fully-hidden note dissolve fix

- Corrected conditional note material selection for partial and zero animated
  dissolve values. Version 1.8.4 adds the renderer fallback needed by Quest
  material/model combinations that still ignore CutoutEffect at exactly zero.


## v1.8.0 loader, lifecycle, and build-system upgrade

This release fixes several problems that could make a source build look valid while still producing an inert or unreliable mod.

### Loader and runtime lifecycle

- Restored `src/main.cpp`, including the Scotland2 `setup()` and `late_load()` entry points. Without these exports the library can compile but never initialize the config or install hooks.
- Runtime reset now disables hooks before clearing state, clears track-parent callbacks, releases movement-provider pools, and invalidates cached Unity/Zenject pointers.
- Stable non-gameplay scene changes now clear transient state instead of only flipping the hook-enabled flag.
- The play-button conflict state is cleared when switching away from a custom difficulty, preventing a previous NE+ME chart from leaving later songs blocked.
- `Block NE + ME conflicts` is now respected consistently by both level selection and gameplay activation. Turning it off no longer leaves Noodle disabled anyway.

### Vivify and Heck compatibility

- Vivify requirements are detected in diagnostics and are explicitly treated as compatible with Noodle Extensions.
- Vivify is **not** added as a hard dependency; its Quest port remains responsible for AssetBundle loading and Vivify events.
- Heck is not linked directly because the official Heck project is the PC framework. On Quest, this port continues to use the equivalent native stack: CustomJSONData, Tracks, SongCore, and Chroma.

### Source and packaging reliability

- Added a native `scripts/build.sh` for macOS/Linux/Git Bash, plus a repaired PowerShell build path.
- Removed machine-specific NDK paths from the source archive. The build now discovers the NDK from an argument, environment variable, local `ndkpath.txt`, or the QPM-RS cache.
- Fixed project validation and expanded it to verify loader exports, IDs, versions, package target, dependencies, shell syntax, CRLF corruption, macOS metadata, and leaked personal paths.
- Added `.gitignore` and `ndkpath.example.txt` so builds no longer dirty the repository or leak local paths.

## v1.7.0 enhanced runtime upgrade

This release moves the fork beyond build compatibility and adds runtime hardening intended for long play sessions, map restarts, rapid level switching, and difficult Quest modcharts.

### Runtime isolation and crash prevention

- Hook installation is now **idempotent**: a second `late_load` or duplicate installer registration cannot install the same hook group twice.
- Hook installer names are deduplicated and installed in deterministic alphabetical order.
- Noodle runtime hooks now **fail closed** at the beginning of every level transition. An early return can no longer leave Noodle enabled from the previously played map.
- Notes, obstacles, linked-note groups, movement-provider pools, player-transform flags, trail flags, jump values and handedness state are reset between maps.
- Leaving `GameCore` also disables the runtime and clears transient state, reducing stale Unity-pointer and pooled-object problems after restarts.
- The NE + Mapping Extensions conflict guard is configurable, with safe blocking enabled by default.

### Configuration fixes

The existing configuration values are now actually used at runtime:

- `Enable note dissolve`
- `Enable mirror note dissolve`
- `Enable obstacle dissolve`
- `Reset runtime state between maps`
- `Runtime diagnostics`
- `Block NE + ME conflicts`

Disabling note or obstacle dissolve no longer gets silently ignored by hardcoded `true` values.

### Diagnostics and performance

- Logs now show hook-group count, per-map NE/ME requirement decisions, activation state, and cache-reset summaries.
- Runtime caches are cleared at predictable lifecycle boundaries rather than surviving until a later object happens to overwrite them.
- Duplicate callbacks and duplicate global registration are blocked.

### Build and QMOD reliability

- `qpm s qmod` is fully cross-platform and no longer depends on a missing PowerShell script.
- Shell scripts are checked for CRLF/`^M` corruption before packaging.
- Project versions are validated across `mod.json`, `qpm.json`, `qpm.shared.json`, and `qpm_defines.cmake`.
- The generated QMOD is integrity-tested with `unzip -t`.
- Packaging prints the QMOD size, file listing, and SHA-256 checksum.
- The QMOD uses stripped ZIP metadata (`zip -X`) for cleaner, more reproducible archives.

## Current status

Playable, but not clean enough to call finished. A lot of the awful stuff is fixed now: invisible notes, broken jump movement, bad track parenting, fake note weirdness, some replay crashes, and a bunch of obstacle timing/position problems. The remaining bugs are mostly the annoying visual polish stuff: wall materials, lighting, fog, and a few map-specific animation differences.

## Fixed in this fork

- [x] Note jump animation no longer just pops notes into existence.
- [x] Tracked / animated notes that were invisible on most modcharts now render.
- [x] Notes no longer face world origin `(0, 0, 0)` by mistake.
- [x] Start beat offset and jump distance/speed handling were corrected.
- [x] `disableBadCut*` custom data modifiers are implemented.
- [x] Restarting a Noodle chart no longer crashes in the known cases.
- [x] Replay loading no longer hard-crashes immediately on modchart replays.
- [x] BeatLeader is guarded from uploading broken/corrupted replay data from these charts.
- [x] Saber trail parenting to player tracks works.
- [x] Saber trail culling issues are patched.
- [x] Saber animation offsets work better on V3 maps.
- [x] Parent/track ordering bugs are patched for maps like `Make a Wish`.
- [x] Obstacle spawn position, duration, speed, and rotation are closer to PC behavior.
- [x] V3 custom events animate `offsetPosition`.
- [x] `offsetWorldPosition` is supported.
- [x] Double-spawned notes are fixed.
- [x] Fake notes are handled better.
- [x] CustomJSONData arcs, chains, VNJS, and related loading issues are patched.
- [x] Chroma V2 `position` / `localPosition` now account for `kNoteLinesDistance`.
- [x] Half-despawning obstacle cases are patched.
- [x] The Doppelganger-style audio drowning issue is fixed.

## Still needs work

- [ ] Keep testing real maps and move bugs into either "must fix before release" or "patch later".
- [ ] Fix scaled obstacle texture bleeding.
- [ ] Fix obstacle frame/fill dissolve mismatch.
- [ ] Improve wall outlines, probably around `ParametricBoxFakeGlowController_Refresh`.
- [ ] Fix Chroma environment/color override behavior.
- [ ] Fix broken lighting in maps like `Paradigm`, `Change of Scene`, and `BSSDHUYR Megamix 2023`.
- [ ] Look at fog support. Some maps expect it, Quest might need a fake version.
- [ ] Rework material switching instead of piling more one-off wall fixes on top.
- [x] Fix left handed mode.
- [x] Fix beatmap note / obstacle / bomb counts, including fake objects.
- [x] Make sure fake notes do not get counted as real notes on maps like `Centipede`.
- [x] Make Noodle runtime hooks activate only for Noodle-required difficulties and fail closed during transitions.
- [x] Fix environment override in scene transition hooks.

### [Recorded](https://drive.google.com/drive/folders/1XNpSEJ44uwEr9L9W3qukeGNnQX24wF0U?usp=drive_link) Maps
Dates are (year/month/day)
- [ ] [1015](https://beatsaver.com/maps/1f304) (25/12/23)
  - [x] Crashes
  - [ ] 0:00 Obstacles are missing their frame outline
  - [x] 1:35 Weird environment geometry under player (Also 1.37)
  - [ ] 1:51 Obstacle dissolve doesn't work
- [ ] [Analys](https://beatsaver.com/maps/d00c) (25/11/25)
  - [ ] 0:50 The obstacles next to the player may not be positioned correctly
  - [ ] 0:56 Track does not rotate smoothly
  - [ ] 1:34 Circle of arrows despawn or does not follow player (Also 1.37)
- [ ] [As the World Caves In](https://beatsaver.com/maps/210e3) (25/12/4)
  - [ ] [Awaiting analysis]
- [ ] [BSSDHUYR Megamix 2023](https://beatsaver.com/maps/39736) (25/12/23)
  - [x] Crashes
  - [ ] Lighting is broken
  - [ ] 1:50 Unintentional hyper walls
  - [ ] 2:40 Floor obstacles don't move along the track
  - [x] 3:20 Notes with base provider animated color are black (Also 1.37)
  - [ ] 5:12 The "2023" wall text doesn't appear in the mirror
  - [ ] 5:27 The "4" wall text doesn't glow the same as the rest of the wall text (Also 1.37)
- [x] [Centipede](https://beatsaver.com/maps/293ab) (25/12/4)
  - [x] So many notes missing
- [ ] [Change of Scene](https://beatsaver.com/maps/3f19a) (25/12/4)
  - [x] Player is slightly elevated above the track
  - [x] 0:00 Title shows as CHANGE OF SCE--:
  - [x] 1:49 Track doesn't follow player
  - [ ] [Awaiting analysis]
- [ ] [C18H27NO3 (Mawntee)](https://beatsaver.com/maps/17d7e) (25/11/26)
  - [ ] [Awaiting analysis]
- [ ] [Disaster](https://beatsaver.com/maps/1dc64) (25/12/4)
  - [ ] [Awaiting analysis]
- [ ] [Doppelganger](https://beatsaver.com/maps/d53c) (25/12/23)
  - [x] Audio being drowned out
  - [x] Obstacles are scaled weirdly
  - [x] Exaggerated note movement
  - [x] Note dissolve isn't removed
  - [x] 1:23 Obstacle is positioned weirdly
  - [ ] 1:37 Obstacle scales while dissolving (currently just disabled)
  - [x] 2:08 Obstacles are at the wrong Y position
  - [x] 3:04 Obstacle timing issues
  - [x] 3:04 Track teleports with no smoothing
- [ ] [Echo](https://beatsaver.com/maps/269a7) (25/12/4)
  - [x] Environment cubes that make up the screen are missing
  - [ ] Environment cubes that make up the screen are slightly smaller than intended
  - [ ] Notes are being reflected in a mirror
  - [ ] Environment spike things are positioned incorrectly or missing
  - [x] 2:20 Rotated tracks are missing
- [ ] [Glorious Octagon of Destiny](https://beatsaver.com/maps/26ad0) (25/11/26)
  - [x] 0:19 "VIVA" text comes in from top and bottom rather than spinning in from sides
  - [ ] 0:25 Obstacle field is missing (Also 1.37)
  - [x] 0:39 Obstacles are missing
  - [ ] 0:47 Note arrows don't disappear on cut (Also 1.37)
  - [x] 0:52 Unintentional hyper walls
  - [x] 1:16 The two tracks are positioned on top of each other
  - [x] 1:30 Unintentional fast walls
  - [x] 1:44 Track doesn't rotate with player
  - [ ] 1:44 Track position is positioned too high (Broke between 25/12/24 and 26/3/1)
  - [ ] 1:47 Wall text doesn't move
  - [x] 2:46 Some walls are unintentionally hyper, some aren't
  - [x] 3:05 Wall text is centered and doesn't scroll
  - [x] 3:14 Notes are missing
  - [x] 3:14 Obstacles are missing
  - [ ] 4:00 Many small white cubes float in the air
  - [x] 4:10 Notes and Super Hexagon obstacles are missing
  - [x] 4:31 "LEVEL FAILED" text is either further away or smaller than normal
  - [x] 4:31 Funny wall in first quadrant of recording
  - [x] 4:31 Notes are missing
  - [x] 4:52 Wall text is missing
  - [x] 5:21 Unintentional hyper walls
  - [x] 5:21 The "printing" obstacle doesn't move
  - [x] 5:33 Wall sign is far away
  - [ ] 5:33 Wall sign is difficult to see
  - [x] 5:42 Notes are missing
  - [x] 5:57 Wall arrows are on top of each other
  - [x] 5:57 Wall arrows don't duplicate (Also 1.37)
  - [ ] 6:09 Wall arrows don't fully disappear (Also 1.37)
  - [x] 6:26 Notes are positioned next to player
  - [x] 6:51 Unintentional hyper walls in Papyrus section
  - [ ] 7:27 Wall text missing letters (Also 1.37)
- [ ] [Heliov](https://beatsaver.com/maps/27ade) (25/12/23)
  - [ ] Lighting is broken
  - [ ] Fog is missing
  - [ ] 0:00 Saber trail disappears and reappears several times
  - [ ] 3:53 This environment scene can't be seen at all
  - [ ] 4:12 Bottom part of environment seems cut off
  - [ ] 4:15 Clouds are missing
- [x] [IGDWUTSWHWHMTC (QueenChief)](https://beatsaver.com/maps/20bc7) (25/11/26)
- [ ] [Lucy, The God Of Time](https://beatsaver.com/maps/15b16) (25/12/23)
  - [ ] Some walls aren't glowy enough
- [x] [Make a Wish](https://beatsaver.com/maps/1a32d) (25/12/4)
  - [x] 0:40 Unintentional hyper walls
- [ ] [Midnight Lady](https://beatsaver.com/maps/da60) (25/12/4)
  - [x] 0:46 Saxaphones have certain walls that rotate in opposite directions
  - [ ] [Awaiting analysis]
- [ ] [Noodle Animation Stuff](https://beatsaver.com/maps/1a53c) (25/12/4)
  - [ ] [Awaiting analysis]
- [ ] [NULCTRL Meiso Flip](https://beatsaver.com/maps/ed2a) (25/11/25)
  - [ ] All the white glowing walls are slightly transparent instead of solid white
  - [ ] 0:39 Walls are supposed to cover the wall text
  - [ ] 1:09 Arrows animation begins with max offset instead of slowly increasing offset
  - [ ] 1:18 Arrows are abnormally bright
  - 1:38 Note for later fixes: These walls don't have bleeding textures
- [ ] [Over Again](https://beatsaver.com/maps/2a093) (25/12/4)
  - [ ] [Awaiting analysis]
- [ ] [Paradigm](https://beatsaver.com/maps/3bbb0) (25/11/25)
  - [x] Crashes
  - [ ] Lighting is broken
  - [ ] Fog is missing (but maybe a PC only feature?)
  - [ ] 1:16 Sabers appear to fly in from above instead of from the front
  - [ ] 4:37 Head follows the sabers
  - [ ] 6:49 Something is obscuring light rays below the horizon
- [ ] [PAUSE](https://beatsaver.com/maps/17e36) (25/11/25)
  - [x] Notes are missing during the pause effect
  - [ ] 1:18 Unintentional hyper walls
- [ ] [Pictured as Perfect](https://beatsaver.com/maps/3f6e7) (25/12/4)
  - [ ] [Awaiting analysis]
- [x] [Real or Lie (Pixelguy)](https://beatsaver.com/maps/126e4) (25/11/26)
- [ ] [Signager](https://beatsaver.com/maps/12498) (26/11/25)
  - [ ] [Awaiting analysis]
- [ ] [Six Forty Seven (Reddek)](https://beatsaver.com/maps/2c89c) (25/12/4)
  - [x] All of the wall decorations are positioned very very wrong
  - [ ] [Awaiting analysis]
- [ ] [Six Forty Seven (SuperMemer417, nasafrasa)](https://beatsaver.com/maps/2a2bd) (25/11/26)
  - [ ] Environment pieces aren't being removed (Probably fixed, just needs a replay)
- [ ] [Takeoff](https://beatsaver.com/maps/2c1ff) (25/12/4)
  - [ ] Crashes
- [ ] [Team Grimoire (Salty)](https://beatsaver.com/maps/4b476) (26/1/28)
  - [ ] [Awaiting analysis]
  - [ ] [Very broken] (No longer very broken (26/3/12))
  - [x] Spinning black hole section doesn't rotate or move notes (26/2/17)
- [ ] [Try](https://beatsaver.com/maps/14d64) (25/12/4)
  - [x] 0:00 Flower is messed up
  - [ ] [Awaiting analysis]
- [ ] [Try This](https://beatsaver.com/maps/decf) (25/11/26)
  - [x] Arrow walls disappear early
  - [ ] 0:31 Unintentional hyper wall
- [x] [UUUUUUUUUUUUUUUUUUUUh](https://beatsaver.com/maps/217cf) (25/12/23)
  - [x] 2:00 Platform isn't moved down
- [ ] [Up & Down](https://beatsaver.com/maps/11cf8) (25/11/25)
  - [ ] Environment pieces aren't being removed
  - [x] 1:00 Animated notes spawn in late
- [ ] [Up & Down (Remastered)](https://beatsaver.com/maps/2c2f4) (25/12/23)
  - [x] Large column environment pieces are missing (26/1/28)
  - [x] 0:03 Track doesn't rotate with the player
  - [x] 0:33 Decorative obstacles are missing
  - [x] 0:33 Only the left decorative obstacle is missing (29/1/28)
  - [x] 0:33 Half the notes are missing
  - [x] 1:05 Notes are missing
  - [ ] 1:05 Notes don't change color
  - [x] 1:54 Tease notes are missing
- [ ] [Wavetapper](https://beatsaver.com/maps/26660) (25/12/23)
  - [ ] White background is missing


## FAQ (Frequently Asked Questions)
- Why are the wall colors not the same as PC (desaturated, transparent etc.)?
  - Due to the Quest not having distortion on walls, Noodle Extensions will attempt to change the walls materials to solid if these conditions are met:
    - Wall color alpha (controlled by Chroma) is greater than or equal 0
    - Dissolve animation is being applied
- Bombs are not coloring/are always black
  - A bug specific to Quest causes bombs to lose their color/default to black when dissolve animation is being applied. It is not certain if this bug will be fixed or worked around.
- Why does Noodle Extensions disable my Qosmetics notes/walls?
  - Qosmetics notes/walls drastically reduce performance especially in Noodle maps and also ruin the artistic experience. 
- Why do you suggest disabling Mapping Extensions? It works fine for me
  - While it _could_ work, we didn't extensively test the impact of performance or stability using Mapping Extensions. It's at your discretion if you choose to use Mapping Extensions simultaneously with Noodle.
  - While some maps do "require" both Noodle Extensions and Mapping Extensions, this is not a supported scenario in either PC or Quest and should be discouraged.
- Where are Tracks/CustomJSONData QMod downloads?
  - Noodle Extensions and Chroma will download these dependencies automatically
- Noodle Extensions isn't loading/working and PinkCore says I don't have the mod installed
  - Try to reinstall Noodle Extensions and update Chroma to at least version 2.5.7 or newer.
- I found a map that doesn't work!!!!11!!11!/Noodle is missing a feature
  - You most likely downloaded the wrong map as most Noodle features are supported (no exceptions except the TODO)
  - In the case you are absolutely certain you found a bug/missing feature, report it in GitHub issues along with a log and steps to reproduce the issue. Footage of said map is also recommended being provided.

## Building from Source

Install `qpm-rust`/`qpm`, Ninja, CMake, and the Android NDK selected by `qpm.json`.
The scripts can find an NDK installed by QPM-RS automatically. You can also set `ANDROID_NDK_HOME`, pass `--ndk`, or copy `ndkpath.example.txt` to the ignored local file `ndkpath.txt`.

### macOS / Linux / Git Bash

```sh
qpm-rust restore
qpm-rust cache legacy-fix
bash ./scripts/validateProject.sh
bash ./scripts/build.sh --release
bash ./scripts/buildQMOD.sh
```

The equivalent QPM shortcuts are:

```sh
qpm s build
qpm s qmod
```

### PowerShell

```powershell
qpm-rust restore
qpm-rust cache legacy-fix
pwsh ./scripts/build.ps1 -release
bash ./scripts/buildQMOD.sh
```

The output library is `build/libnoodleextensions.so`; the packaged mod is `NoodleExtensions.qmod`.

## Notes for testers

If a map still breaks, grab a log and write down the map, difficulty, timestamp, and what actually looked wrong. The TODO list at the top is the current cleanup list; the recorded-map section is just test history.
