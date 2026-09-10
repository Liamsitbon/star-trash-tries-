#include "Synapse/Session.hpp"
#include <algorithm>
#include <limits>

namespace Synapse::Quest {
namespace {
bool TimeChanged(float a,float b) { return std::abs(static_cast<double>(a)-b)>.001; }
std::uint32_t Changes(Models::Status const& old,Models::Status const& value) {
  std::uint32_t flags=0;
  if(old.motd!=value.motd) flags|=MotdChanged;
  if(old.stage.index()!=value.stage.index()) flags|=StageChanged;
  if(auto intro=std::get_if<Models::IntroStage>(&value.stage)) {
    auto before=std::get_if<Models::IntroStage>(&old.stage);
    if(!before || before->url!=intro->url) flags|=IntroUrlChanged;
    if(!before || TimeChanged(before->startTime,intro->startTime)) flags|=IntroTimeChanged;
  } else if(auto play=std::get_if<Models::PlayStage>(&value.stage)) {
    auto before=std::get_if<Models::PlayStage>(&old.stage);
    // Same index is not proof of the same map. Covers download/hash/key/ruleset
    // changes and a revised difficulty as well as metadata updates.
    if(!before || before->index!=play->index || before->map!=play->map) flags|=MapChanged;
    if(!before || TimeChanged(before->startTime,play->startTime)) flags|=StartTimeChanged;
    if(!before || before->playerScore!=play->playerScore) flags|=PlayerScoreChanged;
    if(!before || before->eliminated!=play->eliminated) flags|=EliminatedChanged;
  } else if(auto finish=std::get_if<Models::FinishStage>(&value.stage)) {
    auto before=std::get_if<Models::FinishStage>(&old.stage);
    if(!before || before->url!=finish->url) flags|=FinishUrlChanged;
    if(!before || before->mapCount!=finish->mapCount) flags|=FinishCountChanged;
  }
  return flags;
}
} // namespace

bool Session::Current(SessionIdentity key) const {
  return key==identity_ && key.epoch && key.connection &&
    phase_!=SessionPhase::Idle && phase_!=SessionPhase::Closed;
}
void Session::ClearQueue() {
  for(auto& notice:notices_) notice.reset();
  head_=size_=0;
}
void Session::ResetMap() {
  ++revision_;
  preparation_.Cancel(); ticket_.reset(); stopped_=false;
}
SessionIdentity Session::Begin(std::uint64_t connection) {
  if(!connection) throw ProtocolError("zero session connection");
  if(identity_.epoch==std::numeric_limits<std::uint64_t>::max())
    throw ProtocolError("session epoch exhausted");
  ++identity_.epoch; identity_.connection=connection;
  phase_=SessionPhase::AwaitingAuthentication;
  ClearQueue(); closed_.reset(); authSentAt_.reset();
  status_={}; counts_={}; scores_.fill(std::nullopt); clock_.Reset(); ResetMap();
  Emit(AuthenticationRequested{});
  return identity_;
}
bool Session::Emit(SessionNoticePayload payload) {
  if(size_==Capacity) {
    Close(identity_,SessionCloseReason::QueueOverflow);
    return false;
  }
  notices_[(head_+size_)%Capacity]=SessionNotice{identity_,std::move(payload)};
  ++size_; return true;
}
void Session::Close(SessionIdentity key,SessionCloseReason reason,std::uint8_t code) {
  if(!Current(key)) return;
  phase_=SessionPhase::Closed; closed_=SessionClosed{reason,code};
  authSentAt_.reset(); clock_.Reset(); ResetMap(); status_={}; counts_={};
  scores_.fill(std::nullopt); ClearQueue();
  // Terminal state is out of band as well; overflow cannot hide a disconnect or
  // leave stale queued callbacks able to tear down the current lobby.
  notices_[0]=SessionNotice{identity_,*closed_}; size_=1;
}
std::optional<SessionNotice> Session::Pop() {
  if(!size_) return {};
  auto notice=std::move(notices_[head_]); notices_[head_].reset();
  head_=(head_+1)%Capacity; --size_; return notice;
}
bool Session::AuthenticationSent(SessionIdentity key,double now) {
  if(!Current(key) || phase_!=SessionPhase::AwaitingAuthentication ||
     !std::isfinite(now) || now<0) return false;
  if(!authSentAt_) authSentAt_=now;
  return true;
}
void Session::Apply(Models::Status value) {
  auto flags=Changes(status_,value);
  if(!flags) { status_=std::move(value); return; }
  // Reserve notification capacity before changing preparation/status. A single
  // immutable change set avoids exposing half of a multi-field stage update.
  if(size_==Capacity) { Close(identity_,SessionCloseReason::QueueOverflow); return; }
  if((flags & MapChanged) || ((flags & StageChanged) &&
      std::holds_alternative<Models::PlayStage>(status_.stage))) ResetMap();
  if(flags & StartTimeChanged) stopped_=false;
  status_=std::move(value);
  Emit(StatusUpdated{status_,flags,revision_});
}
bool Session::Accept(SessionIdentity key,TransportEvent const& event) {
  if(!Current(key) || event.connection!=key.connection) return false;
  if(event.kind!=TransportEvent::Kind::Message) return false;
  if(!std::isfinite(event.receivedAt) || event.receivedAt<0) {
    Close(key,SessionCloseReason::InvalidMessage); return false;
  }
  auto const& packet=event.message;
  if(packet.text.size()>MaxBody || !ValidUtf8(packet.text)) {
    Close(key,SessionCloseReason::InvalidMessage); return false;
  }
  if(packet.opcode==FromServer::Disconnect) {
    Close(key,SessionCloseReason::ServerDisconnect,packet.disconnectCode); return true;
  }
  if(packet.opcode==FromServer::Authenticated) {
    if(phase_==SessionPhase::Authenticated) return true; // idempotent server ACK
    if(!authSentAt_ || event.receivedAt<*authSentAt_) {
      Close(key,SessionCloseReason::InvalidMessage); return false;
    }
    if(event.receivedAt-*authSentAt_>=8) {
      Close(key,SessionCloseReason::AuthenticationTimeout); return false;
    }
    if(!Emit(SessionAuthenticated{})) return false;
    phase_=SessionPhase::Authenticated; authSentAt_.reset(); return true;
  }
  // A refusal can be the server's authentication error; it is not success.
  if(packet.opcode==FromServer::RefusedPacket) return Emit(PacketRefused{packet.text});
  if(phase_!=SessionPhase::Authenticated) {
    Close(key,SessionCloseReason::InvalidMessage); return false;
  }
  try {
    switch(packet.opcode) {
      case FromServer::Status: Apply(Models::ParseStatus(packet.text)); break;
      case FromServer::ChatMessage: Emit(Models::ParseChat(packet.text)); break;
      case FromServer::LeaderboardScores: Emit(Models::ParseLeaderboard(packet.text)); break;
      case FromServer::Ping:
        if(clock_.Pong(packet.clientTime,packet.serverTime,event.receivedAt)) Emit(ClockUpdated{});
        break;
      case FromServer::PlayerCount:
        if(Emit(PlayerCounts{packet.chatters,packet.players})) counts_={packet.chatters,packet.players};
        break;
      case FromServer::UserJoin: Emit(UserPresence{true,packet.text}); break;
      case FromServer::UserLeave: Emit(UserPresence{false,packet.text}); break;
      case FromServer::UserBanned: Emit(UserBanned{packet.text}); break;
      case FromServer::AcknowledgeScore:
        if(Emit(ScoreAcknowledged{packet.index,packet.score})) scores_[packet.index]=packet.score;
        break;
      case FromServer::InvalidateScores: Emit(ScoresInvalidated{packet.index}); break;
      case FromServer::StopLevel:
        if(Emit(LevelStopped{})) { preparation_.Cancel(); ticket_.reset(); stopped_=true; }
        break;
      default: Close(key,SessionCloseReason::InvalidMessage); return false;
    }
  } catch(ProtocolError const&) {
    // Never forward JSON, auth tokens, usernames or server text into error logs.
    Close(key,SessionCloseReason::InvalidMessage); return false;
  }
  return phase_!=SessionPhase::Closed;
}
void Session::Tick(SessionIdentity key,double now) {
  if(!Current(key) || !std::isfinite(now) || now<0) return;
  if(phase_==SessionPhase::AwaitingAuthentication && authSentAt_ && now-*authSentAt_>=8)
    Close(key,SessionCloseReason::AuthenticationTimeout);
  else if(phase_==SessionPhase::Authenticated) clock_.TimedOut(now);
}
std::optional<std::vector<std::uint8_t>> Session::MakePing(SessionIdentity key,double now) {
  if(!Current(key) || phase_!=SessionPhase::Authenticated || !std::isfinite(now) ||
     now<0 || now>std::numeric_limits<float>::max()) return {};
  float timestamp=static_cast<float>(now);
  if(!clock_.BeginPing(timestamp)) return {};
  return Writer(ToServer::Ping).Float(timestamp).Finish();
}
std::optional<double> Session::ServerTime(double now) const {
  if(phase_!=SessionPhase::Authenticated) return {};
  return clock_.Time(now);
}
std::optional<PreparationTicket> Session::BeginPreparation(SessionIdentity key,
    std::uint64_t revision,std::string artifact) {
  if(!Current(key) || phase_!=SessionPhase::Authenticated || revision!=revision_ ||
     !std::holds_alternative<Models::PlayStage>(status_.stage) || stopped_ ||
     artifact.empty() || artifact.size()>4096 || !ValidUtf8(artifact)) return {};
  auto generation=preparation_.Begin(artifact);
  ticket_=PreparationTicket{key,revision,generation,std::move(artifact)};
  return ticket_;
}
bool Session::CompletePreparation(PreparationTicket const& ticket,bool verified) {
  if(!Current(ticket.session) || phase_!=SessionPhase::Authenticated || stopped_ ||
     !ticket_ || ticket!=*ticket_ || !verified) return false;
  if(preparation_.CanTransition()) return true; // callback completion is idempotent
  if(size_==Capacity) { Close(identity_,SessionCloseReason::QueueOverflow); return false; }
  if(!preparation_.Complete(ticket.generation,ticket.artifact,verified)) return false;
  return Emit(MapPrepared{ticket});
}
bool Session::CanTransition(PreparationTicket const& ticket) const {
  return Current(ticket.session) && phase_==SessionPhase::Authenticated && !stopped_ &&
    std::holds_alternative<Models::PlayStage>(status_.stage) && ticket_ && ticket==*ticket_ &&
    preparation_.CanTransition();
}
} // namespace Synapse::Quest
