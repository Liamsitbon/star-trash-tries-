// SPDX-License-Identifier: MIT
#pragma once
#include <string_view>

namespace NEFixed {
inline constexpr std::string_view kBaseId = "NoodleExtensions";
inline constexpr std::string_view kBaseVersion = "1.6.3";
enum class BaseStatus { Ready, MissingOrFailed, Unsupported };

constexpr BaseStatus CheckBase(bool loaded, std::string_view id, std::string_view version) {
  if (!loaded) return BaseStatus::MissingOrFailed;
  return id == kBaseId && version == kBaseVersion ? BaseStatus::Ready : BaseStatus::Unsupported;
}

constexpr bool ShouldApply(BaseStatus base, bool enabled, bool defaultNotes, bool noteEffects) {
  return base == BaseStatus::Ready && enabled && defaultNotes && noteEffects;
}
}  // namespace NEFixed
