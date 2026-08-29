// TODO: Fix with SongCore changes

#include "GlobalNamespace/BeatmapData.hpp"
#include "NELogger.h"
#include "beatsaber-hook/shared/utils/hooking.hpp"

#include "GlobalNamespace/BeatmapDataLoader.hpp"
#include "GlobalNamespace/BeatmapDataBasicInfo.hpp"

#include "custom-json-data/shared/CustomBeatmapSaveDatav3.h"

#include "NEHooks.h"
#include "NEJSON.h"
#include "SceneTransitionHelper.hpp"
#include "AssociatedData.h"
#include "FakeNoteHelper.h"

#include "BeatmapDataLoaderVersion3/BeatmapDataLoader.hpp"

#include "BeatmapDataLoaderVersion2_6_0AndEarlier/BeatmapDataLoader.hpp"
#include "BeatmapSaveDataVersion2_6_0AndEarlier/BeatmapSaveData.hpp"
#include "BeatmapSaveDataVersion2_6_0AndEarlier/NoteData.hpp"

#include "custom-json-data/shared/CustomBeatmapSaveDatav2.h"
#include "custom-json-data/shared/misc/BeatmapDataLoaderUtils.hpp"

#include "UnityEngine/JsonUtility.hpp"

#include "beatsaber-hook/shared/rapidjson/include/rapidjson/document.h"
#include "beatsaber-hook/shared/rapidjson/include/rapidjson/stringbuffer.h"
#include "beatsaber-hook/shared/rapidjson/include/rapidjson/writer.h"

#include <optional>
#include <string>

#include "Constants.hpp"
#include "sombrero/shared/linq_functional.hpp"
using namespace System;
using namespace System::Collections::Generic;
using namespace GlobalNamespace;
using namespace CustomJSONData;
using namespace UnityEngine;

// return true if fake
// subtracts from object count if fake
template <typename T> static bool IsFake(T* o, bool v2) {
  auto const optData = o->customData;

  if (!optData || !optData) return false;

  rapidjson::Value const& customData = *optData;

  auto fake = NEJSON::ReadOptionalBool(customData, v2 ? NoodleExtensions::Constants::V2_FAKE_NOTE
                                                      : NoodleExtensions::Constants::INTERNAL_FAKE_NOTE);
  return fake.value_or(false);
}

template <typename U, typename T> auto FakeCount(ArrayW<T> list, bool v2) {
  int i = list.size();
  for (auto o : list) {
    auto note = il2cpp_utils::try_cast<U>(o);
    if (!note) continue;

    if (IsFake(*note, v2)) i--;
  }

  return i;
}

MAKE_HOOK_MATCH(V2_BeatmapDataLoader_GetBeatmapDataBasicInfoFromSaveDataJson,
                &BeatmapDataLoaderVersion2_6_0AndEarlier::BeatmapDataLoader::GetBeatmapDataBasicInfoFromSaveDataJson,
                GlobalNamespace::BeatmapDataBasicInfo*, StringW beatmapSaveDataJson) {
  if (!beatmapSaveDataJson) return V2_BeatmapDataLoader_GetBeatmapDataBasicInfoFromSaveDataJson(beatmapSaveDataJson);

  auto beatmapSaveData =
      JsonUtility::FromJson<BeatmapSaveDataVersion2_6_0AndEarlier::BeatmapSaveData*>(beatmapSaveDataJson);
  if (beatmapSaveData == nullptr || beatmapSaveData->notes == nullptr) {
    return nullptr;
  }
  ListW<BeatmapSaveDataVersion2_6_0AndEarlier::NoteData*> notes = beatmapSaveData->notes;

  auto notBombs = notes | Sombrero::Linq::Functional::Where([](BeatmapSaveDataVersion2_6_0AndEarlier::NoteData* x) {
                    return x->type != BeatmapSaveDataVersion2_6_0AndEarlier::NoteType::Bomb;
                  }) |
                  Sombrero::Linq::Functional::ToArray();
  auto bombs = notes | Sombrero::Linq::Functional::Where([](BeatmapSaveDataVersion2_6_0AndEarlier::NoteData* x) {
                 return x->type == BeatmapSaveDataVersion2_6_0AndEarlier::NoteType::Bomb;
               }) |
               Sombrero::Linq::Functional::ToArray();

  int noteCount = FakeCount<v2::CustomBeatmapSaveData_NoteData>(notBombs, true);
  int bombCount = FakeCount<v2::CustomBeatmapSaveData_NoteData>(bombs, true);
  int obstacleCount = FakeCount<v2::CustomBeatmapSaveData_ObstacleData>(beatmapSaveData->obstacles->ToArray(), true);

  return BeatmapDataBasicInfo::New_ctor(4, noteCount, 0, obstacleCount, bombCount);
}

