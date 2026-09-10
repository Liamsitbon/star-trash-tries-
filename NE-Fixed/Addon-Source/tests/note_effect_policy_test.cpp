#include "NoteEffectPolicy.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>

using namespace NEFixed::NoteEffects;

static rapidjson::Document Parse(char const* text) {
  rapidjson::Document doc;
  doc.Parse(text);
  assert(!doc.HasParseError());
  return doc;
}

static Policy FromMap(rapidjson::Value const& map) {
  Policy policy;
  auto notes = [&](rapidjson::Value const* list) {
    if (!list || !list->IsArray()) return;
    for (auto const& note : list->GetArray())
      if (auto* data = Field(note, "customData", "_customData")) policy.AddNote(*data);
  };
  notes(Field(map, "colorNotes", "_notes"));
  notes(Field(map, "bombNotes"));
  auto* custom = Field(map, "customData", "_customData");
  if (!custom) return policy;
  policy.AddMapCustomData(*custom);
  notes(Field(*custom, "fakeColorNotes"));
  notes(Field(*custom, "fakeBombNotes"));
  auto* events = Field(*custom, "customEvents", "_customEvents");
  if (!events || !events->IsArray()) return policy;
  for (auto const& event : events->GetArray()) {
    auto* type = Field(event, "t", "_type");
    auto* data = Field(event, "d", "_data");
    if (type && type->IsString() && data)
      policy.AddEvent({type->GetString(), type->GetStringLength()}, *data);
  }
  return policy;
}

int main(int argc, char** argv) {
  // Ordinary Chroma notes, walls, sabers and environment animation must not
  // opt a map into default notes merely because it has custom data/events.
  Policy plain;
  plain.AddNote(Parse(R"({"track":"notes","color":[1,0,0]})"));
  plain.AddEvent("AnimateTrack", Parse(R"({"track":"environment","position":[1,2,3]})"));
  plain.AddEvent("AssignObjectPrefab", Parse(R"({"saber":{"asset":"saber.prefab"}})"));
  assert(!plain.RequiresDefaultNotes());
  plain.AddEvent("AnimateTrack", Parse(R"({"track":"notes","dissolveArrow":[[0,0],[1,1]]})"));
  assert(plain.RequiresDefaultNotes());

  Policy parent;
  parent.AddEvent("AssignTrackParent", Parse(R"({"childrenTracks":["parent"],"parentTrack":"child"})"));
  parent.AddEvent("AssignTrackParent", Parse(R"({"childrenTracks":["child"],"parentTrack":"parent"})"));
  parent.AddNote(Parse(R"({"_track":["child","unrelated"]})"));
  assert(!parent.RequiresDefaultNotes()); // Cycle terminates.
  parent.AddEvent("AssignPathAnimation", Parse(R"({"_track":"parent","_dissolve":[[0,0],[1,1]]})"));
  assert(parent.RequiresDefaultNotes());

  Policy direct;
  direct.AddNote(Parse(R"({"animation":{"offsetPosition":[[0,1,0,0],[0,0,0,1]]}})"));
  assert(direct.RequiresDefaultNotes());
  Policy mapPrefab;
  mapPrefab.AddEvent("AssignObjectPrefab", Parse(R"({"colorNotes":{"asset":"mapnote.prefab"}})"));
  assert(mapPrefab.RequiresDefaultNotes());
  Policy malformed;
  malformed.AddNote(Parse("null"));
  malformed.AddEvent("AssignTrackParent", Parse(R"({"childrenTracks":{},"parentTrack":3})"));
  malformed.AddEvent("AnimateTrack", Parse(R"({"track":null,"dissolve":null})"));
  assert(!malformed.RequiresDefaultNotes());

  Policy lateFake;
  lateFake.AddMapCustomData(Parse(R"({"fakeColorNotes":[{"b":0,"x":0,"y":0,"c":0,"d":1}]})"));
  assert(lateFake.RequiresDefaultNotes()); // No customData/NE_fake marker needed.
  Policy fakeBomb;
  fakeBomb.AddMapCustomData(Parse(R"({"fakeBombNotes":[{"b":2,"x":1,"y":1}]})"));
  assert(fakeBomb.RequiresDefaultNotes());
  Policy noFake;
  noFake.AddMapCustomData(Parse(R"({"fakeColorNotes":[],"fakeBombNotes":[null,1],"fakeObstacles":[{}]})"));
  assert(!noFake.RequiresDefaultNotes()); // Walls alone do not disable cosmetics.

  // The classifier owns no Unity objects and no persistent scene decision.
  Policy nextOrdinaryScene;
  nextOrdinaryScene.AddNote(Parse(R"({"color":[0,1,0]})"));
  assert(!nextOrdinaryScene.RequiresDefaultNotes());

  for (int i = 1; i < argc; ++i) {
    std::ifstream stream(argv[i]);
    assert(stream.good());
    std::string text{std::istreambuf_iterator<char>(stream), {}};
    auto map = Parse(text.c_str());
    bool result = FromMap(map).RequiresDefaultNotes();
    std::cout << argv[i] << ": defaultNotes=" << result << '\n';
    assert(result);
  }
  std::cout << "Note-effect policy tests passed\n";
}
