# External video selection (Quest candidate)

Open **Mod Settings → Nexora**. The existing map-embedded video/DAT workflow
remains the default. No files are imported simply by opening the menu,
browsing a folder, or selecting a song/video.

1. **Choose external video**: browse Movies, Download, Storage, and subfolders.
   Files must be accessible to Beat Saber on the headset. No new storage
   permission is silently granted. MP4/M4V/MOV/WebM extensions are listed;
   codec support is still determined by the actual Quest decoder.
2. **Choose installed song**: select from SongCore plus the game's loaded
   catalogue. The list is searchable and paginated. Native DLC ownership,
   missing requirements, and difficulty checks are not bypassed.
3. Optionally adjust video time offset and extra yaw. Positive offset starts
   further into the video; Beat Saber's song audio remains authoritative.
4. **Custom: use in place + open song** binds this video to this exact song
   for the current game session, closes settings, and opens normal Solo song
   selection. Choose difficulty, then **Play**. Decoder prewarm uses the
   existing SongCore play gate. It does not auto-start an unknown difficulty.
5. **Normal map mode** clears the override and restores authored Nexora events.

Custom uses the video at its original absolute local path. It does not copy,
move, rename, delete, import, or modify it. It never edits Info.dat or any
difficulty DAT. Selecting a different song does not apply the binding to it.
The binding is intentionally session-local, not silently persisted into maps.
Authored Nexora events are overridden only during that explicitly selected
Custom play. Vivify/Noodle callbacks and gameplay data are not rewritten.

## Optional explicit copy

**Copy to map...** first displays the exact destination, then requires a
second **Confirm COPY** press. It copies into the selected installed custom
map as `Nexora-Custom-<original filename>`. The original stays where it was.
Existing files and symlinks are never overwritten. Failed/cancelled partial
copies are removed. The original map data and other assets stay untouched.

After copying, the new copy is selected in the menu. Press **Custom** to play
using that copy. Merely copying does not rewrite a DAT or implicitly enable
Nexora. OST/DLC songs can use Custom in-place playback but cannot receive a
copy because they have no writable custom-map directory.

File enumeration/copying runs on one owned worker, not every frame. The UI
shows eight results per page and checks Unity-object lifetime before applying
completion results. The runtime accepts external paths only through the C++
user-selection path; DAT `media` remains map-contained and rejects absolute
paths/traversal/URLs.

## Evidence and limits

Host tests cover in-place/no-map-write selection, exact song identity, clear,
missing/invalid files, cancelled copy, byte-for-byte copy, source retention,
exclusive no-overwrite, and existing symlink protection. These are not Quest
menu, storage-permission, decoder, pause/seek, or gameplay proof. The exact
QMOD needs a headset playtest, including OST, custom maps and a scene restart.

The UI is built against [Quest BSML](https://github.com/bsq-ports/Quest-BSML)
0.4.55 and [Quest SongCore](https://github.com/raineaeternal/Quest-SongCore)
1.1.26 APIs plus the 1.40.8 generated game headers. The settings-close path
uses BSML's own Cancel method; playback uses the game's Solo flow, not a
copied PC Cinema level-start implementation.
