#pragma once
#include "Protocol.hpp"
#include <memory>
#include <optional>
namespace Synapse::Quest {
// Shared process-relative monotonic seconds. Use this same clock for ping sends
// and receipt timestamps; never compare wall-clock or Unity song time to it.
double TransportClockSeconds();
struct Endpoint {
  // Original game's listing uses IPAddress. Hostname DNS is a separate, pending
  // cancellable adapter, not a blocking call hidden in the game thread.
  std::string ip;
  std::uint16_t port=0;
  std::uint32_t ipv6Scope=0;
};
struct TransportOptions { std::uint8_t attempts=3; double connectTimeoutSeconds=5, retryDelaySeconds=2; };
enum class ConnectionState { Idle, Connecting, Connected, RetryDelay, Stopped, Failed };
enum class TransportError {
  None, Cancelled, ConnectFailed, ConnectTimeout, PeerClosed, SocketError,
  MalformedPacket, PacketTimeout, SendTimeout, QueueOverflow, ServerDisconnect
};
struct TransportStatus {
  ConnectionState state=ConnectionState::Idle;
  TransportError error=TransportError::None;
  std::uint64_t connection=0;
  std::uint8_t attempt=0;
};
struct TransportEvent {
  enum class Kind { Connected, Message } kind=Kind::Connected;
  std::uint64_t connection=0;
  ServerMessage message;
  double receivedAt=0; // Captured by the worker, not when a stalled UI pops it.
};
class TcpTransport {
  struct Impl;
  std::unique_ptr<Impl> impl_;
public:
  TcpTransport();
  ~TcpTransport();
  TcpTransport(TcpTransport const&)=delete;
  TcpTransport& operator=(TcpTransport const&)=delete;
  // Single caller thread (later Unity main). IO uses one worker, 50ms bounded
  // polls and no Unity references. Start never joins a running worker. Destructor
  // cancels and joins the non-DNS worker; Stop itself is asynchronous.
  bool Start(Endpoint,TransportOptions={});
  void Stop();
  TransportStatus Status() const;
  std::optional<TransportEvent> Pop();
  // Requires the Connected event's identity. Old frames are never replayed on
  // reconnect. Caller must handle false (queue full/stale/disconnected/invalid).
  bool Send(std::uint64_t connection,std::vector<std::uint8_t> frame);
};
} // namespace Synapse::Quest
