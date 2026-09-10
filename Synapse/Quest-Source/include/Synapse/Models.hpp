#pragma once
// MIT. Field contract: Aeroluna's Synapse.Networking.Models (2024).
#include "Protocol.hpp"
#include <limits>
#include <optional>
#include <variant>

namespace Synapse::Quest::Models {
struct PlayerScore { std::int32_t score=0; float percentage=0; };
struct Ruleset {
  std::optional<bool> allowOverrideColors, allowLeftHand, allowResubmission;
  // Missing means the upstream default []; explicit null stays null.
  std::optional<std::vector<std::string>> modifiers=std::vector<std::string>{};
};
struct Key { std::string characteristic; std::int32_t difficulty=0; };
struct Download {
  std::string gameVersion, url, hash;
  std::optional<std::string> key;
};
struct Map {
  std::string name;
  std::optional<std::string> altCoverUrl=std::string{};
  std::optional<Ruleset> ruleset;
  std::vector<Key> keys;
  std::vector<Download> downloads;
};
struct InvalidStage {};
struct IntroStage {
  std::string url;
  float startTime=std::numeric_limits<float>::lowest();
};
struct PlayStage {
  std::int32_t index=-1;
  float startTime=std::numeric_limits<float>::lowest();
  bool eliminated=false;
  std::optional<PlayerScore> playerScore;
  Map map;
};
struct FinishStage { std::string url; std::int32_t mapCount=0; };
using Stage=std::variant<InvalidStage,IntroStage,PlayStage,FinishStage>;
struct Status { std::string motd; Stage stage=InvalidStage{}; };
enum class MessageType : std::uint8_t { PrioritySystem, System, Say, WhisperFrom, WhisperTo };
struct ChatMessage {
  std::string id, username, message;
  std::optional<std::string> color;
  MessageType type=MessageType::PrioritySystem;
};
struct LeaderboardCell {
  std::int32_t rank=0, score=0;
  std::string playerName, color;
  float percentage=0;
};
struct LeaderboardScores {
  std::int32_t index=0, playerScoreIndex=-1, scoreCount=0, aliveCount=0;
  std::string title;
  std::vector<LeaderboardCell> scores;
};
struct BundleInfo {
  std::string gameVersion, url;
  std::uint32_t hash=0;
  // Optional Quest extension. An absent tag is UNKNOWN, never proof of Android.
  std::optional<std::string> platform;
};
enum class BundlePlatform { Unknown, Android, Windows, Unsupported };
BundlePlatform PlatformOf(BundleInfo const&);
struct TakeoverInfo {
  bool disableDust=false, disableLogo=false;
  std::string countdownTMP;
  std::vector<BundleInfo> bundles;
};
struct LobbyInfo {
  bool disableDust=false, disableSmoke=false;
  std::int32_t depthTextureMode=0;
  std::vector<BundleInfo> bundles;
};
struct Division { std::string name, description; };
struct ModInfo { std::string hash, id, url, version; };
struct RequiredMods { std::string gameVersion; std::vector<ModInfo> mods; };
struct Listing {
  std::string guid, title, ipAddress, bannerImage, bannerColor, gameVersion;
  // Preserve DateTime's ISO representation; UI timezone conversion is separate.
  std::string time="0001-01-01T00:00:00";
  std::vector<Division> divisions;
  TakeoverInfo takeover;
  LobbyInfo lobby;
  std::vector<RequiredMods> requiredMods;
};
struct ScoreSubmission { std::int32_t division=0, index=0, score=0; float percentage=0; };

// Bounded, strict JSON parsing. No coercion of booleans/integers, nonfinite
// numbers, duplicate keys, deep nesting, null nonnullable objects, or raw data in
// exceptions. Unknown fields are accepted for forward-compatible servers.
Status ParseStatus(std::string_view);
ChatMessage ParseChat(std::string_view);
LeaderboardScores ParseLeaderboard(std::string_view);
Listing ParseListing(std::string_view); // HTTP response limit: 1 MiB
std::string SerializeScore(ScoreSubmission const&);
using JsonMessage=std::variant<Status,ChatMessage,LeaderboardScores>;
JsonMessage DecodeJsonMessage(ServerMessage const&);
} // namespace Synapse::Quest::Models
