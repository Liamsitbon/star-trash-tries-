#pragma once

#include <cstdint>
#include <string_view>
#include "beatsaber-hook/shared/config/config-utils.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-functions.hpp"
#include "paper2_scotland2/shared/logger.hpp"
#include "scotland2/shared/modloader.h"
#include "_config.hpp"

Configuration& getConfig();
void EnsureConfigDefaults();
void InstallNexoraFileLogSink();

bool GetNexoraEnabled();
bool GetFileLoggingEnabled();
bool GetDebugLoggingEnabled();
bool GetCameraEffectsEnabled();
int GetMaxLayers();
float GetSyncToleranceSeconds();
float GetPrepareTimeoutSeconds();
int GetDomeResolution();

constexpr auto PaperLogger = Paper::ConstLoggerContext("Nexora");
