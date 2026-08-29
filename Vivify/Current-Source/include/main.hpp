#pragma once
#include <string_view>
#include "scotland2/shared/modloader.h"
#include "beatsaber-hook/shared/config/config-utils.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-functions.hpp"
#include "beatsaber-hook/shared/utils/logging.hpp"
#include "paper2_scotland2/shared/logger.hpp"
#include "_config.hpp"
Configuration &getConfig();
bool GetMultipassRenderingEnabled();
bool IsQuestXrRuntime();
bool GetVivifyDebugLogging();
bool GetDisableBeat0FilmgrainBlit();
bool GetDisableAllBlits();
bool GetDisableCreateCameraDepth();
bool GetQuestCompatibilityMode();
bool GetStaggerPrefabPrewarm();
float GetImmediatePrefabPrewarmSeconds();
float GetPrefabPrewarmLeadSeconds();
int GetPrefabsPerFrame();
void EnsureConfigDefaults();
constexpr auto PaperLogger = Paper::ConstLoggerContext("Vivify");

#define VIVIFY_DEBUG(...)                                          \
  do {                                                             \
    if (GetVivifyDebugLogging()) PaperLogger.info(__VA_ARGS__);    \
  } while (false)
