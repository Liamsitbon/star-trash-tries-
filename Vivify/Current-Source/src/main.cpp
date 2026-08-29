#include "main.hpp"
#include "VivifyRuntime.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>
#include <fstream>
#include <mutex>
#include <filesystem>
#include "HMUI/ViewController.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/XR/XRSettings.hpp"
#include "bsml/shared/BSML-Lite/Creation/Layout.hpp"
#include "bsml/shared/BSML-Lite/Creation/Settings.hpp"
#include "bsml/shared/BSML/Settings/BSMLSettings.hpp"
#include "custom-types/shared/register.hpp"
#include "scotland2/shared/modloader.h"

static modloader::ModInfo modInfo{MOD_ID, VERSION, 0};

namespace {
constexpr std::string_view kEnableBlitEffectsConfigKey = "enableBlitEffects";
constexpr std::string_view kEnableBeat0FilmgrainConfigKey = "enableBeat0Filmgrain";
constexpr std::string_view kEnableSecondaryDepthCamerasConfigKey = "enableSecondaryDepthCameras";
constexpr std::string_view kEnablePrefabPrewarmingConfigKey = "enablePrefabPrewarming";
constexpr std::string_view kVivifyDebugLoggingConfigKey = "vivifyDebugLogging";

// The settings page exposes only individual features. Every feature is enabled
// by default and can be disabled independently.
bool gEnableBlitEffects = true;
bool gEnableBeat0Filmgrain = true;
bool gEnableSecondaryDepthCameras = true;
bool gEnablePrefabPrewarming = true;
bool gVivifyDebugLogging = false;

constexpr std::string_view kVivifyLogDir = "/sdcard/ModData/com.beatgames.beatsaber/Logs";
constexpr std::string_view kVivifyLogPath = "/sdcard/ModData/com.beatgames.beatsaber/Logs/Vivify.log";
std::ofstream gVivifyLogFile;
std::mutex gVivifyLogMutex;
bool gVivifyLogSinkInstalled = false;
std::size_t gVivifyLogLinesSinceFlush = 0;

void InstallVivifyFileLogSink() {
  if (gVivifyLogSinkInstalled) return;
  gVivifyLogSinkInstalled = true;
  std::error_code ec;
  std::filesystem::create_directories(std::string(kVivifyLogDir), ec);

  gVivifyLogFile.open(std::string(kVivifyLogPath), std::ios::out | std::ios::trunc);
  if (!gVivifyLogFile.is_open()) {
    PaperLogger.warn("Vivify: could not open log file at {} (logging to logcat only)", kVivifyLogPath);
    return;
  }
  gVivifyLogFile << "=== Vivify " << VERSION << " session log ===\n";
  gVivifyLogFile.flush();

  Paper::Logger::AddLogSink([](Paper::LogData const& data) {
    if (!data.tag.has_value() || *data.tag != std::string_view(MOD_ID)) return;
    std::lock_guard<std::mutex> lock(gVivifyLogMutex);
    if (!gVivifyLogFile.is_open()) return;
    gVivifyLogFile << '[' << Paper::format_as(data.level) << "] " << data.message << '\n';
    gVivifyLogLinesSinceFlush++;
    bool const urgent = data.level == Paper::LogLevel::WRN ||
                        data.level == Paper::LogLevel::ERR ||
                        data.level == Paper::LogLevel::CRIT;
    // Synchronous flash I/O after every informational event can create visible
    // frame spikes on Quest. Preserve crash evidence by flushing warnings and
    // errors immediately, while batching ordinary lines into small groups.
    if (urgent || gVivifyLogLinesSinceFlush >= 32) {
      gVivifyLogFile.flush();
      gVivifyLogLinesSinceFlush = 0;
    }
  });
}

void EnsureConfigObject() {
  auto& doc = getConfig().config;
  if (!doc.IsObject()) {
    doc.SetObject();
  }
}

bool EnsureBoolConfigValue(std::string_view key, bool defaultValue, bool& value) {
  auto& doc = getConfig().config;
  auto it = doc.FindMember(key.data());
  if (it != doc.MemberEnd() && it->value.IsBool()) {
    value = it->value.GetBool();
    return false;
  }

  auto& allocator = doc.GetAllocator();
  value = defaultValue;
  if (it == doc.MemberEnd()) {
    doc.AddMember(rapidjson::Value(key.data(), allocator), rapidjson::Value(defaultValue), allocator);
  } else {
    it->value.SetBool(defaultValue);
  }
  return true;
}

void SetBoolConfigValue(std::string_view key, bool enabled, bool& value) {
  auto& config = getConfig();
  auto& doc = config.config;
  EnsureConfigObject();
  auto& allocator = doc.GetAllocator();
  auto it = doc.FindMember(key.data());
  if (it == doc.MemberEnd()) {
    doc.AddMember(rapidjson::Value(key.data(), allocator), rapidjson::Value(enabled), allocator);
  } else {
    it->value.SetBool(enabled);
  }
  value = enabled;
  config.Write();
}

void RegisterModSettings() {
  BSML::BSMLSettings::get_instance()->TryAddSettingsMenu(
      [](HMUI::ViewController* viewController, bool firstActivation, bool, bool) {
        if (!firstActivation || viewController == nullptr) return;
        auto* container = BSML::Lite::CreateScrollableSettingsContainer(viewController->get_transform());
        if (container == nullptr) return;

        BSML::Lite::CreateToggle(
            container->get_transform(), u"Blit effects", gEnableBlitEffects,
            [](bool value) {
              SetBoolConfigValue(kEnableBlitEffectsConfigKey, value, gEnableBlitEffects);
              Vivify::RefreshIsolationSettings();
            });
        BSML::Lite::CreateToggle(
            container->get_transform(), u"Beat-0 film grain", gEnableBeat0Filmgrain,
            [](bool value) {
              SetBoolConfigValue(kEnableBeat0FilmgrainConfigKey, value, gEnableBeat0Filmgrain);
              Vivify::RefreshIsolationSettings();
            });
        BSML::Lite::CreateToggle(
            container->get_transform(), u"Secondary/depth cameras", gEnableSecondaryDepthCameras,
            [](bool value) {
              SetBoolConfigValue(kEnableSecondaryDepthCamerasConfigKey, value,
                                 gEnableSecondaryDepthCameras);
              Vivify::RefreshIsolationSettings();
            });
        BSML::Lite::CreateToggle(
            container->get_transform(), u"Prefab prewarming", gEnablePrefabPrewarming,
            [](bool value) {
              SetBoolConfigValue(kEnablePrefabPrewarmingConfigKey, value,
                                 gEnablePrefabPrewarming);
            });
      },
      "Vivify", false);
}
}

