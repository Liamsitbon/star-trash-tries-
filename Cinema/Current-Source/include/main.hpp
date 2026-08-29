#pragma once

#include "_config.hpp"
#include "beatsaber-hook/shared/config/config-utils.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-functions.hpp"
#include "paper2_scotland2/shared/logger.hpp"

Configuration& getConfig();
bool GetCinemaEnabled();
void SetCinemaEnabled(bool enabled);

constexpr auto PaperLogger = Paper::ConstLoggerContext("Cinema");
