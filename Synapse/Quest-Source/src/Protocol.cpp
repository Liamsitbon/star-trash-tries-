#include "Synapse/Protocol.hpp"
#include <algorithm>
#include <utility>
namespace Synapse::Quest {
bool ValidUtf8(std::string_view s) {
  for (std::size_t i=0;i<s.size();) {
    auto first=static_cast<std::uint8_t>(s[i++]);
    if (first<0x80) continue;
    int n; std::uint32_t code, minimum;
    if (first>=0xc2 && first<=0xdf) { n=1; code=first&31; minimum=0x80; }
    else if (first>=0xe0 && first<=0xef) { n=2; code=first&15; minimum=0x800; }
    else if (first>=0xf0 && first<=0xf4) { n=3; code=first&7; minimum=0x10000; }
    else return false;
    if (i+n>s.size()) return false;
    while(n--) { auto c=static_cast<std::uint8_t>(s[i++]); if ((c&0xc0)!=0x80) return false; code=(code<<6)|(c&63); }
    if (code<minimum || code>0x10ffff || (code>=0xd800 && code<=0xdfff)) return false;
  }
  return true;
}
Writer& Writer::Byte(std::uint8_t v) {
  if (data_.size()>=MaxBody+2) throw ProtocolError("packet exceeds 16KiB");
  data_.push_back(v); return *this;
}
Writer& Writer::U16(std::uint16_t v) { Byte(v&255); return Byte(v>>8); }
Writer& Writer::Int(std::int32_t v) {
  auto u=std::bit_cast<std::uint32_t>(v);
  for (int shift=0;shift<32;shift+=8) Byte((u>>shift)&255);
  return *this;
}
Writer& Writer::Float(float v) {
  if (!std::isfinite(v)) throw ProtocolError("nonfinite float");
  return Int(std::bit_cast<std::int32_t>(v));
}
Writer& Writer::String(std::string_view s) {
  if (!ValidUtf8(s) || s.size()>MaxBody) throw ProtocolError("invalid UTF8/string length");
  auto length=static_cast<std::uint32_t>(s.size());
  do { auto b=static_cast<std::uint8_t>(length&127); length>>=7; Byte(b|(length ? 128:0)); } while(length);
  for (unsigned char c : s) Byte(c);
  return *this;
}
std::vector<std::uint8_t> Writer::Finish() {
  if (data_.size()<3) throw ProtocolError("writer already consumed");
  auto length=data_.size()-2;
  data_[0]=length&255; data_[1]=length>>8;
  return std::move(data_);
}
std::uint8_t Reader::Byte() {
  if (cursor_==data_.size()) throw ProtocolError("truncated packet");
  return data_[cursor_++];
}
std::uint16_t Reader::U16() { std::uint16_t v=Byte(); return v|(static_cast<std::uint16_t>(Byte())<<8); }
std::int32_t Reader::Int() {
  std::uint32_t v=0;
  for(int shift=0;shift<32;shift+=8) v|=static_cast<std::uint32_t>(Byte())<<shift;
  return std::bit_cast<std::int32_t>(v);
}
float Reader::Float() {
  auto v=std::bit_cast<float>(Int());
  if(!std::isfinite(v)) throw ProtocolError("nonfinite incoming float");
  return v;
}
bool Reader::Bool() { auto v=Byte(); if(v>1) throw ProtocolError("invalid bool"); return v!=0; }
std::string Reader::String() {
  std::uint32_t length=0; int shift=0;
  for(;;) {
    auto b=Byte();
    if(shift==28 && (b&0xf8)) throw ProtocolError("7-bit string length overflow");
    length|=static_cast<std::uint32_t>(b&127)<<shift;
    if(!(b&128)) break;
    shift+=7; if(shift>28) throw ProtocolError("7-bit string length overflow");
  }
  if(length>data_.size()-cursor_) throw ProtocolError("truncated string");
  std::string s(reinterpret_cast<const char*>(data_.data()+cursor_),length);
  cursor_+=length;
  if(!ValidUtf8(s)) throw ProtocolError("invalid incoming UTF8");
  return s;
}
void Reader::End(bool padded) {
  while(cursor_<data_.size()) if(!padded || Byte()!=0) throw ProtocolError("unexpected trailing payload");
}
void FrameDecoder::Feed(std::span<const std::uint8_t> input, double now,
    std::function<void(std::span<const std::uint8_t>)> const& callback) {
  if(poisoned_) throw ProtocolError("stream is poisoned");
  if(!std::isfinite(now) || Expired(now)) { poisoned_=true; throw ProtocolError("packet timeout/clock error"); }
  try {
    for(auto b : input) {
      if(!used_) started_=now;
      bytes_[used_++]=b;
      if(used_==2) {
        expected_=bytes_[0]|(static_cast<std::size_t>(bytes_[1])<<8);
        if(!expected_ || expected_>MaxBody) throw ProtocolError("invalid frame length");
        expected_+=2;
      }
      if(expected_ && used_==expected_) {
        callback(std::span<const std::uint8_t>(bytes_.data()+2,expected_-2));
        used_=expected_=0;
      }
    }
  } catch(...) { poisoned_=true; throw; }
}
ServerMessage DecodeServer(std::span<const std::uint8_t> data) {
  Reader r(data); ServerMessage message;
  auto code=r.Byte();
  if(code>static_cast<unsigned>(FromServer::UserLeave)) throw ProtocolError("unknown server opcode");
  message.opcode=static_cast<FromServer>(code);
  switch(message.opcode) {
    case FromServer::Authenticated: case FromServer::StopLevel: break;
    case FromServer::Disconnect: message.disconnectCode=r.Byte(); break;
    case FromServer::Ping: message.clientTime=r.Float(); message.serverTime=r.Float(); break;
    case FromServer::AcknowledgeScore: message.index=r.Byte(); message.score=r.Int(); break;
    case FromServer::InvalidateScores: message.index=r.Byte(); break;
    case FromServer::PlayerCount: message.chatters=r.U16(); message.players=r.U16(); break;
    case FromServer::RefusedPacket: case FromServer::Status: case FromServer::ChatMessage:
    case FromServer::UserBanned: case FromServer::LeaderboardScores:
    case FromServer::UserJoin: case FromServer::UserLeave: message.text=r.String(); break;
  }
  r.End(true); // Original non-NET PacketBuilder can pad its stream capacity with zeroes.
  return message;
}
std::vector<std::uint8_t> EncodeAuthentication(Authentication const& a) {
  return Writer(ToServer::Authentication).String(a.userId).String(a.userName).Byte(a.platform)
      .String(a.sessionToken).String(a.gameVersion).String(a.listingGuid).Finish();
}
std::vector<std::uint8_t> EncodeLeaderboardRequest(std::int32_t index,std::int32_t division,bool eliminated) {
  return Writer(ToServer::LeaderboardRequest).Int(index).Int(division).Bool(eliminated).Finish();
}
} // namespace Synapse::Quest
