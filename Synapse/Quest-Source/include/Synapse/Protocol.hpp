#pragma once
// MIT. Protocol behavior derived from Aeroluna's Synapse.Networking (2024).
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace Synapse::Quest {
inline constexpr std::size_t MaxBody=16384;
enum class ToServer : std::uint8_t {
  Authentication, Disconnect, Ping, SetChatter, SetDivision, ChatMessage,
  Command, ScoreSubmission, LeaderboardRequest
};
enum class FromServer : std::uint8_t {
  Authenticated, Disconnect, RefusedPacket, Ping, Status, ChatMessage, UserBanned,
  AcknowledgeScore, InvalidateScores, LeaderboardScores, StopLevel, PlayerCount,
  UserJoin, UserLeave
};
class ProtocolError : public std::runtime_error { using std::runtime_error::runtime_error; };
bool ValidUtf8(std::string_view);

class Writer {
  std::vector<std::uint8_t> data_;
public:
  explicit Writer(ToServer opcode) : data_{0,0,static_cast<std::uint8_t>(opcode)} { data_.reserve(256); }
  Writer& Byte(std::uint8_t);
  Writer& U16(std::uint16_t);
  Writer& Int(std::int32_t);
  Writer& Float(float);
  Writer& Bool(bool v) { return Byte(v ? 1 : 0); }
  Writer& String(std::string_view);
  std::vector<std::uint8_t> Finish();
};
class Reader {
  std::span<const std::uint8_t> data_;
  std::size_t cursor_=0;
public:
  explicit Reader(std::span<const std::uint8_t> data) : data_(data) {
    if (data.empty() || data.size()>MaxBody) throw ProtocolError("invalid body size");
  }
  std::uint8_t Byte();
  std::uint16_t U16();
  std::int32_t Int();
  float Float();
  bool Bool();
  std::string String();
  void End(bool allowLegacyZeroPadding=false);
};

// Feed owns its buffer; callback span is borrowed ONLY until the callback ends.
// Network integration must copy complete messages into a bounded owned queue.
// Invalid framing poisons the stream until Reset (the connection must close).
class FrameDecoder {
  std::array<std::uint8_t,MaxBody+2> bytes_{};
  std::size_t used_=0, expected_=0;
  double started_=0;
  bool poisoned_=false;
public:
  void Reset() { used_=expected_=0; started_=0; poisoned_=false; }
  bool Expired(double now) const {
    return used_ && std::isfinite(now) && now-started_>=2.0;
  }
  void Feed(std::span<const std::uint8_t>, double now,
            std::function<void(std::span<const std::uint8_t>)> const& callback);
};
struct ServerMessage {
  FromServer opcode{};
  std::string text; // Raw JSON for Status/Chat/Leaderboard; typed models follow separately.
  float clientTime=0, serverTime=0;
  std::int32_t score=0;
  std::uint16_t chatters=0, players=0;
  std::uint8_t index=0, disconnectCode=0;
};
ServerMessage DecodeServer(std::span<const std::uint8_t>);

struct Authentication {
  std::string userId, userName;
  std::uint8_t platform=0;
  std::string sessionToken, gameVersion, listingGuid;
};
std::vector<std::uint8_t> EncodeAuthentication(Authentication const&);
std::vector<std::uint8_t> EncodeLeaderboardRequest(std::int32_t index, std::int32_t division, bool eliminated);
} // namespace Synapse::Quest
