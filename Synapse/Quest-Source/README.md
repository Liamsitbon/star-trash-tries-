# Synapse Quest port — native core milestone, not a playable mod

Target: Beat Saber Quest standalone **1.40.8_7379**. Branch: `synapse-quest-port`.
This is **not** the full Synapse port, not an installable QMOD, and not a replacement
for the unidentified Synapse library previously found on the user's Quest.

Behavioral authority: the supplied `Synapse-master.zip`, SHA-256
`4986a19f90c638cf98b3a02c152eb5b71e79be1fac6756a7285c9393ac6efd6b`.
The extracted upstream remains untouched in the local `Synapse/Upstream-2026-09-09`
folder. Only the new portable core, tests and MIT notices are published here;
no personal upstream appsettings, Windows sample bundles or maps are uploaded.

## Implemented and tested

- Little-endian framing, bounded 16KiB messages, .NET 7-bit UTF8 string lengths,
  int32/ushort/bool/IEEE float encoding, strict truncated/invalid input checks.
- All nine client opcode layouts have byte-for-byte tests against an independent
  .NET BinaryWriter oracle, including Hebrew/emoji and long strings.
- All fourteen server opcode payload layouts are decoded. Status/chat/leaderboard
  JSON is retained verbatim for the future typed model/UI integration, not discarded.
- Fragmented/coalesced TCP stream decoder, including one-byte headers, timeout,
  poisoned-stream handling, and compatibility with old zero-padded PC packets.
- Bounded owned-message queue with generation checks and explicit overflow signal;
  it contains no Unity objects and cannot expose a reused receive buffer to the UI.
- Thirty-sample server clock, matched ping responses, seconds-based 10s timeout.
  Timeout is not treated as a successful synchronization sample.
- Exact-artifact/generation readiness gate: an old download callback cannot mark
  the new map ready or trigger scene teardown before verified preparation.

Host CTest, 10,000 malformed inputs under ASan/UBSan, .NET oracle comparison and
Android ARM64 compilation passed locally. No server connection, real auth,
gameplay transition or headset behavior was tested. The ARM64 output is a static
component library, **not** a mod to sideload.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/compare_dotnet.py
```

The oracle needs .NET 10, uses synthetic credentials and never opens a network
connection. Native code requires C++20; it does not depend on a PC Unity DLL.

## Remaining full-port inventory — not waived

1. Cancellable TCP/DNS/reconnect transport, real Quest platform authentication,
   listing/status JSON models, backend compatibility and failure reporting.
2. Quest BSML menu, event banner/takeover, countdown, divisions, chat/profanity/
   opt-out, join/leave/ban messages, notifications, leaderboards and moderation UI.
3. Verified download/cache identity, encrypted map AES/MD5 compatibility, bounded
   traversal-safe ZIP extraction, SongCore integration and prepare-next-before-
   teardown scene lifecycle. Server-synchronized start, pause/stop and elimination.
4. Scoring/resubmission/ruleset/modifier/Heck settings integration and required-mod
   installer compatibility. Do not install or trust unknown QMODs automatically.
5. Android lobby/takeover prefabs, matching audio/cinematics and camera/depth effects.
   Windows bundles in the supplied PC sample are **not** Quest assets. This needs
   Android exports and a platform-aware listing selector, not just a version match.
6. Preserve original Server/Listing/TestClient and all administration commands as
   separate backend/tools. These are not PC DLLs to put inside the Quest QMOD.

The previous PC authentication assumptions are not proof of secure Quest identity.
Do not forge a platform/session token or ship an always-authorized bypass. Do not
claim the entire mod is ported from this network-core milestone.
