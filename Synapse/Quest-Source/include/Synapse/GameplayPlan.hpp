#pragma once
#include "Models.hpp"
#include <bitset>

namespace Synapse::Quest {
// All thirteen named modifiers in upstream LevelStartManager. NoEnergy is a
// separate per-map request, not a persistent global flag or NoFail score penalty.
enum class GameplayModifier : std::size_t {
  NoFailOn0Energy, InstaFail, FailOnSaberClash, NoBombs, FastNotes, StrictAngles,
  DisappearingArrows, NoArrows, GhostNotes, ProMode, ZenMode, SmallCubes, NoEnergy,
  Count
};
struct GameplayPlan {
  Models::Key key;
  Models::Download download;
  std::int32_t division=0;
  bool usePlayerColorOverride=false, leftHanded=false, allowResubmission=false;
  std::bitset<static_cast<std::size_t>(GameplayModifier::Count)> modifiers;
  // Upstream ignores unrecognized modifiers. Preserve/report them instead of
  // pretending they were applied or enabling an unrelated modifier.
  std::vector<std::string> unrecognizedModifiers;
  bool Has(GameplayModifier value) const { return modifiers.test(static_cast<std::size_t>(value)); }
};
// Upstream uses comma-delimited EXACT version membership, not prefix/semver
// fallback. Which server version string represents Quest is an adapter decision.
bool MatchesGameVersion(std::string_view accepted,std::string_view gameVersion);
std::optional<std::size_t> FindDownload(Models::Map const&,std::string_view gameVersion);
// A pure owned plan: no mutation of saved player settings, no scene teardown,
// no download and no Unity pointers. Heck's map settings still take precedence
// when the future Quest level-start adapter applies this plan.
GameplayPlan PlanGameplay(Models::Map const&,std::int32_t division,std::string_view gameVersion,
                          bool playerLeftHanded,bool playerColorOverride);
struct BundleCandidates {
  std::vector<std::size_t> androidTagged, platformInspectionRequired;
};
// A game-version match never proves a Windows/untagged bundle is Android.
// Even androidTagged candidates still require hash/header verification.
BundleCandidates FindQuestBundleCandidates(std::vector<Models::BundleInfo> const&,std::string_view gameVersion);
} // namespace Synapse::Quest
