#pragma once
// MIT. Owner-thread session reducer; no Unity objects, sockets or credentials.
#include "Models.hpp"
#include "SessionState.hpp"
#include "TcpTransport.hpp"

namespace Synapse::Quest {
struct SessionIdentity {
  std::uint64_t epoch=0, connection=0;
  bool operator==(SessionIdentity const&) const = default;
};
enum class SessionPhase { Idle, AwaitingAuthentication, Authenticated, Closed };
enum class SessionCloseReason {
  Cancelled, TransportLost, ServerDisconnect, AuthenticationTimeout,
  InvalidMessage, QueueOverflow
};
enum StatusChange : std::uint32_t {
  MotdChanged=1u<<0, StageChanged=1u<<1, IntroUrlChanged=1u<<2,
  IntroTimeChanged=1u<<3, MapChanged=1u<<4, StartTimeChanged=1u<<5,
  PlayerScoreChanged=1u<<6, EliminatedChanged=1u<<7,
  FinishUrlChanged=1u<<8, FinishCountChanged=1u<<9
};
struct AuthenticationRequested {};
struct SessionAuthenticated {};
struct SessionClosed { SessionCloseReason reason; std::uint8_t disconnectCode=0; };
struct StatusUpdated {
  Models::Status value;
  std::uint32_t changes=0;
  std::uint64_t mapRevision=0;
};
struct ClockUpdated {};
struct PlayerCounts { std::uint16_t chatters=0, players=0; };
struct UserPresence { bool joined=false; std::string username; };
struct UserBanned { std::string message; };
struct PacketRefused { std::string message; };
struct ScoreAcknowledged { std::uint8_t index=0; std::int32_t score=0; };
// Upstream invalidates the leaderboard view, NOT the submission acknowledgement.
struct ScoresInvalidated { std::uint8_t index=0; };
struct LevelStopped {};
struct PreparationTicket {
  SessionIdentity session;
  std::uint64_t mapRevision=0, generation=0;
  std::string artifact;
  bool operator==(PreparationTicket const&) const = default;
};
struct MapPrepared { PreparationTicket ticket; };
using SessionNoticePayload=std::variant<AuthenticationRequested,SessionAuthenticated,
  SessionClosed,StatusUpdated,ClockUpdated,PlayerCounts,UserPresence,UserBanned,
  PacketRefused,ScoreAcknowledged,ScoresInvalidated,LevelStopped,MapPrepared,
  Models::ChatMessage,Models::LeaderboardScores>;
struct SessionNotice { SessionIdentity session; SessionNoticePayload payload; };

class Session {
  static constexpr std::size_t Capacity=64;
  SessionIdentity identity_;
  SessionPhase phase_=SessionPhase::Idle;
  std::optional<SessionClosed> closed_;
  std::optional<double> authSentAt_;
  Models::Status status_;
  PlayerCounts counts_;
  std::array<std::optional<std::int32_t>,256> scores_{};
  ServerClock clock_;
  PreparedMapGate preparation_;
  std::optional<PreparationTicket> ticket_;
  std::uint64_t revision_=0;
  bool stopped_=false;
  std::array<std::optional<SessionNotice>,Capacity> notices_;
  std::size_t head_=0, size_=0;

  bool Current(SessionIdentity) const;
  void ClearQueue();
  void ResetMap();
  bool Emit(SessionNoticePayload);
  void Apply(Models::Status);
public:
  Session()=default;
  Session(Session const&)=delete;
  Session& operator=(Session const&)=delete;
  // Call exactly once for each newly connected transport. Epoch also protects
  // against a replacement transport reusing a numeric connection id.
  SessionIdentity Begin(std::uint64_t connection);
  // Only the real auth adapter may call this AFTER transport accepted its frame.
  // No token is stored here. Repeated submissions cannot extend the 8s deadline
  // (upstream attempts authentication four times, two seconds apart).
  bool AuthenticationSent(SessionIdentity,double now);
  // Connected events do not implicitly reset a session. Drain transport messages
  // before Tick; use worker receivedAt, not UI dequeue time, for Ping/auth timing.
  bool Accept(SessionIdentity,TransportEvent const&);
  void Tick(SessionIdentity,double now);
  void Close(SessionIdentity,SessionCloseReason,std::uint8_t disconnectCode=0);
  std::optional<SessionNotice> Pop();
  SessionIdentity Identity() const { return identity_; }
  SessionPhase Phase() const { return phase_; }
  std::optional<SessionClosed> Closed() const { return closed_; }
  Models::Status const& Status() const { return status_; }
  PlayerCounts Counts() const { return counts_; }
  std::optional<std::int32_t> AcknowledgedScore(std::uint8_t index) const { return scores_[index]; }
  std::uint64_t MapRevision() const { return revision_; }

  // Failure to enqueue/send this frame must be handled by the transport adapter.
  // Only one outstanding ping. Tick expires it; rejected Pong is never a sample.
  std::optional<std::vector<std::uint8_t>> MakePing(SessionIdentity,double now);
  std::optional<double> ServerTime(double now) const;
  std::optional<PreparationTicket> BeginPreparation(SessionIdentity,std::uint64_t revision,
                                                  std::string artifact);
  bool CompletePreparation(PreparationTicket const&,bool verified);
  bool CanTransition(PreparationTicket const&) const;
};
} // namespace Synapse::Quest
