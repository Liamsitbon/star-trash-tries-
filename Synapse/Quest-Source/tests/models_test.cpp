#include "Synapse/Models.hpp"
#include <cassert>
#include <iostream>
using namespace Synapse::Quest;
using namespace Synapse::Quest::Models;
template<class F> void Reject(F&& f) {
  bool bad=false; try { f(); } catch(ProtocolError const&) { bad=true; } assert(bad);
}
int main() {
  assert(std::holds_alternative<InvalidStage>(ParseStatus("{}").stage));
  assert(std::holds_alternative<InvalidStage>(ParseStatus(R"({"stage":{"name":"invalid"}})").stage));
  auto intro=std::get<IntroStage>(ParseStatus(R"({"stage":{"name":"intro","url":"https://example.invalid/intro","startTime":12.5}})").stage);
  assert(intro.startTime==12.5f && intro.url.ends_with("/intro"));
  auto finish=std::get<FinishStage>(ParseStatus(R"({"stage":{"name":"finish","url":"end","mapCount":7}})").stage);
  assert(finish.mapCount==7 && finish.url=="end");
  auto status=ParseStatus(R"({"motd":"שלום 🎵","stage":{"name":"play","index":3,"startTime":51.25,
    "eliminated":true,"playerScore":{"score":2000000,"percentage":98.25},
    "map":{"name":"Dynasty","altCoverUrl":null,"ruleset":{"allowOverrideColors":false,
      "allowLeftHand":true,"allowResubmission":null,"modifiers":["noFail","ghostNotes"]},
      "keys":[{"characteristic":"Standard","difficulty":4}],"downloads":[{"gameVersion":"1.40.8_7379",
        "url":"https://example.invalid/map","hash":"md5","key":"synthetic-key"}]}}})");
  assert(status.motd=="שלום 🎵"); auto& s=std::get<PlayStage>(status.stage);
  assert(s.index==3 && s.startTime==51.25 && s.eliminated && s.playerScore->score==2000000);
  assert(s.playerScore->percentage==98.25f && s.map.name=="Dynasty" && !s.map.altCoverUrl);
  assert(s.map.ruleset->allowOverrideColors==false && s.map.ruleset->allowLeftHand==true);
  assert(!s.map.ruleset->allowResubmission && s.map.ruleset->modifiers->size()==2);
  assert(s.map.keys.at(0).difficulty==4 && s.map.downloads.at(0).key=="synthetic-key");
  auto defaults=std::get<PlayStage>(ParseStatus(R"({"stage":{"name":"play","map":{"ruleset":{}}}})").stage);
  assert(defaults.index==-1 && defaults.startTime==std::numeric_limits<float>::lowest());
  assert(!defaults.playerScore && defaults.map.ruleset->modifiers->empty() && defaults.map.altCoverUrl=="");
  assert(!std::get<PlayStage>(ParseStatus(R"({"stage":{"name":"play","map":{"ruleset":{"modifiers":null}}}})").stage).map.ruleset->modifiers);
  for(int i=0;i<5;++i) {
    auto chat=ParseChat("{\"type\":"+std::to_string(i)+R"(,"id":"123","username":"Liam","message":"שלום","color":null})");
    assert(static_cast<int>(chat.type)==i && chat.id=="123" && chat.username=="Liam" && chat.message=="שלום" && !chat.color);
  }
  auto lb=ParseLeaderboard(R"({"index":2,"title":"Final","playerScoreIndex":1,"scoreCount":3,"aliveCount":2,
    "scores":[{"rank":1,"playerName":"A","score":600,"percentage":96.5,"color":"#abcdef"}]})");
  assert(lb.index==2 && lb.playerScoreIndex==1 && lb.title=="Final" && lb.scoreCount==3 && lb.aliveCount==2);
  assert(lb.scores.at(0).rank==1 && lb.scores[0].score==600 && lb.scores[0].playerName=="A" && lb.scores[0].percentage==96.5 && lb.scores[0].color=="#abcdef");
  auto listing=ParseListing(R"({"guid":"guid","title":"Show","ipAddress":"127.0.0.1:4000","bannerImage":"image",
    "bannerColor":"#123456","gameVersion":"1.40.8_7379","time":"2026-09-10T00:00:00Z",
    "divisions":[{"name":"Open","description":"All players"}],"takeover":{"disableDust":true,"disableLogo":true,
      "countdownTMP":"Countdown","bundles":[{"gameVersion":"1.40.8_7379","url":"takeover","hash":4294967295}]},
    "lobby":{"disableDust":true,"disableSmoke":true,"depthTextureMode":3,
      "bundles":[{"gameVersion":"1.40.8_7379","url":"lobby","hash":42,"platform":"android"}]},
    "requiredMods":[{"gameVersion":"1.40.8_7379","mods":[{"id":"vivify","version":"0.6.13","hash":"sha256","url":"qmod"}]}]})");
  assert(listing.guid=="guid" && listing.title=="Show" && listing.ipAddress=="127.0.0.1:4000");
  assert(listing.bannerImage=="image" && listing.bannerColor=="#123456" && listing.gameVersion=="1.40.8_7379");
  assert(listing.time=="2026-09-10T00:00:00Z" && listing.divisions[0].description=="All players");
  assert(listing.takeover.disableDust && listing.takeover.disableLogo && listing.takeover.countdownTMP=="Countdown");
  assert(listing.takeover.bundles[0].hash==UINT32_MAX && PlatformOf(listing.takeover.bundles[0])==BundlePlatform::Unknown);
  assert(listing.lobby.disableDust && listing.lobby.disableSmoke && listing.lobby.depthTextureMode==3);
  assert(PlatformOf(listing.lobby.bundles[0])==BundlePlatform::Android);
  auto& mod=listing.requiredMods.at(0).mods.at(0); assert(mod.id=="vivify" && mod.hash=="sha256" && mod.url=="qmod" && mod.version=="0.6.13");
  listing.lobby.bundles[0].platform="windows64"; assert(PlatformOf(listing.lobby.bundles[0])==BundlePlatform::Windows);
  auto score=SerializeScore({2,3,10000,98.5f});
  assert(score.find("\"division\":2")!=std::string::npos && score.find("\"percentage\":98.5")!=std::string::npos);
  ServerMessage message; message.opcode=FromServer::Status; message.text="{}";
  assert(std::holds_alternative<Status>(DecodeJsonMessage(message)));
  message.opcode=FromServer::Ping; Reject([&]{DecodeJsonMessage(message);});
  // Reject wrong types/ranges instead of silently accepting different gameplay rules.
  for(auto invalid:{"[]","null",R"({"stage":null})",R"({"stage":{"name":"unknown"}})",R"({"stage":{"name":"play","index":1.0}})",
       R"({"stage":{"name":"play","index":4294967296}})",R"({"stage":{"name":"play","eliminated":1}})",
       R"({"stage":{"name":"play","startTime":1e100}})",R"({"stage":{"name":"play","map":{"keys":[{"difficulty":5}]}}})",
       R"({"stage":{"name":"play","map":{"ruleset":{"modifiers":true}}}})",R"({"motd":"a","motd":"b"})",
       R"({"stage":{"name":"play","map":{"name":"a","name":"b"}}})"}) Reject([&]{ParseStatus(invalid);});
  Reject([]{ParseChat(R"({"type":5})");});
  Reject([]{ParseLeaderboard(R"({"scores":null})");});
  Reject([]{ParseListing(R"({"lobby":{"bundles":[{"hash":-1}]}})");});
  Reject([]{ParseListing(R"({"lobby":{"bundles":[{"hash":4294967296}]}})");});
  Reject([]{SerializeScore({0,0,0,std::numeric_limits<float>::infinity()});});
  Reject([]{ParseStatus(std::string(MaxBody+1,' '));});
  Reject([]{ParseListing(std::string(1024*1024+1,' '));});
  std::string deep=R"({"unknown":)"+std::string(40,'[')+"0"+std::string(40,']')+"}";
  Reject([&]{ParseStatus(deep);});
  // Repeated field names in different objects are valid; unknown keys preserved by
  // the wire message can coexist with known typed fields without breaking parsing.
  ParseStatus(R"({"stage":{"name":"play","map":{"name":"x"}},"extra":{"name":"y"}})");
  std::cout<<"Synapse typed JSON models passed\n";
}
