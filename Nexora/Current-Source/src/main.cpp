#include "main.hpp"
#include "NexoraRuntime.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>

#include "custom-types/shared/register.hpp"

static modloader::ModInfo modInfo{MOD_ID, VERSION, 0};

namespace {
bool gEnabled = true;
bool gFileLogging = true;
bool gDebugLogging = false;
bool gCameraEffects = true;
int gMaxLayers = 3;
float gSyncTolerance = 0.085f;
float gPrepareTimeout = 18.0f;
int gDomeResolution = 48;
bool gForceUnlitFallback = false;

constexpr std::string_view kNexoraLogDir = "/sdcard/ModData/com.beatgames.beatsaber/Logs";
constexpr std::string_view kNexoraLogPath = "/sdcard/ModData/com.beatgames.beatsaber/Logs/Nexora.log";
std::ofstream gNexoraLogFile;
std::mutex gNexoraLogMutex;
bool gNexoraLogSinkInstalled = false;
std::size_t gNexoraLogLinesSinceFlush = 0;

void EnsureObject() {
  auto& document = getConfig().config;
  if (!document.IsObject()) document.SetObject();
}

bool EnsureBool(char const* key, bool defaultValue, bool& target) {
  auto& document = getConfig().config;
  auto iterator = document.FindMember(key);
  if (iterator != document.MemberEnd() && iterator->value.IsBool()) {
    target = iterator->value.GetBool();
    return false;
  }
  target = defaultValue;
  if (iterator == document.MemberEnd()) {
    document.AddMember(rapidjson::Value(key, document.GetAllocator()), rapidjson::Value(defaultValue),
                       document.GetAllocator());
  } else {
    iterator->value.SetBool(defaultValue);
  }
  return true;
}

bool EnsureInt(char const* key, int defaultValue, int minimum, int maximum, int& target) {
  auto& document = getConfig().config;
  auto iterator = document.FindMember(key);
  if (iterator != document.MemberEnd() && iterator->value.IsInt()) {
    target = std::clamp(iterator->value.GetInt(), minimum, maximum);
    if (target == iterator->value.GetInt()) return false;
    iterator->value.SetInt(target);
    return true;
  }
  target = defaultValue;
  if (iterator == document.MemberEnd()) {
    document.AddMember(rapidjson::Value(key, document.GetAllocator()), rapidjson::Value(target),
                       document.GetAllocator());
  } else {
    iterator->value.SetInt(target);
  }
  return true;
}

bool EnsureFloat(char const* key, float defaultValue, float minimum, float maximum,
                 float& target) {
  auto& document = getConfig().config;
  auto iterator = document.FindMember(key);
  if (iterator != document.MemberEnd() && iterator->value.IsNumber()) {
    target = std::clamp(iterator->value.GetFloat(), minimum, maximum);
    if (target == iterator->value.GetFloat()) return false;
    iterator->value.SetFloat(target);
    return true;
  }
  target = defaultValue;
  if (iterator == document.MemberEnd()) {
    document.AddMember(rapidjson::Value(key, document.GetAllocator()), rapidjson::Value(target),
                       document.GetAllocator());
  } else {
    iterator->value.SetFloat(target);
  }
  return true;
}
}  // namespace

Configuration& getConfig() {
  static Configuration configuration(modInfo);
  return configuration;
}

void InstallNexoraFileLogSink() {
  if (gNexoraLogSinkInstalled || !gFileLogging) return;
  gNexoraLogSinkInstalled = true;
  std::error_code ec;
  std::filesystem::create_directories(std::string(kNexoraLogDir), ec);

  gNexoraLogFile.open(std::string(kNexoraLogPath), std::ios::out | std::ios::trunc);
  if (!gNexoraLogFile.is_open()) {
    PaperLogger.warn("Nexora: could not open log file at {} (logging to logcat only)", kNexoraLogPath);
    return;
  }
  gNexoraLogFile << "=== Nexora " << VERSION << " session log ===\n";
  gNexoraLogFile << "[INFO] File logging active -> " << kNexoraLogPath
                 << " enabled=" << (gEnabled ? "true" : "false")
                 << " cameraEffects=" << (gCameraEffects ? "true" : "false")
                 << " maxLayers=" << gMaxLayers
                 << " syncTol=" << gSyncTolerance
                 << " domeRes=" << gDomeResolution
                 << " unlitFallback=" << (gForceUnlitFallback ? "true" : "false")
                 << "\n";
  gNexoraLogFile.flush();

  Paper::Logger::AddLogSink([](Paper::LogData const& data) {
    if (!data.tag.has_value() || *data.tag != std::string_view("Nexora")) return;
    std::lock_guard<std::mutex> lock(gNexoraLogMutex);
    if (!gNexoraLogFile.is_open()) return;
    gNexoraLogFile << '[' << Paper::format_as(data.level) << "] " << data.message << '\n';
    gNexoraLogFile.flush();
  });
}

bool GetNexoraEnabled() { return gEnabled; }
bool GetFileLoggingEnabled() { return gFileLogging; }
bool GetDebugLoggingEnabled() { return gDebugLogging; }
bool GetCameraEffectsEnabled() { return gCameraEffects; }
int GetMaxLayers() { return gMaxLayers; }
float GetSyncToleranceSeconds() { return gSyncTolerance; }
float GetPrepareTimeoutSeconds() { return gPrepareTimeout; }
int GetDomeResolution() { return gDomeResolution; }
bool GetForceUnlitFallback() { return gForceUnlitFallback; }

void EnsureConfigDefaults() {
  auto& configuration = getConfig();
  EnsureObject();
  bool changed = false;
  changed |= EnsureBool("enabled", true, gEnabled);
  changed |= EnsureBool("fileLogging", true, gFileLogging);
  changed |= EnsureBool("debugLogging", false, gDebugLogging);
  changed |= EnsureBool("cameraEffects", true, gCameraEffects);
  changed |= EnsureInt("maxLayers", 3, 1, 6, gMaxLayers);
  changed |= EnsureFloat("syncToleranceSeconds", 0.085f, 0.02f, 0.5f, gSyncTolerance);
  changed |= EnsureFloat("prepareTimeoutSeconds", 18.0f, 5.0f, 60.0f, gPrepareTimeout);
  changed |= EnsureInt("domeResolution", 48, 16, 96, gDomeResolution);
  changed |= EnsureBool("forceUnlitFallback", false, gForceUnlitFallback);
  if (changed) configuration.Write();
}

MOD_EXTERN_FUNC void setup(CModInfo* info) noexcept {
  *info = modInfo.to_c();
  getConfig().Load();
  EnsureConfigDefaults();
  InstallNexoraFileLogSink();
  PaperLogger.info(
      "Nexora {} setup: enabled={} fileLogging={} debugLogging={} maxLayers={} cameraEffects={} syncTolerance={} domeRes={}",
      VERSION, gEnabled, gFileLogging, gDebugLogging, gMaxLayers, gCameraEffects, gSyncTolerance, gDomeResolution);
}

MOD_EXTERN_FUNC void late_load() noexcept {
  il2cpp_functions::Init();
  custom_types::Register::AutoRegister();
  Nexora::LateLoad();
}

