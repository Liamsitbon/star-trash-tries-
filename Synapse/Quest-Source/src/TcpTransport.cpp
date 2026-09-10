#include "Synapse/TcpTransport.hpp"
#include <arpa/inet.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <mutex>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace Synapse::Quest {
double TransportClockSeconds() {
  static auto const origin=std::chrono::steady_clock::now();
  return std::chrono::duration<double>(std::chrono::steady_clock::now()-origin).count();
}
namespace {
double Now() { return TransportClockSeconds(); }
constexpr int PollMs=50;
constexpr std::size_t QueueCapacity=64;
struct Socket {
  int fd=-1;
  explicit Socket(int f):fd(f) {}
  ~Socket() { if(fd>=0) close(fd); }
  Socket(Socket const&)=delete;
};
bool Address(Endpoint const& e,sockaddr_storage& storage,socklen_t& size) {
  if(!e.port || e.ip.empty() || e.ip.size()>INET6_ADDRSTRLEN || e.ip.find('\0')!=std::string::npos) return false;
  sockaddr_in v4{}; v4.sin_family=AF_INET; v4.sin_port=htons(e.port);
  if(inet_pton(AF_INET,e.ip.c_str(),&v4.sin_addr)==1) {
    std::memcpy(&storage,&v4,sizeof(v4)); size=sizeof(v4); return true;
  }
  sockaddr_in6 v6{}; v6.sin6_family=AF_INET6; v6.sin6_port=htons(e.port); v6.sin6_scope_id=e.ipv6Scope;
  if(inet_pton(AF_INET6,e.ip.c_str(),&v6.sin6_addr)!=1) return false;
  std::memcpy(&storage,&v6,sizeof(v6)); size=sizeof(v6); return true;
}
bool PendingError() { return errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR; }
}
struct TcpTransport::Impl {
  mutable std::mutex mutex;
  TransportStatus status;
  std::deque<TransportEvent> events;
  std::deque<std::vector<std::uint8_t>> outgoing;
  std::atomic<bool> stop{false}, done{true};
  std::thread worker;
  std::uint64_t nextConnection=0;
  void Set(ConnectionState state,TransportError error,std::uint8_t attempt) {
    std::lock_guard lock(mutex);
    status.state=state; status.error=error; status.attempt=attempt;
    if(state!=ConnectionState::Connected) outgoing.clear();
  }
  bool Connected(std::uint8_t attempt) {
    std::lock_guard lock(mutex);
    if(events.size()==QueueCapacity) return false;
    status={ConnectionState::Connected,TransportError::None,++nextConnection,attempt};
    events.push_back({TransportEvent::Kind::Connected,status.connection,{},Now()}); return true;
  }
  bool Received(ServerMessage message) {
    std::lock_guard lock(mutex);
    if(events.size()==QueueCapacity) return false;
    events.push_back({TransportEvent::Kind::Message,status.connection,std::move(message),Now()}); return true;
  }
  std::vector<std::uint8_t> NextFrame() {
    std::lock_guard lock(mutex);
    if(outgoing.empty()) return {};
    auto frame=std::move(outgoing.front()); outgoing.pop_front(); return frame;
  }
  TransportError Connect(int fd,sockaddr_storage const& address,socklen_t size,double timeout) {
    if(connect(fd,reinterpret_cast<sockaddr const*>(&address),size)==0) return TransportError::None;
    if(errno!=EINPROGRESS && errno!=EINTR) return TransportError::ConnectFailed;
    double deadline=Now()+timeout;
    while(!stop) {
      pollfd p{fd,POLLOUT,0}; int result=poll(&p,1,PollMs);
      if(result<0 && errno!=EINTR) return TransportError::SocketError;
      if(result>0) {
        int error=0; socklen_t length=sizeof(error);
        if(getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&length)<0 || error!=0) return TransportError::ConnectFailed;
        return TransportError::None;
      }
      if(Now()>=deadline) return TransportError::ConnectTimeout;
    }
    return TransportError::Cancelled;
  }
  TransportError ReadWrite(int fd) {
    FrameDecoder decoder; std::array<std::uint8_t,4096> incoming{};
    std::vector<std::uint8_t> sending; std::size_t sent=0; double sendStarted=0;
    while(!stop) {
      if(sending.empty()) { sending=NextFrame(); sent=0; sendStarted=Now(); }
      pollfd p{fd,static_cast<short>(POLLIN | (sending.empty() ? 0 : POLLOUT)),0};
      int result=poll(&p,1,PollMs);
      if(stop) break;
      if(result<0) { if(errno==EINTR) continue; return TransportError::SocketError; }
      if(decoder.Expired(Now())) return TransportError::PacketTimeout;
      if(!sending.empty() && Now()-sendStarted>=2) return TransportError::SendTimeout;
      if(p.revents & POLLNVAL) return TransportError::SocketError;
      // Read bytes delivered alongside HUP before reporting EOF. A final server
      // Disconnect/StopLevel must not disappear just because the socket closed.
      if(p.revents & (POLLIN|POLLHUP|POLLERR)) {
        auto n=recv(fd,incoming.data(),incoming.size(),0);
        if(n==0) return TransportError::PeerClosed;
        if(n<0 && !PendingError()) return TransportError::SocketError;
        if(n>0) {
          TransportError packetError=TransportError::None;
          try {
            decoder.Feed(std::span(incoming).first(static_cast<std::size_t>(n)),Now(),[&](auto body) {
              if(packetError!=TransportError::None) return;
              auto message=DecodeServer(body); bool disconnect=message.opcode==FromServer::Disconnect;
              if(!Received(std::move(message))) packetError=TransportError::QueueOverflow;
              else if(disconnect) packetError=TransportError::ServerDisconnect;
            });
          } catch(ProtocolError const&) { return TransportError::MalformedPacket; }
          if(packetError!=TransportError::None) return packetError;
        }
      }
      if((p.revents & POLLOUT) && !sending.empty()) {
#ifdef MSG_NOSIGNAL
        constexpr int flags=MSG_NOSIGNAL;
#else
        constexpr int flags=0; // macOS uses SO_NOSIGPIPE, configured below.
#endif
        auto n=send(fd,sending.data()+sent,sending.size()-sent,flags);
        if(n<0 && !PendingError()) return TransportError::SocketError;
        if(n==0) return TransportError::PeerClosed;
        if(n>0) { sent+=static_cast<std::size_t>(n); if(sent==sending.size()) sending.clear(); }
      }
    }
    return TransportError::Cancelled;
  }
  void Run(sockaddr_storage address,socklen_t size,TransportOptions options) noexcept {
    TransportError error=TransportError::ConnectFailed; std::uint8_t attempt=0;
    try {
      for(attempt=1; attempt<=options.attempts && !stop; ++attempt) {
        Set(ConnectionState::Connecting,TransportError::None,attempt);
        { // Only the worker owns/closes this descriptor: no cross-thread fd reuse.
          Socket socket{::socket(address.ss_family,SOCK_STREAM,IPPROTO_TCP)};
          if(socket.fd<0) error=TransportError::SocketError;
          else {
            int oldFlags=fcntl(socket.fd,F_GETFL,0);
            if(oldFlags<0 || fcntl(socket.fd,F_SETFL,oldFlags|O_NONBLOCK)<0) error=TransportError::SocketError;
            else {
              int one=1; setsockopt(socket.fd,IPPROTO_TCP,TCP_NODELAY,&one,sizeof(one));
#ifdef SO_NOSIGPIPE
              if(setsockopt(socket.fd,SOL_SOCKET,SO_NOSIGPIPE,&one,sizeof(one))<0) { error=TransportError::SocketError; break; }
#endif
              error=Connect(socket.fd,address,size,options.connectTimeoutSeconds);
              if(error==TransportError::None) error=Connected(attempt) ? ReadWrite(socket.fd) : TransportError::QueueOverflow;
            }
          }
        }
        if(stop || error==TransportError::QueueOverflow || error==TransportError::MalformedPacket ||
           error==TransportError::ServerDisconnect || attempt==options.attempts) break;
        Set(ConnectionState::RetryDelay,error,attempt);
        auto deadline=Now()+options.retryDelaySeconds;
        while(!stop && Now()<deadline) std::this_thread::sleep_for(std::chrono::milliseconds(PollMs));
      }
    } catch(...) { error=TransportError::SocketError; } // Never leak payloads or terminate from a worker exception.
    Set(stop ? ConnectionState::Stopped : ConnectionState::Failed,stop ? TransportError::Cancelled : error,attempt);
    done=true;
  }
};
TcpTransport::TcpTransport():impl_(std::make_unique<Impl>()) {}
TcpTransport::~TcpTransport() { Stop(); if(impl_->worker.joinable()) impl_->worker.join(); }
bool TcpTransport::Start(Endpoint endpoint,TransportOptions options) {
  sockaddr_storage address{}; socklen_t size=0;
  if(!Address(endpoint,address,size) || options.attempts==0 || options.attempts>10 ||
     !std::isfinite(options.connectTimeoutSeconds) || options.connectTimeoutSeconds<=0 || options.connectTimeoutSeconds>30 ||
     !std::isfinite(options.retryDelaySeconds) || options.retryDelaySeconds<0 || options.retryDelaySeconds>30) return false;
  if(!impl_->done.load()) return false;
  if(impl_->worker.joinable()) impl_->worker.join(); // Already finished; no live join here.
  {
    std::lock_guard lock(impl_->mutex);
    impl_->events.clear(); impl_->outgoing.clear();
    impl_->status={ConnectionState::Connecting,TransportError::None,0,0};
  }
  impl_->stop=false; impl_->done=false;
  try { impl_->worker=std::thread([p=impl_.get(),address,size,options]{p->Run(address,size,options);}); }
  catch(...) { impl_->done=true; impl_->Set(ConnectionState::Failed,TransportError::SocketError,0); return false; }
  return true;
}
void TcpTransport::Stop() { impl_->stop=true; }
TransportStatus TcpTransport::Status() const { std::lock_guard lock(impl_->mutex); return impl_->status; }
std::optional<TransportEvent> TcpTransport::Pop() {
  std::lock_guard lock(impl_->mutex);
  if(impl_->stop || impl_->events.empty()) return {};
  auto event=std::move(impl_->events.front()); impl_->events.pop_front(); return event;
}
bool TcpTransport::Send(std::uint64_t connection,std::vector<std::uint8_t> frame) {
  if(frame.size()<3 || frame.size()>MaxBody+2 ||
     (static_cast<std::size_t>(frame[0])+(static_cast<std::size_t>(frame[1])<<8))!=frame.size()-2 ||
     frame[2]>static_cast<std::uint8_t>(ToServer::LeaderboardRequest)) return false;
  std::lock_guard lock(impl_->mutex);
  if(impl_->stop || impl_->status.state!=ConnectionState::Connected || impl_->status.connection!=connection ||
     impl_->outgoing.size()==QueueCapacity) return false;
  impl_->outgoing.push_back(std::move(frame)); return true;
}
} // namespace Synapse::Quest
