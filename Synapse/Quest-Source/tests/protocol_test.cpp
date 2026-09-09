#include "Synapse/Protocol.hpp"
#include "Synapse/SessionState.hpp"
#include "Synapse/MessageQueue.hpp"
#include <cassert>
#include <iostream>
#include <iomanip>
#include <limits>
#include <random>
using namespace Synapse::Quest;
template<class F> void rejects(F f) { bool rejected=false; try { f(); } catch(ProtocolError const&) { rejected=true; } assert(rejected); }
void golden(std::string const& name,std::vector<std::uint8_t> const& bytes) {
  std::cout<<name<<' '; for(auto b:bytes) std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<unsigned>(b);
  std::cout<<std::dec<<'\n';
}
int main(int argc,char**) {
  auto auth=EncodeAuthentication({"123456", "שלום 🦋",1,"synthetic-test-token","1.40.8_7379","test-listing"});
  if(argc>1) {
    golden("auth",auth);
    golden("disconnect",Writer(ToServer::Disconnect).Byte(2).Finish());
    golden("ping",Writer(ToServer::Ping).Float(1.25f).Finish());
    golden("chatter",Writer(ToServer::SetChatter).Bool(true).Finish());
    golden("division",Writer(ToServer::SetDivision).Int(-1).Finish());
    golden("chat",Writer(ToServer::ChatMessage).String("שלום").Finish());
    golden("command",Writer(ToServer::Command).String("/test synthetic").Finish());
    golden("score",Writer(ToServer::ScoreSubmission).String("{\"division\":0,\"index\":1,\"score\":123,\"percentage\":98.5}").Finish());
    golden("leaderboard",EncodeLeaderboardRequest(-1,2,true));
    golden("long-string",Writer(ToServer::ChatMessage).String(std::string(300,'x')).Finish());
    return 0;
  }
  Reader reader{std::span(auth).subspan(2)};
  assert(reader.Byte()==0 && reader.String()=="123456" && reader.String()=="שלום 🦋");
  assert(reader.Byte()==1 && reader.String()=="synthetic-test-token");
  assert(reader.String()=="1.40.8_7379" && reader.String()=="test-listing"); reader.End();
  assert(ValidUtf8("שלום 🦋") && !ValidUtf8("\xc0\xaf") && !ValidUtf8("\xed\xa0\x80"));
  for(std::size_t chunk=1;chunk<=auth.size();++chunk) {
    FrameDecoder decoder; int received=0;
    std::vector<std::uint8_t> stream=auth; stream.insert(stream.end(),auth.begin(),auth.end());
    for(std::size_t i=0;i<stream.size();i+=chunk)
      decoder.Feed(std::span(stream).subspan(i,std::min(chunk,stream.size()-i)),1,
          [&](auto body) { ++received; assert(std::equal(body.begin(),body.end(),auth.begin()+2)); });
    assert(received==2);
  }
  FrameDecoder decoder;
  decoder.Feed(std::span(auth).first(1),1,[](auto){});
  assert(!decoder.Expired(2.9) && decoder.Expired(3));
  rejects([&]{decoder.Feed(std::span(auth).subspan(1),3,[](auto){});});
  decoder.Reset(); std::array<std::uint8_t,2> tooLarge{1,64};
  rejects([&]{decoder.Feed(tooLarge,0,[](auto){});});
  rejects([&]{decoder.Feed(auth,1,[](auto){});}); // Poisoned even after valid input.
  rejects([] { Writer(ToServer::Ping).Float(std::numeric_limits<float>::quiet_NaN()); });
  rejects([] { Writer(ToServer::ChatMessage).String(std::string(MaxBody,'x')); });
  std::array<std::uint8_t,5> overflow{0xff,0xff,0xff,0xff,0x7f};
  rejects([&]{ Reader(overflow).String(); });
  for(unsigned code=0;code<=13;++code) {
    Writer w(static_cast<ToServer>(code));
    switch(static_cast<FromServer>(code)) {
      case FromServer::Authenticated: case FromServer::StopLevel: break;
      case FromServer::Disconnect: case FromServer::InvalidateScores: w.Byte(2); break;
      case FromServer::Ping: w.Float(1).Float(2); break;
      case FromServer::AcknowledgeScore: w.Byte(1).Int(123); break;
      case FromServer::PlayerCount: w.U16(2).U16(8); break;
      default: w.String("{}"); break;
    }
    auto packet=w.Finish(); auto m=DecodeServer(std::span(packet).subspan(2));
    assert(static_cast<unsigned>(m.opcode)==code);
    packet.push_back(0); DecodeServer(std::span(packet).subspan(2));
    packet.back()=1; rejects([&]{DecodeServer(std::span(packet).subspan(2));});
  }
  // Bounded malformed stream corpus; run with ASan/UBSan as well as ordinary CTest.
  std::mt19937 random(42);
  for(int n=0;n<10000;++n) {
    std::vector<std::uint8_t> input(random()%512+1);
    for(auto& b:input) b=static_cast<std::uint8_t>(random());
    try { FrameDecoder f; f.Feed(input,0,[](auto body){DecodeServer(body);}); } catch(ProtocolError const&) {}
  }
  ServerClock clock;
  assert(!clock.Time(1)); assert(clock.BeginPing(1));
  assert(!clock.Pong(2,100,1.2)); assert(clock.Pong(1,100,1.2));
  assert(std::abs(*clock.Time(2)-100.9)<.00001);
  assert(clock.BeginPing(3)); assert(clock.TimedOut(13)); assert(!clock.Pong(3,110,13));
  clock.Reset(); assert(!clock.Time(1));
  PreparedMapGate gate;
  auto old=gate.Begin("old-hash"); auto current=gate.Begin("next-hash");
  assert(!gate.Complete(old,"old-hash",true) && !gate.CanTransition());
  assert(!gate.Complete(current,"next-hash",false));
  assert(gate.Complete(current,"next-hash",true) && gate.CanTransition());
  gate.Cancel(); assert(!gate.Complete(current,"next-hash",true) && !gate.CanTransition());
  MessageQueue queue;
  auto first=queue.NewSession();
  for(int i=0;i<64;++i) { ServerMessage message; message.index=i; assert(queue.Push(first,std::move(message))); }
  assert(!queue.Push(first,{}));
  for(int i=0;i<64;++i) assert(queue.Pop()->index==i);
  assert(!queue.Pop());
  auto second=queue.NewSession(); assert(!queue.Push(first,{})); assert(queue.Push(second,{}));
  queue.NewSession(); assert(!queue.Pop());
  std::cout<<"Synapse wire, fragmented TCP, 14 receive opcodes, timeout and generation tests passed\n";
}
