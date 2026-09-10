#include "Synapse/Session.hpp"
#include <cassert>
#include <iostream>

using namespace Synapse::Quest;
TransportEvent Packet(SessionIdentity id,FromServer opcode,std::string text={},double at=1) {
  TransportEvent e; e.kind=TransportEvent::Kind::Message; e.connection=id.connection;
  e.message.opcode=opcode; e.message.text=std::move(text); e.receivedAt=at; return e;
}
template<class T> T Take(Session& s) {
  auto n=s.Pop(); assert(n && n->session==s.Identity());
  assert(std::holds_alternative<T>(n->payload)); return std::get<T>(std::move(n->payload));
}
SessionIdentity Login(Session& s,std::uint64_t connection=1) {
  auto id=s.Begin(connection); Take<AuthenticationRequested>(s);
  assert(!s.ServerTime(1));
  assert(s.AuthenticationSent(id,0));
  assert(s.Accept(id,Packet(id,FromServer::Authenticated)));
  Take<SessionAuthenticated>(s); assert(!s.Pop());
  return id;
}
std::string Play(std::string hash="artifact-a",int index=0,float start=12) {
  return R"({"motd":"Hello","stage":{"name":"play","index":)"+std::to_string(index)+
    R"(,"startTime":)"+std::to_string(start)+R"(,"map":{"name":"Dynasty",
      "keys":[{"characteristic":"Standard","difficulty":4}],
      "downloads":[{"gameVersion":"1.40.8_7379","url":"https://example.invalid/map","hash":")"+hash+R"("}]}}})";
}
StatusUpdated SetPlay(Session& s,SessionIdentity id,std::string hash="artifact-a",int index=0,float start=12) {
  assert(s.Accept(id,Packet(id,FromServer::Status,Play(hash,index,start))));
  return Take<StatusUpdated>(s);
}
int main() {
  // Fourteen server opcodes retain their payload and source behavior.
  Session s; auto id=Login(s);
  assert(s.Accept(id,Packet(id,FromServer::Authenticated))); assert(!s.Pop());
  assert(s.Accept(id,Packet(id,FromServer::RefusedPacket,"reason")));
  assert(Take<PacketRefused>(s).message=="reason");
  auto counts=Packet(id,FromServer::PlayerCount); counts.message.chatters=7; counts.message.players=18;
  assert(s.Accept(id,counts)); assert(Take<PlayerCounts>(s).players==18 && s.Counts().chatters==7);
  auto first=SetPlay(s,id);
  assert((first.changes & StageChanged) && (first.changes & MapChanged) && (first.changes & MotdChanged));
  assert(std::get<Models::PlayStage>(s.Status().stage).map.name=="Dynasty");
  assert(s.Accept(id,Packet(id,FromServer::Status,Play()))); assert(!s.Pop());
  assert(s.Accept(id,Packet(id,FromServer::ChatMessage,R"({"username":"Liam","message":"שלום","type":2})")));
  assert(Take<Models::ChatMessage>(s).message=="שלום");
  assert(s.Accept(id,Packet(id,FromServer::UserJoin,"New user")));
  auto joined=Take<UserPresence>(s); assert(joined.joined && joined.username=="New user");
  assert(s.Accept(id,Packet(id,FromServer::UserLeave,"Old user"))); assert(!Take<UserPresence>(s).joined);
  assert(s.Accept(id,Packet(id,FromServer::UserBanned,"ban notice")));
  assert(Take<UserBanned>(s).message=="ban notice" && s.Phase()==SessionPhase::Authenticated);
  auto ack=Packet(id,FromServer::AcknowledgeScore); ack.message.index=255; ack.message.score=1234;
  assert(s.Accept(id,ack)); assert(Take<ScoreAcknowledged>(s).score==1234);
  assert(s.AcknowledgedScore(255)==1234 && !s.AcknowledgedScore(0));
  auto invalid=Packet(id,FromServer::InvalidateScores); invalid.message.index=255;
  assert(s.Accept(id,invalid)); assert(Take<ScoresInvalidated>(s).index==255);
  assert(s.AcknowledgedScore(255)==1234); // invalidates leaderboard, not accepted submission
  assert(s.Accept(id,Packet(id,FromServer::LeaderboardScores,R"({"index":255,"scores":[]})")));
  assert(Take<Models::LeaderboardScores>(s).index==255);

  // Clock uses network receipt timestamp even if Unity does not dequeue for 5s.
  auto ping=s.MakePing(id,10); assert(ping && !s.MakePing(id,10.01));
  Reader pingReader(std::span<const std::uint8_t>(*ping).subspan(2));
  assert(pingReader.Byte()==static_cast<std::uint8_t>(ToServer::Ping));
  auto pong=Packet(id,FromServer::Ping,{},10.2); pong.message.clientTime=pingReader.Float();
  pong.message.serverTime=100; pingReader.End();
  assert(s.Accept(id,pong)); Take<ClockUpdated>(s);
  assert(std::abs(*s.ServerTime(15.2)-105.1)<.00001);
  assert(s.Accept(id,pong)); assert(!s.Pop()); // duplicate cannot resample clock
  assert(s.MakePing(id,20)); s.Tick(id,30);
  pong.receivedAt=30.1; pong.message.clientTime=20;
  assert(s.Accept(id,pong)); assert(!s.Pop()); assert(s.MakePing(id,31));

  // An exact verified generation is necessary before ANY scene transition.
  auto ticket=s.BeginPreparation(id,first.mapRevision,"artifact-a"); assert(ticket);
  assert(!s.CanTransition(*ticket) && !s.CompletePreparation(*ticket,false));
  auto forged=*ticket; forged.artifact="other";
  assert(!s.CompletePreparation(forged,true));
  assert(s.CompletePreparation(*ticket,true)); Take<MapPrepared>(s);
  assert(s.CanTransition(*ticket));
  assert(s.CompletePreparation(*ticket,true)); assert(!s.Pop()); // idempotent
  // Same map index but different hash must cancel the old preparation.
  auto next=SetPlay(s,id,"artifact-b");
  assert((next.changes & MapChanged) && next.mapRevision!=first.mapRevision);
  assert(!s.CanTransition(*ticket) && !s.CompletePreparation(*ticket,true));
  assert(!s.BeginPreparation(id,first.mapRevision,"artifact-a"));
  ticket=s.BeginPreparation(id,next.mapRevision,"artifact-b"); assert(ticket);
  auto cancelled=*ticket;
  ticket=s.BeginPreparation(id,next.mapRevision,"artifact-b"); assert(ticket);
  assert(!s.CompletePreparation(cancelled,true));
  assert(s.CompletePreparation(*ticket,true)); Take<MapPrepared>(s);
  assert(s.Accept(id,Packet(id,FromServer::StopLevel))); Take<LevelStopped>(s);
  assert(!s.CanTransition(*ticket) && !s.CompletePreparation(*ticket,true));
  assert(!s.BeginPreparation(id,next.mapRevision,"artifact-b"));
  next=SetPlay(s,id,"artifact-b",0,30);
  assert(next.changes==StartTimeChanged);
  ticket=s.BeginPreparation(id,next.mapRevision,"artifact-b"); assert(ticket);
  assert(s.CompletePreparation(*ticket,true)); Take<MapPrepared>(s);

  assert(s.Accept(id,Packet(id,FromServer::Status,R"({"stage":{"name":"intro","url":"intro","startTime":40}})")));
  auto intro=Take<StatusUpdated>(s);
  assert((intro.changes & StageChanged) && (intro.changes & IntroUrlChanged) && (intro.changes & IntroTimeChanged));
  assert(!s.CanTransition(*ticket) && !s.CompletePreparation(*ticket,true));
  assert(s.Accept(id,Packet(id,FromServer::Status,R"({"stage":{"name":"finish","url":"end","mapCount":4}})")));
  auto finish=Take<StatusUpdated>(s);
  assert((finish.changes & FinishUrlChanged) && (finish.changes & FinishCountChanged));
  auto disconnect=Packet(id,FromServer::Disconnect); disconnect.message.disconnectCode=2;
  assert(s.Accept(id,disconnect));
  auto closed=Take<SessionClosed>(s);
  assert(closed.reason==SessionCloseReason::ServerDisconnect && closed.disconnectCode==2);
  assert(s.Phase()==SessionPhase::Closed && !s.AcknowledgedScore(255) && !s.ServerTime(100));
  assert(!s.Accept(id,counts) && !s.CompletePreparation(*ticket,true));

  // Session epoch protects even against reused numeric transport connection ids.
  auto stale=id; id=Login(s,stale.connection);
  assert(id.epoch!=stale.epoch && !s.Accept(stale,Packet(id,FromServer::StopLevel)));
  s.Close(stale,SessionCloseReason::Cancelled); assert(s.Phase()==SessionPhase::Authenticated);
  auto other=Packet(id,FromServer::StopLevel); ++other.connection;
  assert(!s.Accept(id,other));
  auto newMap=SetPlay(s,id); ticket=s.BeginPreparation(id,newMap.mapRevision,"a"); assert(ticket);
  s.CompletePreparation(*ticket,true); // leave queued intentionally
  auto newId=s.Begin(99); Take<AuthenticationRequested>(s);
  assert(!s.Pop() && !s.CanTransition(*ticket));
  assert(!s.AuthenticationSent(id,1) && !s.MakePing(id,1));

  // Auth must be explicitly submitted; an unsolicited ACK cannot grant access.
  assert(!s.Accept(newId,Packet(newId,FromServer::Authenticated)));
  assert(Take<SessionClosed>(s).reason==SessionCloseReason::InvalidMessage);
  id=s.Begin(2); Take<AuthenticationRequested>(s);
  assert(s.Accept(id,Packet(id,FromServer::RefusedPacket,"bad credentials")));
  Take<PacketRefused>(s); assert(s.Phase()==SessionPhase::AwaitingAuthentication);
  assert(!s.Accept(id,Packet(id,FromServer::Status,"{}")));
  assert(Take<SessionClosed>(s).reason==SessionCloseReason::InvalidMessage);
  id=s.Begin(3); Take<AuthenticationRequested>(s);
  assert(s.AuthenticationSent(id,10) && s.AuthenticationSent(id,17));
  s.Tick(id,18); assert(Take<SessionClosed>(s).reason==SessionCloseReason::AuthenticationTimeout);
  id=s.Begin(4); Take<AuthenticationRequested>(s); s.AuthenticationSent(id,10);
  assert(!s.Accept(id,Packet(id,FromServer::Authenticated,{},18)));
  assert(Take<SessionClosed>(s).reason==SessionCloseReason::AuthenticationTimeout);

  // Refuse malformed JSON with a stable error enum, never user text in an error.
  id=Login(s); assert(!s.Accept(id,Packet(id,FromServer::Status,R"({"stage":{"name":"bad"}})")));
  assert(Take<SessionClosed>(s).reason==SessionCloseReason::InvalidMessage);
  id=Login(s); other=Packet(id,FromServer::PlayerCount); other.receivedAt=INFINITY;
  assert(!s.Accept(id,other)); Take<SessionClosed>(s);
  id=Login(s); assert(!s.Accept(id,Packet(id,FromServer::UserJoin,std::string(MaxBody+1,'x'))));
  Take<SessionClosed>(s);
  id=Login(s); assert(!s.Accept(id,Packet(id,FromServer::UserJoin,std::string(1,'\xff'))));
  Take<SessionClosed>(s);
  id=Login(s); other=Packet(id,static_cast<FromServer>(255));
  assert(!s.Accept(id,other)); Take<SessionClosed>(s);

  // Full notice queues cannot partially commit a new stage or hide termination.
  id=Login(s); newMap=SetPlay(s,id); ticket=s.BeginPreparation(id,newMap.mapRevision,"a"); assert(ticket);
  for(int i=0;i<64;++i) assert(s.Accept(id,Packet(id,FromServer::UserJoin,"queued")));
  assert(!s.CompletePreparation(*ticket,true));
  assert(s.Closed()->reason==SessionCloseReason::QueueOverflow && !s.CanTransition(*ticket));
  Take<SessionClosed>(s); assert(!s.Pop());
  id=Login(s);
  for(int i=0;i<64;++i) assert(s.Accept(id,Packet(id,FromServer::UserJoin,"queued")));
  assert(!s.Accept(id,Packet(id,FromServer::Status,Play())));
  assert(s.Closed()->reason==SessionCloseReason::QueueOverflow);
  assert(std::holds_alternative<Models::InvalidStage>(s.Status().stage));
  Take<SessionClosed>(s); assert(!s.Pop());
  // Retained notice snapshots are owned and cannot mutate into later status.
  id=Login(s); first=SetPlay(s,id,"first"); SetPlay(s,id,"second");
  assert(std::get<Models::PlayStage>(first.value.stage).map.downloads[0].hash=="first");
  std::cout<<"Synapse session reducer: all opcodes, auth/clock epochs, preparation, overflow passed\n";
}
