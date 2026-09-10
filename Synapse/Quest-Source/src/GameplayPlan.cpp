#include "Synapse/GameplayPlan.hpp"
#include <algorithm>
#include <charconv>

namespace Synapse::Quest {
bool MatchesGameVersion(std::string_view accepted,std::string_view version) {
  if(version.empty()) return false;
  for(std::size_t start=0;start<=accepted.size();) {
    auto end=accepted.find(',',start);
    auto part=accepted.substr(start,end==std::string_view::npos?end:end-start);
    if(part==version) return true;
    if(end==std::string_view::npos) break;
    start=end+1;
  }
  return false;
}
std::optional<std::size_t> FindDownload(Models::Map const& map,std::string_view version) {
  for(std::size_t i=0;i<map.downloads.size();++i)
    if(MatchesGameVersion(map.downloads[i].gameVersion,version)) return i;
  return {};
}
namespace {
std::optional<std::size_t> Modifier(std::string value) {
  constexpr std::array<std::string_view,13> names={"nofailon0energy","instafail","failonsaberclash",
    "nobombs","fastnotes","strictangles","disappearingarrows","noarrows","ghostnotes",
    "promode","zenmode","smallcubes","noenergy"};
  auto first=value.find_first_not_of(" \t\r\n"),last=value.find_last_not_of(" \t\r\n");
  if(first==std::string::npos) return {};
  value=value.substr(first,last-first+1);
  for(char& c:value) if(c>='A'&&c<='Z') c=static_cast<char>(c-'A'+'a');
  for(std::size_t i=0;i<names.size();++i) if(value==names[i]) return i;
  // Enum.TryParse also accepts numeric enum values. Accept only the defined
  // values, never overflow or a cast that indexes outside the modifier table.
  int numeric=-1;
  auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),numeric);
  if(error==std::errc{} && end==value.data()+value.size() && numeric>=0 && numeric<13)
    return static_cast<std::size_t>(numeric);
  return {};
}
} // namespace
GameplayPlan PlanGameplay(Models::Map const& map,std::int32_t division,std::string_view version,
    bool leftHanded,bool colorOverride) {
  if(division<0 || static_cast<std::size_t>(division)>=map.keys.size())
    throw ProtocolError("Synapse division has no matching difficulty");
  auto const& key=map.keys[division];
  if(key.characteristic.empty() || key.difficulty<0 || key.difficulty>4)
    throw ProtocolError("Synapse difficulty key is invalid");
  auto index=FindDownload(map,version);
  if(!index) throw ProtocolError("No Synapse download for exact game version");
  GameplayPlan plan;
  plan.key=key; plan.download=map.downloads[*index]; plan.division=division;
  if(plan.download.url.empty() || plan.download.hash.empty())
    throw ProtocolError("Synapse download identity is incomplete");
  plan.leftHanded=leftHanded; plan.usePlayerColorOverride=colorOverride;
  if(map.ruleset) {
    auto const& rules=*map.ruleset;
    if(rules.allowLeftHand==false) plan.leftHanded=false;
    if(rules.allowOverrideColors==false) plan.usePlayerColorOverride=false;
    plan.allowResubmission=rules.allowResubmission.value_or(false);
    if(rules.modifiers) for(auto const& name:*rules.modifiers) {
      if(auto modifier=Modifier(name)) plan.modifiers.set(*modifier);
      else plan.unrecognizedModifiers.push_back(name);
    }
  }
  return plan;
}
BundleCandidates FindQuestBundleCandidates(std::vector<Models::BundleInfo> const& bundles,std::string_view version) {
  BundleCandidates result;
  for(std::size_t i=0;i<bundles.size();++i) {
    if(!MatchesGameVersion(bundles[i].gameVersion,version)) continue;
    auto platform=Models::PlatformOf(bundles[i]);
    if(platform==Models::BundlePlatform::Android) result.androidTagged.push_back(i);
    else if(platform==Models::BundlePlatform::Unknown) result.platformInspectionRequired.push_back(i);
  }
  return result;
}
} // namespace Synapse::Quest
