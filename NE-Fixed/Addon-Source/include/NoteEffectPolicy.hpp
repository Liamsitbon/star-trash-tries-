// SPDX-License-Identifier: MIT
// Derived from the legacy fork's standalone note-effect classifier.
#pragma once

#ifndef RAPIDJSON_HAS_STDSTRING
#define RAPIDJSON_HAS_STDSTRING 1
#endif
#include "beatsaber-hook/shared/rapidjson/include/rapidjson/document.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace NEFixed::NoteEffects {

inline rapidjson::Value const* Field(rapidjson::Value const& value, char const* key) {
  if (!value.IsObject()) return nullptr;
  auto it = value.FindMember(key);
  return it != value.MemberEnd() && !it->value.IsNull() ? &it->value : nullptr;
}

inline rapidjson::Value const* Field(rapidjson::Value const& value, char const* key,
                                     char const* legacyKey) {
  auto* result = Field(value, key);
  return result ? result : Field(value, legacyKey);
}

template<class F> void EachTrack(rapidjson::Value const* value, F&& callback) {
  if (!value) return;
  auto visit = [&](rapidjson::Value const& track) {
    if (track.IsString() && track.GetStringLength() != 0)
      callback(std::string(track.GetString(), track.GetStringLength()));
  };
  if (value->IsArray()) {
    for (auto const& track : value->GetArray()) visit(track);
  } else {
    visit(*value);
  }
}

inline bool HasNoteAnimation(rapidjson::Value const& data) {
  // Static Chroma color alone is not a modchart. These properties change the
  // geometry/visibility that cosmetic note renderers must follow.
  for (char const* key : {"dissolve", "_dissolve", "dissolveArrow", "_dissolveArrow",
                         "position", "_position", "offsetPosition", "definitePosition",
                         "_definitePosition", "rotation", "_rotation", "offsetWorldRotation",
                         "localRotation", "_localRotation", "scale", "_scale",
                         "interactable", "_interactable"}) {
    if (Field(data, key)) return true;
  }
  return false;
}

// Collected once from the actual difficulty being installed, never from a
// menu's asynchronous preview/preload or a map-name allowlist.
class Policy {
 public:
  // Official NE 1.6.3's late injector can record fake state in private AD
  // without adding NE_fake to the visible per-note JSON. Read, do not mutate,
  // the map's raw arrays so those notes still get prefab compatibility.
  void AddMapCustomData(rapidjson::Value const& data) {
    for (auto key : {"fakeColorNotes", "fakeBombNotes"}) {
      auto* array = Field(data, key);
      if (!array || !array->IsArray()) continue;
      for (auto const& item : array->GetArray()) {
        if (!item.IsObject()) continue;
        directEffect = true;
        if (auto* custom = Field(item, "customData", "_customData")) AddNote(*custom);
      }
    }
  }

  void AddNote(rapidjson::Value const& data) {
    EachTrack(Field(data, "track", "_track"), [&](std::string const& track) {
      noteTracks.insert(track);
    });
    if (auto* animation = Field(data, "animation", "_animation");
        animation && HasNoteAnimation(*animation)) directEffect = true;
    for (char const* key : {"fake", "_fake", "NE_fake"}) {
      if (auto* value = Field(data, key); value && value->IsBool() && value->GetBool())
        directEffect = true;
    }
  }

  void AddEvent(std::string_view type, rapidjson::Value const& data) {
    if (type == "AssignObjectPrefab") {
      // Only note assignments count; custom sabers or an environment alone do
      // not disable the player's cosmetic notes. Vivify's own assignments are
      // applied later and are not intercepted by the prefab redecorator.
      if (Field(data, "colorNotes") || Field(data, "bombNotes")) directEffect = true;
    } else if (type == "AssignTrackParent") {
      auto* parent = Field(data, "parentTrack", "_parentTrack");
      if (!parent || !parent->IsString()) return;
      std::string parentName(parent->GetString(), parent->GetStringLength());
      EachTrack(Field(data, "childrenTracks", "_childrenTracks"), [&](std::string const& child) {
        parents[child].push_back(parentName);
      });
    } else if ((type == "AnimateTrack" || type == "AssignPathAnimation") && HasNoteAnimation(data)) {
      EachTrack(Field(data, "track", "_track"), [&](std::string const& track) {
        animatedTracks.insert(track);
      });
    }
  }

  bool RequiresDefaultNotes() const {
    if (directEffect) return true;
    auto relevantTracks = noteTracks;
    std::vector<std::string> pending(noteTracks.begin(), noteTracks.end());
    // A visited set also bounds malformed/cyclic parent graphs.
    for (size_t i = 0; i < pending.size(); ++i) {
      auto const track = pending[i];
      if (animatedTracks.contains(track)) return true;
      if (auto it = parents.find(track); it != parents.end()) {
        for (auto const& parent : it->second) {
          if (relevantTracks.insert(parent).second) pending.push_back(parent);
        }
      }
    }
    return false;
  }

 private:
  bool directEffect = false;
  std::unordered_set<std::string> noteTracks;
  std::unordered_set<std::string> animatedTracks;
  std::unordered_map<std::string, std::vector<std::string>> parents;
};

}  // namespace NEFixed::NoteEffects
