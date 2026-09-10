# Synapse Quest port — protocol, transport and session core, not a playable mod

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
- All fourteen server opcode payload layouts are decoded. All original fields in
  status/stages/maps/rulesets/chat/leaderboard/listing/lobby/takeover/required-mod
  JSON have typed native models. Null/false/missing semantics are preserved.
- Bounded JSON parsing (wire 16KiB, HTTP listing 1MiB, nesting 32), duplicate-key,
  type, integer-range and nonfinite checks. Parser errors never echo source data.
  Unknown JSON fields remain accepted; unknown stage types are rejected explicitly.
- Native numeric IPv4/IPv6 TCP transport: nonblocking socket, worker-owned lifetime,
  asynchronous cancellation, 5s connect timeout, 2s packet/write timeout, bounded
  retries and 64-entry incoming/outgoing queues. No Unity calls on the worker.
- Connection identity guards all sends; pending outgoing frames are cleared on
  retry, never replayed into a new auth session. Server disconnect and malformed
  packets are terminal; queue overflow is visible even when no more events fit.
- Loopback tests cover fragmented/coalesced messages, bidirectional framing,
  reconnect identity, cancellation, stalled headers, malformed data and overflow.
- Fragmented/coalesced TCP stream decoder, including one-byte headers, timeout,
  poisoned-stream handling, and compatibility with old zero-padded PC packets.
- Bounded owned-message queue with generation checks and explicit overflow signal;
  it contains no Unity objects and cannot expose a reused receive buffer to the UI.
- Thirty-sample server clock, matched ping responses, seconds-based 10s timeout.
  Timeout is not treated as a successful synchronization sample.
  Transport events retain worker receipt time in the shared process-relative
  monotonic clock: a delayed UI must use `receivedAt` for Pong, not its dequeue
  time. Only one ping may remain outstanding, so repeated sends cannot extend
  a missing reply's timeout forever.
- Exact-artifact/generation readiness gate: an old download callback cannot mark
  the new map ready or trigger scene teardown before verified preparation.
- Owner-thread session reducer connects all fourteen decoded server messages to
  typed notices, immutable status snapshots, counts, acknowledged scores and the
  thirty-sample clock. It does not call Unity or open a connection.
- Session epoch plus transport connection identity reject old messages, auth
  callbacks and map completions even if a replacement transport reuses an id.
  Same-index changes to a map's download/hash/key/ruleset invalidate preparation.
- Authenticated state requires an explicit submission marker plus the current
  server ACK; repeated submissions cannot extend the eight-second response
  deadline. There is no fake token/provider. A real Quest auth adapter, retry
  scheduler, secure deployment and authenticated settings sends remain pending.
- Sixty-four owned notices, atomic status change sets and out-of-band terminal
  state prevent queue overflow from leaving partially applied transitions.
  StopLevel cancels prepared transitions. A new scheduled start can prepare again.
- Upstream's InvalidateScores refreshes leaderboard data; it does not erase a
  submission acknowledgement. Session restart does clear the old session's scores.

Host CTest, 10,000 malformed inputs under ASan/UBSan, .NET oracle comparison and
Android ARM64 compilation passed locally. No real server connection, real auth,
gameplay transition or headset behavior was tested. The ARM64 output is a static
component library, **not** a mod to sideload.
The host transport suite also passed under ThreadSanitizer. This covers exercised
thread interleavings, not a proof of all possible races or headset behavior.
The session suite additionally passed under ASan/UBSan: all server opcodes,
duplicate authentication ACKs, pre-auth rejection, stalled-UI Pong timestamps,
same-index map replacement, stale completions, stop/restart and full notice queues.

### Session adapter contract

The future Quest adapter owns `Session` on the Unity main thread. Once transport
reports a new connection, call `Begin(connection)` and retain its returned identity
in every asynchronous callback. Capture `TransportClockSeconds()` immediately
before enqueueing a real auth frame; only if `Send` accepts it, pass that timestamp
to `AuthenticationSent`. The reducer stores no credentials and does not send auth.
Provider acquisition needs its own cancellation/timeout policy.

Drain received messages with their original worker timestamps before calling
`Tick`. Terminal transport status must call `Close(..., TransportLost)`. A closed
or overflowing session must in turn stop the transport. After an authenticated
notice, the adapter sends opt-in chatter/division settings and schedules pings.
Failure to queue any outbound packet must be surfaced, not silently retried into
another connection. All server-supplied UI strings are untrusted text, not markup.

`BeginPreparation(identity, revision, artifact)` returns a one-use generation
ticket. The future downloader must establish the exact artifact identity, verify
the download/decryption/extraction and resolve the requested SongCore difficulty
before `CompletePreparation(ticket, true)`. The boolean is a caller contract, not
a file verifier implemented by this core. Immediately before any scene teardown,
recheck `CanTransition(ticket)` and the current server schedule/division. Receiving
an earlier `MapPrepared` notice is not permission to start a now-obsolete map.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/compare_dotnet.py
```

The oracle needs .NET 10, uses synthetic credentials and never opens a network
connection. The transport tests open ephemeral loopback sockets only. Native code
requires C++20; it does not depend on a PC Unity DLL. The JSON parser is vendored
nlohmann/json 3.12.0, SHA-256-pinned with upstream MIT license/provenance retained.

The wire transport retains upstream's **unencrypted TCP** compatibility. It has
not been approved for sending real platform/session tokens over an untrusted
network. A secure authenticated transport/deployment policy and real Quest auth
remain integration requirements, not an always-authorized bypass. No credentials
are requested by these tests and no connection is opened at library startup.

Asset bundle listings without a `platform` tag are classified **unknown**, not
automatically Android just because their game version matches. An explicit
`platform: "android"` / `"quest"` tag helps selection but does not replace inspecting
the downloaded bundle before loading it. Existing PC listing fields are retained.

## Remaining full-port inventory — not waived

1. Cancellable hostname DNS adapter, real Quest platform authentication, secure
   backend compatibility policy, real auth/retry/settings orchestration and failure
   UI. The portable session reducer above is implemented, not yet wired to Quest.
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
