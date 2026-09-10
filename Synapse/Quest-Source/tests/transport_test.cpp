#include "Synapse/TcpTransport.hpp"
#include <arpa/inet.h>
#include <cassert>
#include <chrono>
#include <future>
#include <iostream>
#include <poll.h>
#include <thread>
#include <unistd.h>
using namespace Synapse::Quest;
using namespace std::chrono_literals;
struct Peer { int fd; explicit Peer(int f):fd(f) {assert(fd>=0);} ~Peer(){close(fd);} };
struct Listener {
  Peer socket{::socket(AF_INET,SOCK_STREAM,0)}; std::uint16_t port=0;
  Listener() {
    sockaddr_in a{}; a.sin_family=AF_INET; a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    assert(bind(socket.fd,reinterpret_cast<sockaddr*>(&a),sizeof(a))==0); assert(listen(socket.fd,4)==0);
    socklen_t length=sizeof(a); assert(getsockname(socket.fd,reinterpret_cast<sockaddr*>(&a),&length)==0); port=ntohs(a.sin_port);
  }
  int Accept() { pollfd p{socket.fd,POLLIN,0}; assert(poll(&p,1,4000)>0); return accept(socket.fd,nullptr,nullptr); }
};
template<class F> void Until(F&& condition,double seconds=4) {
  auto end=std::chrono::steady_clock::now()+std::chrono::duration<double>(seconds);
  while(!condition()) { assert(std::chrono::steady_clock::now()<end); std::this_thread::sleep_for(2ms); }
}
void Write(int fd,std::span<const std::uint8_t> bytes) {
  while(!bytes.empty()) { pollfd p{fd,POLLOUT,0}; assert(poll(&p,1,2000)>0);
    auto n=send(fd,bytes.data(),bytes.size(),0); assert(n>0); bytes=bytes.subspan(static_cast<std::size_t>(n)); }
}
std::vector<std::uint8_t> Read(int fd,std::size_t count) {
  std::vector<std::uint8_t> data(count); std::size_t read=0;
  while(read<count) { pollfd p{fd,POLLIN,0}; assert(poll(&p,1,2000)>0);
    auto n=recv(fd,data.data()+read,count-read,0); assert(n>0); read+=static_cast<std::size_t>(n); }
  return data;
}
std::uint64_t Connection(TcpTransport& client) {
  std::uint64_t id=0;
  Until([&]{ if(auto e=client.Pop(); e && e->kind==TransportEvent::Kind::Connected) id=e->connection; return id!=0; }); return id;
}
int main() {
  TcpTransport invalid;
  assert(!invalid.Start({"hostname.invalid",1})); assert(!invalid.Start({"127.0.0.1",0}));
  assert(!invalid.Start({"127.0.0.1",1},{0,1,0}));
  assert(!invalid.Start({std::string("127.0.0.1\0suffix",16),1}));
  {
    Listener server; TcpTransport client; auto frame=EncodeLeaderboardRequest(3,2,false);
    std::promise<void> mayClose; auto closing=mayClose.get_future();
    auto peer=std::async(std::launch::async,[&] {
      Peer p(server.Accept()); assert(Read(p.fd,frame.size())==frame);
      std::array<std::uint8_t,3> auth{1,0,0};
      for(auto byte:auth) { Write(p.fd,std::span(&byte,1)); std::this_thread::sleep_for(2ms); }
      std::array<std::uint8_t,7> controls{1,0,10,2,0,1,4}; // StopLevel; Disconnect(ServerClosing).
      Write(p.fd,controls); closing.wait();
    });
    assert(client.Start({"127.0.0.1",server.port},{1,1,0})); auto id=Connection(client);
    assert(!client.Start({"127.0.0.1",server.port})); assert(!client.Send(id+1,frame)); assert(client.Send(id,frame));
    std::vector<FromServer> messages;
    Until([&]{ while(auto e=client.Pop()) { messages.push_back(e->message.opcode); if(e->message.opcode==FromServer::Disconnect) assert(e->message.disconnectCode==4); } return messages.size()==3; });
    assert((messages==std::vector{FromServer::Authenticated,FromServer::StopLevel,FromServer::Disconnect}));
    Until([&]{return client.Status().state==ConnectionState::Failed;}); assert(client.Status().error==TransportError::ServerDisconnect);
    mayClose.set_value(); peer.get();
  }
  {
    Listener server; TcpTransport client; std::promise<void> closeFirst, closeSecond;
    auto first=closeFirst.get_future(); auto second=closeSecond.get_future();
    auto peer=std::async(std::launch::async,[&] {{Peer p(server.Accept()); first.wait();} {Peer p(server.Accept()); second.wait();}});
    assert(client.Start({"127.0.0.1",server.port},{2,1,.01})); auto oldId=Connection(client);
    closeFirst.set_value(); auto newId=Connection(client); assert(newId!=oldId);
    assert(!client.Send(oldId,Writer(ToServer::Ping).Float(1).Finish()));
    auto start=std::chrono::steady_clock::now(); client.Stop(); Until([&]{return client.Status().state==ConnectionState::Stopped;},1);
    assert(std::chrono::steady_clock::now()-start<500ms); assert(!client.Send(newId,Writer(ToServer::Ping).Float(1).Finish()));
    assert(!client.Pop());
    closeSecond.set_value(); peer.get();
  }
  {
    Listener server; TcpTransport client; std::promise<void> release; auto wait=release.get_future();
    auto peer=std::async(std::launch::async,[&]{Peer p(server.Accept()); std::array<std::uint8_t,1> b{1}; Write(p.fd,b); wait.wait();});
    assert(client.Start({"127.0.0.1",server.port},{1,1,0})); Connection(client);
    Until([&]{return client.Status().state==ConnectionState::Failed;}); assert(client.Status().error==TransportError::PacketTimeout);
    release.set_value(); peer.get();
  }
  {
    Listener server; TcpTransport client; std::promise<void> release; auto wait=release.get_future();
    auto peer=std::async(std::launch::async,[&] {
      Peer p(server.Accept()); std::vector<std::uint8_t> frames;
      for(int i=0;i<100;++i) frames.insert(frames.end(),{1,0,0}); Write(p.fd,frames); wait.wait();
    });
    assert(client.Start({"127.0.0.1",server.port},{1,1,0})); Until([&]{return client.Status().state==ConnectionState::Failed;});
    assert(client.Status().error==TransportError::QueueOverflow); int count=0; while(client.Pop()) ++count; assert(count==64);
    release.set_value(); peer.get();
  }
  {
    Listener server; TcpTransport client;
    auto peer=std::async(std::launch::async,[&]{Peer p(server.Accept()); std::array<std::uint8_t,2> bad{0,0}; Write(p.fd,bad);});
    assert(client.Start({"127.0.0.1",server.port},{3,1,0})); Until([&]{return client.Status().state==ConnectionState::Failed;});
    assert(client.Status().error==TransportError::MalformedPacket && client.Status().attempt==1); peer.get();
  }
  std::cout<<"Synapse loopback transport passed; no external server contacted\n";
}
