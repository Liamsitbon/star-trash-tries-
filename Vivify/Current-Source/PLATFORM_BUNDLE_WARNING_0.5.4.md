# Vivify Quest 0.5.4 platform-bundle warning

## Trigger

The warning appears only when all of the following are true:

1. The selected difficulty requires `Vivify`.
2. `bundleAndroid2021.vivify` is absent.
3. `Info.dat` has no Android bundle checksum.
4. A Windows bundle checksum or a local `*windows*.vivify` file is present.

## In-game result

SongCore's `?` Requirements panel receives this non-blocking warning:

> Quest warning: this map only provides a Windows Vivify bundle. No Android bundle was found. Visuals may be missing, incorrect, white/grey, or uncomfortable, but playback is allowed.

The Play button remains enabled. Vivify first probes local `.vivify` files in case one is Android-compatible despite its filename. If none can load, gameplay continues without the unavailable assets.