static JSONWrapper* JSONWrapperOrNull(v3::CustomDataOpt const& val) {
  auto* wrapper = JSONWrapper::New_ctor();

  if (!val || !val->get().IsObject()) {
    return wrapper;
  }

  wrapper->value = val;

  return wrapper;
}

namespace {
std::optional<StringW> InjectV3FakeObjectsIntoJson(StringW beatmapSaveDataJson) {
  if (!beatmapSaveDataJson) return std::nullopt;

  std::string source = static_cast<std::string>(beatmapSaveDataJson);
  rapidjson::Document document;
  document.Parse(source.data(), source.size());
  if (document.HasParseError() || !document.IsObject()) return std::nullopt;

  auto customDataIt = document.FindMember("customData");
  if (customDataIt == document.MemberEnd() || !customDataIt->value.IsObject()) return std::nullopt;
  auto& customData = customDataIt->value;

  static constexpr char kInjectedMarker[] = "NE_fakeObjectsInjected";
  auto markerIt = customData.FindMember(kInjectedMarker);
  if (markerIt != customData.MemberEnd() && markerIt->value.IsBool() && markerIt->value.GetBool()) {
    return std::nullopt;
  }

  auto& allocator = document.GetAllocator();
  int injected = 0;

  auto injectArray = [&](char const* fakeName, char const* normalName) {
    auto fakeIt = customData.FindMember(fakeName);
    if (fakeIt == customData.MemberEnd() || !fakeIt->value.IsArray() || fakeIt->value.Empty()) return;

    auto normalIt = document.FindMember(normalName);
    if (normalIt == document.MemberEnd()) {
      rapidjson::Value name(normalName, allocator);
      document.AddMember(name.Move(), rapidjson::Value(rapidjson::kArrayType).Move(), allocator);
      normalIt = document.FindMember(normalName);
    }
    if (!normalIt->value.IsArray()) return;

    for (auto const& sourceItem : fakeIt->value.GetArray()) {
      if (!sourceItem.IsObject()) continue;
      rapidjson::Value item(sourceItem, allocator);
      auto itemCustomDataIt = item.FindMember("customData");
      if (itemCustomDataIt == item.MemberEnd()) {
        item.AddMember(rapidjson::StringRef("customData"),
                       rapidjson::Value(rapidjson::kObjectType).Move(), allocator);
        itemCustomDataIt = item.FindMember("customData");
      }
      if (!itemCustomDataIt->value.IsObject()) itemCustomDataIt->value.SetObject();

      auto fakeMarkerIt = itemCustomDataIt->value.FindMember(
          NoodleExtensions::Constants::INTERNAL_FAKE_NOTE.data());
      if (fakeMarkerIt == itemCustomDataIt->value.MemberEnd()) {
        itemCustomDataIt->value.AddMember(
            rapidjson::StringRef(NoodleExtensions::Constants::INTERNAL_FAKE_NOTE.data()),
            rapidjson::Value(true).Move(), allocator);
      } else {
        fakeMarkerIt->value.SetBool(true);
      }

      normalIt->value.PushBack(item.Move(), allocator);
      injected++;
    }
  };

  injectArray("fakeColorNotes", "colorNotes");
  injectArray("fakeBombNotes", "bombNotes");
  injectArray("fakeObstacles", "obstacles");
  injectArray("fakeBurstSliders", "burstSliders");
  injectArray("fakeSliders", "sliders");

  if (injected == 0) return std::nullopt;

  markerIt = customData.FindMember(kInjectedMarker);
  if (markerIt == customData.MemberEnd()) {
    customData.AddMember(rapidjson::StringRef(kInjectedMarker),
                         rapidjson::Value(true).Move(), allocator);
  } else {
    markerIt->value.SetBool(true);
  }

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  document.Accept(writer);
  NELogger::Logger.info(
      "Injected {} V3 fake object(s) before CustomJSONData conversion", injected);
  return StringW(std::string_view(buffer.GetString(), buffer.GetSize()));
}
} // namespace

MAKE_HOOK_MATCH(V3_BeatmapDataLoader_GetBeatmapDataFromSaveDataJson,
                &BeatmapDataLoaderVersion3::BeatmapDataLoader::GetBeatmapDataFromSaveDataJson,
                GlobalNamespace::BeatmapData*, StringW beatmapSaveDataJson,
                StringW defaultLightshowJson, GlobalNamespace::BeatmapDifficulty beatmapDifficulty,
                float_t startBpm, bool loadingForDesignatedEnvironment,
                GlobalNamespace::IEnvironmentInfo* environmentInfo,
                GlobalNamespace::BeatmapLevelDataVersion beatmapLevelDataVersion,
                GlobalNamespace::PlayerSpecificSettings* playerSpecificSettings,
                GlobalNamespace::IBeatmapLightEventConverter* lightEventConverter) {
  auto injectedJson = InjectV3FakeObjectsIntoJson(beatmapSaveDataJson);
  return V3_BeatmapDataLoader_GetBeatmapDataFromSaveDataJson(
      injectedJson.value_or(beatmapSaveDataJson), defaultLightshowJson, beatmapDifficulty, startBpm,
      loadingForDesignatedEnvironment, environmentInfo, beatmapLevelDataVersion,
      playerSpecificSettings, lightEventConverter);
}

