#include "Synapse/GameplayPlan.hpp"
#include <cassert>
#include <iostream>
using namespace Synapse::Quest;
template<class F> void Reject(F&& f) {
  bool bad=false; try { f(); } catch(ProtocolError const&) { bad=true; } assert(bad);
}
int main() {
  constexpr auto version="1.40.8_7379";
  assert(MatchesGameVersion("1.29.1,1.40.8_7379",version));
  assert(MatchesGameVersion("1.40.8_7379,1.29.1",version));
  assert(!MatchesGameVersion("1.40.8",version));
  assert(!MatchesGameVersion("1.40.8_73790",version));
  assert(!MatchesGameVersion(" 1.40.8_7379",version)); // exact upstream behavior
  assert(!MatchesGameVersion("", ""));
  Models::Map map;
  map.keys={{"Standard",3},{"Standard",4}};
  map.downloads={{"1.29.1","https://example.invalid/old","old",{}},
    {"1.29.1,1.40.8_7379","https://example.invalid/new","new",std::string("synthetic-key")}};
  auto plan=PlanGameplay(map,1,version,true,true);
  assert(plan.key.difficulty==4 && plan.download.hash=="new" && plan.download.key=="synthetic-key");
  assert(plan.leftHanded && plan.usePlayerColorOverride && !plan.allowResubmission && plan.modifiers.none());
  Models::Ruleset rules;
  rules.allowLeftHand=false; rules.allowOverrideColors=false; rules.allowResubmission=true;
  rules.modifiers=std::vector<std::string>{"noFailOn0Energy","InstaFail","failOnSaberClash","noBombs",
    "fastNotes","strictAngles","disappearingArrows","noArrows","ghostNotes","proMode","zenMode","smallCubes"," noEnergy "};
  map.ruleset=rules; auto original=map;
  plan=PlanGameplay(map,0,version,true,true);
  assert(plan.modifiers.all() && !plan.leftHanded && !plan.usePlayerColorOverride && plan.allowResubmission);
  assert(plan.unrecognizedModifiers.empty() && map==original); // saved settings/map are not mutated
  map.ruleset->modifiers=std::vector<std::string>{"noEnergy","12","SMALLCUBES","futureFlag","999999999999999999999"};
  plan=PlanGameplay(map,0,version,true,true);
  assert(plan.Has(GameplayModifier::NoEnergy) && plan.Has(GameplayModifier::SmallCubes));
  assert(!plan.Has(GameplayModifier::NoFailOn0Energy) && plan.modifiers.count()==2);
  assert(plan.unrecognizedModifiers.size()==2);
  map.ruleset->modifiers.reset(); map.ruleset->allowLeftHand.reset(); map.ruleset->allowOverrideColors.reset();
  plan=PlanGameplay(map,0,version,true,true);
  assert(plan.modifiers.none() && plan.leftHanded && plan.usePlayerColorOverride);
  // A new map never inherits noEnergy or another previous map's modifier.
  map.ruleset.reset(); assert(!PlanGameplay(map,0,version,true,true).Has(GameplayModifier::NoEnergy));
  Reject([&]{PlanGameplay(map,-1,version,true,true);});
  Reject([&]{PlanGameplay(map,2,version,true,true);});
  Reject([&]{PlanGameplay(map,0,"1.40.8",true,true);});
  auto bad=map; bad.downloads[1].hash.clear(); Reject([&]{PlanGameplay(bad,0,version,true,true);});
  bad=map; bad.keys[0].difficulty=5; Reject([&]{PlanGameplay(bad,0,version,true,true);});
  std::vector<Models::BundleInfo> bundles={{version,"a",1,std::string("windows64")},
    {version,"b",2,{}},{version,"c",3,std::string("android")},
    {"1.29.1","d",4,std::string("android")},{version,"e",5,std::string("quest")}};
  auto candidates=FindQuestBundleCandidates(bundles,version);
  assert((candidates.androidTagged==std::vector<std::size_t>{2,4}));
  assert((candidates.platformInspectionRequired==std::vector<std::size_t>{1}));
  std::cout<<"Synapse gameplay plan: 13 modifiers, exact versions, division, independent map state, bundle candidates passed\n";
}
