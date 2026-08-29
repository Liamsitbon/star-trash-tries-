#include "FakeNoteHelper.h"

#include "GlobalNamespace/NoteController.hpp"
#include "GlobalNamespace/ObstacleController.hpp"

#include "custom-json-data/shared/CustomBeatmapData.h"
#include "custom-json-data/shared/CustomBeatmapSaveDatav3.h"

#include "NELogger.h"
#include "Constants.hpp"
#include "AssociatedData.h"

#include "System/Collections/Generic/List_1.hpp"
#include "System/Collections/Generic/IList_1.hpp"
#include "custom-json-data/shared/VList.h"

using namespace GlobalNamespace;


bool FakeNoteHelper::EnsureV3FakeObjectsInjected(CustomJSONData::v3::CustomBeatmapSaveData* beatmap) {
  using namespace CustomJSONData::v3;

  if (!beatmap || !beatmap->doc || !beatmap->customData) return false;

  auto& customData = const_cast<rapidjson::Value&>(beatmap->customData->get());
  if (!customData.IsObject()) return false;

  static constexpr char kInjectedMarker[] = "NE_fakeObjectsInjected";
  auto existingMarker = customData.FindMember(kInjectedMarker);
  if (existingMarker != customData.MemberEnd() && existingMarker->value.IsBool() &&
      existingMarker->value.GetBool()) {
    return true;
  }

  auto& alloc = beatmap->doc->GetAllocator();
  int injected = 0;

#define INJECT_V3_FAKE_ARRAY(key, array, parse)                                                                        \
  do {                                                                                                                 \
    auto key##It = customData.FindMember(#key);                                                                        \
    if (key##It == customData.MemberEnd() || !key##It->value.IsArray() || !(array)) break;                             \
    for (auto& rawItem : key##It->value.GetArray()) {                                                                  \
      if (!rawItem.IsObject()) continue;                                                                               \
      auto customDataIt = rawItem.FindMember("customData");                                                           \
      if (customDataIt == rawItem.MemberEnd()) {                                                                       \
        rapidjson::Value object(rapidjson::kObjectType);                                                               \
        rawItem.AddMember(rapidjson::StringRef("customData"), object.Move(), alloc);                                  \
        customDataIt = rawItem.FindMember("customData");                                                              \
      }                                                                                                                \
      if (!customDataIt->value.IsObject()) customDataIt->value.SetObject();                                            \
      auto fakeIt = customDataIt->value.FindMember(                                                                   \
          NoodleExtensions::Constants::INTERNAL_FAKE_NOTE.data());                                                     \
      if (fakeIt == customDataIt->value.MemberEnd()) {                                                                 \
        customDataIt->value.AddMember(                                                                                 \
            rapidjson::StringRef(NoodleExtensions::Constants::INTERNAL_FAKE_NOTE.data()),                             \
            rapidjson::Value(true).Move(), alloc);                                                                     \
      } else {                                                                                                         \
        fakeIt->value.SetBool(true);                                                                                   \
      }                                                                                                                \
      auto* item = parse(rawItem);                                                                                     \
      if (!item) continue;                                                                                             \
      (array)->Add(item);                                                                                              \
      injected++;                                                                                                      \
    }                                                                                                                  \
  } while (false)

  INJECT_V3_FAKE_ARRAY(fakeColorNotes, beatmap->colorNotes, Parser::DeserializeColorNote);
  INJECT_V3_FAKE_ARRAY(fakeBombNotes, beatmap->bombNotes, Parser::DeserializeBombNote);
  INJECT_V3_FAKE_ARRAY(fakeObstacles, beatmap->obstacles, Parser::DeserializeObstacle);
  INJECT_V3_FAKE_ARRAY(fakeBurstSliders, beatmap->burstSliders, Parser::DeserializeBurstSlider);
  INJECT_V3_FAKE_ARRAY(fakeSliders, beatmap->sliders, Parser::DeserializeSlider);

#undef INJECT_V3_FAKE_ARRAY

  if (injected <= 0) return false;

  auto marker = customData.FindMember(kInjectedMarker);
  if (marker == customData.MemberEnd()) {
    customData.AddMember(rapidjson::StringRef(kInjectedMarker), rapidjson::Value(true).Move(), alloc);
  } else {
    marker->value.SetBool(true);
  }

  NELogger::Logger.info("Injected {} V3 fake object(s) into CJD's normal save-data path", injected);
  return true;
}


/**
 * @brief Checks if a note is a fake note.
 * 
 * @param noteData The note data to check.
 * @return true if the note is fake, false if the note is not fake.
 */
bool FakeNoteHelper::GetFakeNote(NoteData* noteData) {
  auto customNoteData = il2cpp_utils::try_cast<CustomJSONData::CustomNoteData>(noteData);
  if (!customNoteData || !customNoteData.value()->customData) {
    return false;
  }
  BeatmapObjectAssociatedData& ad = getAD(customNoteData.value()->customData);
  // `uninteractable` is part of the public V3 fake-object contract and is a
  // safe fallback when a legacy loader lost the internal NE_fake marker.
  return ad.objectData.fake.value_or(false) || ad.objectData.uninteractable.value_or(false);
}

bool FakeNoteHelper::GetCuttable(NoteData* noteData) {
  auto customNoteData = il2cpp_utils::try_cast<CustomJSONData::CustomNoteData>(noteData);
  if (!customNoteData || !customNoteData.value()->customData) {
    return true;
  }
  BeatmapObjectAssociatedData& ad = getAD(customNoteData.value()->customData);
  return !ad.objectData.uninteractable.value_or(false);
}

bool FakeNoteHelper::GetAttractableArc(SliderData* arcData) {
  auto customArcData = il2cpp_utils::try_cast<CustomJSONData::CustomSliderData>(arcData);
  if (!customArcData || !customArcData.value()->customData) {
    return true;
  }
  BeatmapObjectAssociatedData& ad = getAD(customArcData.value()->customData);
  return !ad.objectData.uninteractable.value_or(false);
}

System::Collections::Generic::List_1<GlobalNamespace::ObstacleController*>*
FakeNoteHelper::ObstacleFakeCheck(VList<GlobalNamespace::ObstacleController*> intersectingObstacles) {
  auto* filtered =
      System::Collections::Generic::List_1<GlobalNamespace::ObstacleController*>::New_ctor();

  for (auto const& obstacle : intersectingObstacles) {
    if (!obstacle || !obstacle->_obstacleData) continue;

    auto customObstacle =
        il2cpp_utils::try_cast<CustomJSONData::CustomObstacleData>(obstacle->_obstacleData);
    if (!customObstacle || !customObstacle.value()->customData || !customObstacle.value()->customData->value) {
      filtered->Add(obstacle);
      continue;
    }

    auto const& ad = getAD(customObstacle.value()->customData);
    if (!ad.objectData.fake.value_or(false)) {
      filtered->Add(obstacle);
    }
  }

  return filtered;
}