Configuration &getConfig() {
  static Configuration config(modInfo);
  return config;
}

bool GetMultipassRenderingEnabled() {
  // MULTIPASS_ENABLED selects the two-render-pass shader variant. Quest's
  // SinglePassMultiview mode is value 3 and must use the texture-array shader
  // variant instead; forcing this keyword there makes the two eyes sample the
  // wrong layout and produces rows, duplicates and divergent eye output.
  return UnityEngine::XR::XRSettings::get_enabled() &&
         UnityEngine::XR::XRSettings::get_stereoRenderingMode().value__ == 0;
}

bool IsQuestXrRuntime() {
  return UnityEngine::XR::XRSettings::get_enabled();
}

bool GetVivifyDebugLogging() {
  return gVivifyDebugLogging;
}

bool GetDisableBeat0FilmgrainBlit() {
  return !gEnableBeat0Filmgrain;
}

bool GetDisableAllBlits() {
  return !gEnableBlitEffects;
}

bool GetDisableCreateCameraDepth() {
  return !gEnableSecondaryDepthCameras;
}

bool GetQuestCompatibilityMode() {
  return false;
}

bool GetStaggerPrefabPrewarm() {
  return gEnablePrefabPrewarming;
}

float GetImmediatePrefabPrewarmSeconds() {
  return 12.0f;
}

float GetPrefabPrewarmLeadSeconds() {
  return 18.0f;
}

int GetPrefabsPerFrame() {
  return 1;
}

void EnsureConfigDefaults() {
  auto& config = getConfig();
  auto& doc = config.config;
  EnsureConfigObject();

  // Remove every old profile, safety, recovery, tuning and negative-disable key.
  constexpr std::array<std::string_view, 14> obsoleteKeys{
      "multipassRendering", "stereoSafetyMode", "disableBeat0FilmgrainBlit",
      "disableAllBlits", "disableCreateCameraDepth", "questCompatibilityMode",
      "staggerPrefabPrewarm", "questRenderProfile", "immediatePrefabPrewarmSeconds",
      "prefabPrewarmLeadSeconds", "prefabsPerFrame", "fullEffectsBaseline0_5_8",
      "questMultiviewBaseline0_5_9", "enableMultiviewSafety"};
  bool needsWrite = false;
  for (auto key : obsoleteKeys) {
    auto it = doc.FindMember(key.data());
    if (it != doc.MemberEnd()) {
      doc.RemoveMember(it);
      needsWrite = true;
    }
  }

  needsWrite |= EnsureBoolConfigValue(kEnableBlitEffectsConfigKey, true, gEnableBlitEffects);
  needsWrite |= EnsureBoolConfigValue(kEnableBeat0FilmgrainConfigKey, true, gEnableBeat0Filmgrain);
  needsWrite |= EnsureBoolConfigValue(kEnableSecondaryDepthCamerasConfigKey, true,
                                      gEnableSecondaryDepthCameras);
  needsWrite |= EnsureBoolConfigValue(kEnablePrefabPrewarmingConfigKey, true,
                                      gEnablePrefabPrewarming);
  needsWrite |= EnsureBoolConfigValue(kVivifyDebugLoggingConfigKey, false, gVivifyDebugLogging);

  if (needsWrite) config.Write();
}

MOD_EXTERN_FUNC void setup(CModInfo *info) noexcept {
  *info = modInfo.to_c();
  InstallVivifyFileLogSink();
  getConfig().Load();
  EnsureConfigDefaults();
  PaperLogger.info("Vivify file logging active -> {} blits={} filmgrain={} cameras={} prewarm={}",
                   kVivifyLogPath, gEnableBlitEffects, gEnableBeat0Filmgrain,
                   gEnableSecondaryDepthCameras, gEnablePrefabPrewarming);
}
MOD_EXTERN_FUNC void late_load() noexcept {
  il2cpp_functions::Init();
  custom_types::Register::AutoRegister();
  RegisterModSettings();
  Vivify::LateLoad();
}