MAKE_HOOK_MATCH(V3_BeatmapDataLoader_GetBeatmapDataFromSaveData,
                &BeatmapDataLoaderVersion3::BeatmapDataLoader::GetBeatmapDataFromSaveData,
                GlobalNamespace::BeatmapData*, ::BeatmapSaveDataVersion3::BeatmapSaveData* beatmapSaveData,
                ::BeatmapSaveDataVersion4::LightshowSaveData* defaultLightshowSaveData,
                ::GlobalNamespace::BeatmapDifficulty beatmapDifficulty, float_t startBpm,
                bool loadingForDesignatedEnvironment, ::GlobalNamespace::EnvironmentKeywords* environmentKeywords,
                ::GlobalNamespace::IEnvironmentLightGroups* environmentLightGroups,
                ::GlobalNamespace::PlayerSpecificSettings* playerSpecificSettings,
                ::GlobalNamespace::IBeatmapLightEventConverter* lightEventConverter,
                ::System::Diagnostics::Stopwatch* stopwatch) {
  using namespace CustomJSONData::v3;

  // SongCore may parse/cache custom songs before Noodle's ParsedEvent callback
  // is registered. Inject the standardized V3 fake arrays here as well, before
  // CJD converts the save data, so cached songs get the same normal parse/sort
  // path as freshly parsed songs.
  auto* customSaveData =
      il2cpp_utils::try_cast<CustomJSONData::v3::CustomBeatmapSaveData>(beatmapSaveData).value_or(nullptr);
  bool const preInjectedBeforeLoad = FakeNoteHelper::EnsureV3FakeObjectsInjected(customSaveData);

  auto beatmap = V3_BeatmapDataLoader_GetBeatmapDataFromSaveData(
      beatmapSaveData, defaultLightshowSaveData, beatmapDifficulty, startBpm, loadingForDesignatedEnvironment,
      environmentKeywords, environmentLightGroups, playerSpecificSettings, lightEventConverter, stopwatch);

  auto customBeatmap = il2cpp_utils::try_cast<CustomBeatmapData>(beatmap).value_or(nullptr);
  if (!customBeatmap || !customBeatmap->customData || !customBeatmap->customData->value ||
      !customBeatmap->customData->value.value().get().IsObject()) {
    return beatmap;
  }

  rapidjson::Value const& customData = customBeatmap->customData->value.value();

  // Preferred path: the parser callback has already appended every V3 fake
  // object to the save-data lists, so CJD converted and sorted them together
  // with normal objects in the original call above. Re-adding the top-level
  // fake arrays here would duplicate the visual notes and their prefabs.
  auto injectedMarker = NEJSON::ReadOptionalBool(customData, "NE_fakeObjectsInjected");
  if (preInjectedBeforeLoad || injectedMarker.value_or(false)) {
    NELogger::Logger.info("V3 fake objects were loaded by the normal CJD parse path; late fallback skipped");
    return customBeatmap;
  }

  NELogger::Logger.warn("V3 fake objects were not pre-injected; using the compatibility fallback");

  if (!beatmapSaveData || !beatmapSaveData->bpmEvents) {
    NELogger::Logger.warn("V3 fake-object fallback skipped because source save data is unavailable");
    return customBeatmap;
  }

  ListW<BeatmapSaveDataVersion3::BpmChangeEventData*> bpmEvents = beatmapSaveData->bpmEvents;
  CustomJSONData::BpmTimeProcessor bpmTimeProcessor(startBpm, bpmEvents);

  auto const BeatToTime = [&bpmTimeProcessor](float beat) {
    auto time = bpmTimeProcessor.ConvertBeatToTime(beat);
    return time;
  };

#define PARSE_ARRAY(key, parse, convert)                                                                               \
  auto key##it = customData.FindMember(#key);                                                                          \
  if (key##it != customData.MemberEnd() && key##it->value.IsArray()) {                                                 \
    bpmTimeProcessor.Reset();                                                                                          \
    for (auto const& it : key##it->value.GetArray()) {                                                                 \
      if (!it.IsObject()) continue;                                                                                    \
      auto item = parse(it);                                                                                           \
      if (!item) continue;                                                                                             \
      auto obj = convert(item);                                                                                        \
      if (!obj) continue;                                                                                              \
      auto& ad = getAD(obj->customData);                                                                               \
      ad.objectData.fake = true;                                                                                       \
      customBeatmap->AddBeatmapObjectDataOverride(obj);                                                                \
    }                                                                                                                  \
  }

  PARSE_ARRAY(fakeColorNotes, Parser::DeserializeColorNote, [&](v3::CustomBeatmapSaveData_ColorNoteData* data) {
    auto rotation = 0;
    return CreateCustomBasicNoteData(BeatToTime(data->b), data->b, rotation, data->line,
                                     ConvertNoteLineLayer(data->layer), ConvertColorType(data->color),
                                     ConvertNoteCutDirection(data->cutDirection), data->customData);
  });
  PARSE_ARRAY(fakeBombNotes, Parser::DeserializeBombNote, [&](v3::CustomBeatmapSaveData_BombNoteData* data) {
    auto rotation = 0;

    return CreateCustomBombNoteData(BeatToTime(data->b), data->b, rotation, data->line,
                                    ConvertNoteLineLayer(data->layer), data->customData);
  });
  PARSE_ARRAY(fakeObstacles, Parser::DeserializeObstacle, [&](v3::CustomBeatmapSaveData_ObstacleData* data) {
    float beat = BeatToTime(data->b);
    auto rotation = 0;
    auto* obstacle =
        CustomObstacleData::New_ctor(beat, data->b, data->b + data->duration, rotation, data->get_line(), GetNoteLineLayer(data->get_layer()),
                                     BeatToTime(data->b + data->duration) - beat, data->width, data->height);

    obstacle->customData->Init(data->customData);

    return obstacle;
  });

  PARSE_ARRAY(
      fakeSliders, Parser::DeserializeSlider, [&](v3::CustomBeatmapSaveData_SliderData* data) -> CustomSliderData* {
        auto headRotation = 0;
        auto tailRotation = 0;
        return CustomSliderData_CreateCustomSliderData(
            ConvertColorType(data->get_colorType()), BeatToTime(data->b), data->b, headRotation, data->get_headLine(),
            ConvertNoteLineLayer(data->get_headLayer()), ConvertNoteLineLayer(data->get_headLayer()),
            data->get_headControlPointLengthMultiplier(), ConvertNoteCutDirection(data->get_headCutDirection()),
            BeatToTime(data->get_tailBeat()), tailRotation, data->get_tailLine(),
            ConvertNoteLineLayer(data->get_tailLayer()), ConvertNoteLineLayer(data->get_tailLayer()),
            data->get_tailControlPointLengthMultiplier(), ConvertNoteCutDirection(data->get_tailCutDirection()),
            ConvertSliderMidAnchorMode(data->get_sliderMidAnchorMode()), data->customData);
      });

  PARSE_ARRAY(fakeBurstSliders, Parser::DeserializeBurstSlider,
              [&](v3::CustomBeatmapSaveData_BurstSliderData* data) -> CustomSliderData* {
                auto headRotation = 0;
                auto tailRotation = 0;
                return CustomSliderData_CreateCustomBurstSliderData(
                    ConvertColorType(data->colorType), BeatToTime(data->beat), data->beat, headRotation, data->headLine,
                    ConvertNoteLineLayer(data->headLayer), ConvertNoteLineLayer(data->headLayer),
                    ConvertNoteCutDirection(data->headCutDirection), BeatToTime(data->tailBeat), data->tailBeat,
                    data->tailLine, ConvertNoteLineLayer(data->tailLayer), ConvertNoteLineLayer(data->tailLayer),
                    data->sliceCount, data->squishAmount, data->customData);
              });

  customBeatmap->ProcessRemainingData();
  customBeatmap->ProcessAndSortBeatmapData();

  return customBeatmap;
}

// Beat Saber 1.40.8 only exposes the V3 basic-info loader as
// GetBeatmapDataBasicInfoFromSaveDataJson(StringW). The older save-data hook
// used by this fork no longer exists in bs-cordl 4008.0.0, so the unsupported
// hook is intentionally omitted. Fake V3 objects are still loaded and marked
// by V3_BeatmapDataLoader_GetBeatmapDataFromSaveData above.

void HandleFakeV3Objects(v3::CustomBeatmapSaveData*) {}
void InstallBeatmapDataHooks(){
  // force CJD to be first, is this needed?
  // Modloader::requireMod("CustomJSONData");
  INSTALL_HOOK(NELogger::Logger, V2_BeatmapDataLoader_GetBeatmapDataBasicInfoFromSaveDataJson);
  INSTALL_HOOK(NELogger::Logger, V3_BeatmapDataLoader_GetBeatmapDataFromSaveDataJson);
  INSTALL_HOOK(NELogger::Logger, V3_BeatmapDataLoader_GetBeatmapDataFromSaveData);

  // v3::Parser::ParsedEvent.addCallback(HandleFakeV3Objects);
}

NEInstallHooks(InstallBeatmapDataHooks);
