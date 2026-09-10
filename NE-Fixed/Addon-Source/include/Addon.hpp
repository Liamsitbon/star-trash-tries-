// SPDX-License-Identifier: MIT
#pragma once
#include "AddonPolicy.hpp"
#include "paper2_scotland2/shared/logger.hpp"
#include "scotland2/shared/loader.hpp"
#include <string>

namespace NEFixed {
inline constexpr auto Logger = Paper::ConstLoggerContext("NEFixed");
modloader::ModInfo const& Info();
BaseStatus GetBaseStatus();
bool Enabled();
bool DefaultNotes();
bool CutoutPrecision();
void SetLastScene(std::string message);
void InstallNoteCompatibility();
void InstallCutoutPrecision();
}  // namespace NEFixed
