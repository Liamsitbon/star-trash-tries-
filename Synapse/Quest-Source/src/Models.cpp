#include "Synapse/Models.hpp"
#include <nlohmann/json.hpp>
#include <unordered_set>

namespace Synapse::Quest::Models {
namespace {
using Json=nlohmann::json;
[[noreturn]] void Bad() { throw ProtocolError("invalid Synapse JSON field or shape"); }
Json Parse(std::string_view text, std::size_t limit=MaxBody) {
  if(text.empty() || text.size()>limit) throw ProtocolError("Synapse JSON size limit");
  std::array<std::unordered_set<std::string>,34> objectKeys;
  try {
    auto value=Json::parse(text,[&](int depth,Json::parse_event_t event,Json& parsed) {
      if(depth<0 || depth>32) throw ProtocolError("Synapse JSON nesting limit");
      if(event==Json::parse_event_t::object_start) objectKeys[depth+1].clear();
      if(event==Json::parse_event_t::key &&
         !objectKeys[depth].insert(parsed.get<std::string>()).second)
        throw ProtocolError("duplicate Synapse JSON field");
      return true;
    });
    if(!value.is_object()) Bad();
    return value;
  } catch(Json::exception const&) {
    // Do not propagate nlohmann's source excerpts (may contain keys/tokens).
    throw ProtocolError("malformed Synapse JSON");
  }
}
Json const& Object(Json const& j) { if(!j.is_object()) Bad(); return j; }
Json const* Field(Json const& j,char const* key) {
  Object(j); auto i=j.find(key); return i==j.end() ? nullptr : &*i;
}
std::string String(Json const& j) { if(!j.is_string()) Bad(); return j.get<std::string>(); }
std::string Text(Json const& j,char const* key,std::string fallback={}) {
  auto p=Field(j,key); return p ? String(*p) : std::move(fallback);
}
std::optional<std::string> NullableText(Json const& j,char const* key,
                                      std::optional<std::string> fallback={}) {
  auto p=Field(j,key); if(!p) return fallback;
  return p->is_null() ? std::nullopt : std::optional(String(*p));
}
std::int32_t IntValue(Json const& j) {
  if(!j.is_number_integer()) Bad();
  if(j.is_number_unsigned()) {
    auto n=j.get<std::uint64_t>(); if(n>INT32_MAX) Bad(); return static_cast<std::int32_t>(n);
  }
  auto n=j.get<std::int64_t>(); if(n<INT32_MIN || n>INT32_MAX) Bad();
  return static_cast<std::int32_t>(n);
}
std::int32_t Int(Json const& j,char const* key,std::int32_t fallback=0) {
  auto p=Field(j,key); return p ? IntValue(*p) : fallback;
}
std::uint32_t UInt(Json const& j,char const* key) {
  auto p=Field(j,key); if(!p) return 0;
  if(!p->is_number_integer() || (!p->is_number_unsigned() && p->get<std::int64_t>()<0)) Bad();
  auto n=p->get<std::uint64_t>(); if(n>UINT32_MAX) Bad(); return static_cast<std::uint32_t>(n);
}
float Float(Json const& j,char const* key,float fallback=0) {
  auto p=Field(j,key); if(!p) return fallback;
  if(!p->is_number()) Bad();
  auto n=p->get<double>();
  if(!std::isfinite(n) || std::abs(n)>std::numeric_limits<float>::max()) Bad();
  return static_cast<float>(n);
}
bool Bool(Json const& j,char const* key,bool fallback=false) {
  auto p=Field(j,key); if(!p) return fallback;
  if(!p->is_boolean()) Bad();
  return p->get<bool>();
}
std::optional<bool> NullableBool(Json const& j,char const* key) {
  auto p=Field(j,key); if(!p || p->is_null()) return {};
  if(!p->is_boolean()) Bad();
  return p->get<bool>();
}
template<class T,class F> std::vector<T> Array(Json const& j,F parse) {
  if(!j.is_array() || j.size()>4096) Bad();
  std::vector<T> result; result.reserve(j.size());
  for(auto const& item:j) result.push_back(parse(item));
  return result;
}
template<class T,class F> std::vector<T> List(Json const& j,char const* key,F parse) {
  auto p=Field(j,key); return p ? Array<T>(*p,parse) : std::vector<T>{};
}
Ruleset ReadRuleset(Json const& j) {
  Ruleset r;
  r.allowOverrideColors=NullableBool(j,"allowOverrideColors");
  r.allowLeftHand=NullableBool(j,"allowLeftHand");
  r.allowResubmission=NullableBool(j,"allowResubmission");
  if(auto p=Field(j,"modifiers")) {
    if(p->is_null()) r.modifiers.reset();
    else r.modifiers=Array<std::string>(*p,String);
  }
  return r;
}
PlayerScore ReadPlayerScore(Json const& j) { return {Int(j,"score"),Float(j,"percentage")}; }
Map ReadMap(Json const& j) {
  Map m;
  m.name=Text(j,"name"); m.altCoverUrl=NullableText(j,"altCoverUrl",std::string{});
  if(auto p=Field(j,"ruleset"); p && !p->is_null()) m.ruleset=ReadRuleset(*p);
  m.keys=List<Key>(j,"keys",[](Json const& x) {
    Key k{Text(x,"characteristic"),Int(x,"difficulty")};
    if(k.difficulty<0 || k.difficulty>4) Bad();
    return k;
  });
  m.downloads=List<Download>(j,"downloads",[](Json const& x) {
    return Download{Text(x,"gameVersion"),Text(x,"url"),Text(x,"hash"),NullableText(x,"key")};
  });
  return m;
}
Stage ReadStage(Json const& j) {
  auto name=Text(j,"name");
  if(name=="invalid") return InvalidStage{};
  if(name=="intro") return IntroStage{Text(j,"url"),Float(j,"startTime",std::numeric_limits<float>::lowest())};
  if(name=="finish") return FinishStage{Text(j,"url"),Int(j,"mapCount")};
  if(name!="play") throw ProtocolError("unknown Synapse stage");
  PlayStage s;
  s.index=Int(j,"index",-1); s.startTime=Float(j,"startTime",std::numeric_limits<float>::lowest());
  s.eliminated=Bool(j,"eliminated");
  if(auto p=Field(j,"playerScore"); p && !p->is_null()) s.playerScore=ReadPlayerScore(*p);
  if(auto p=Field(j,"map")) s.map=ReadMap(*p);
  return s;
}
BundleInfo ReadBundle(Json const& j) {
  return {Text(j,"gameVersion"),Text(j,"url"),UInt(j,"hash"),NullableText(j,"platform")};
}
} // namespace
Status ParseStatus(std::string_view text) {
  auto j=Parse(text); Status s; s.motd=Text(j,"motd");
  if(auto p=Field(j,"stage")) s.stage=ReadStage(*p);
  return s;
}
ChatMessage ParseChat(std::string_view text) {
  auto j=Parse(text); auto t=Int(j,"type"); if(t<0 || t>4) Bad();
  return {Text(j,"id"),Text(j,"username"),Text(j,"message"),NullableText(j,"color"),static_cast<MessageType>(t)};
}
LeaderboardScores ParseLeaderboard(std::string_view text) {
  auto j=Parse(text); LeaderboardScores s;
  s.index=Int(j,"index"); s.title=Text(j,"title"); s.playerScoreIndex=Int(j,"playerScoreIndex",-1);
  s.scoreCount=Int(j,"scoreCount"); s.aliveCount=Int(j,"aliveCount");
  s.scores=List<LeaderboardCell>(j,"scores",[](Json const& x) {
    return LeaderboardCell{Int(x,"rank"),Int(x,"score"),Text(x,"playerName"),Text(x,"color"),Float(x,"percentage")};
  });
  return s;
}
Listing ParseListing(std::string_view text) {
  auto j=Parse(text,1024*1024); Listing l;
  l.guid=Text(j,"guid"); l.title=Text(j,"title"); l.ipAddress=Text(j,"ipAddress");
  l.bannerImage=Text(j,"bannerImage"); l.bannerColor=Text(j,"bannerColor");
  l.gameVersion=Text(j,"gameVersion"); l.time=Text(j,"time",l.time);
  l.divisions=List<Division>(j,"divisions",[](Json const& x) { return Division{Text(x,"name"),Text(x,"description")}; });
  if(auto p=Field(j,"takeover")) {
    l.takeover.disableDust=Bool(*p,"disableDust"); l.takeover.disableLogo=Bool(*p,"disableLogo");
    l.takeover.countdownTMP=Text(*p,"countdownTMP"); l.takeover.bundles=List<BundleInfo>(*p,"bundles",ReadBundle);
  }
  if(auto p=Field(j,"lobby")) {
    l.lobby.disableDust=Bool(*p,"disableDust"); l.lobby.disableSmoke=Bool(*p,"disableSmoke");
    l.lobby.depthTextureMode=Int(*p,"depthTextureMode"); l.lobby.bundles=List<BundleInfo>(*p,"bundles",ReadBundle);
  }
  l.requiredMods=List<RequiredMods>(j,"requiredMods",[](Json const& x) {
    return RequiredMods{Text(x,"gameVersion"),List<ModInfo>(x,"mods",[](Json const& v) {
      return ModInfo{Text(v,"hash"),Text(v,"id"),Text(v,"url"),Text(v,"version")};
    })};
  });
  return l;
}
BundlePlatform PlatformOf(BundleInfo const& bundle) {
  if(!bundle.platform || bundle.platform->empty()) return BundlePlatform::Unknown;
  if(*bundle.platform=="android" || *bundle.platform=="quest") return BundlePlatform::Android;
  if(*bundle.platform=="windows" || *bundle.platform=="windows64") return BundlePlatform::Windows;
  return BundlePlatform::Unsupported;
}
std::string SerializeScore(ScoreSubmission const& s) {
  if(!std::isfinite(s.percentage)) Bad();
  return Json{{"division",s.division},{"index",s.index},{"score",s.score},{"percentage",s.percentage}}.dump();
}
JsonMessage DecodeJsonMessage(ServerMessage const& message) {
  switch(message.opcode) {
    case FromServer::Status:return ParseStatus(message.text);
    case FromServer::ChatMessage:return ParseChat(message.text);
    case FromServer::LeaderboardScores:return ParseLeaderboard(message.text);
    default:throw ProtocolError("opcode does not carry Synapse model JSON");
  }
}
} // namespace Synapse::Quest::Models
