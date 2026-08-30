# Third-party notices

Cinema Quest implements local map-video compatibility for the `cinema-video.json`
and legacy `video.json` formats used by Beat Saber Cinema.

The reference project is BeatSaberCinema by the Beat Saber Cinema contributors:
https://github.com/Kevga/BeatSaberCinema

The 0.2.0 Quest rewrite was audited against upstream commit
`9c8bcb8ef8c78e0e7655b558ec4b062276b3409f` (2024-11-13). This is a modified,
unofficial Android/Quest port, not an upstream BeatSaberCinema release.

BeatSaberCinema is distributed under GNU General Public License version 3. Cinema
Quest is likewise distributed under GPL-3.0 and contains no downloader, ffmpeg,
Windows DLL, executable, or other PC runtime payload.

`include/QuestInterop.hpp` is an original, optional SongCore capability shim by
Liam Sitbon. It is distributed under the MIT License; the full notice is in
`LICENSES/QuestModInterop-MIT.txt`. It contains no BeatSaberCinema, Vivify,
Noodle Extensions or Nexora implementation code.
