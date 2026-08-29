#include "main.hpp"

#include "CinemaRuntime.hpp"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>

#include "HMUI/ViewController.hpp"
#include "bsml/shared/BSML-Lite/Creation/Layout.hpp"
#include "bsml/shared/BSML-Lite/Creation/Settings.hpp"
#include "bsml/shared/BSML.hpp"
#include "custom-types/shared/register.hpp"
#include "scotland2/shared/modloader.h"

static modloader::ModInfo modInfo{MOD_ID, VERSION, 0};

namespace {
bool gEnabled = false;
std::ofstream gLogFile;
std::mutex gLogMutex;
bool gLogSinkInstalled = false;
std::size_t gLinesSinceFlush = 0;

constexpr std::string_view kLogDirectory =
    "/sdcard/ModData/com.beatgames.beatsaber/Logs";
constexpr std::string_view kLogPath =
    "/sdcard/ModData/com.beatgames.beatsaber/Logs/Cinema.log";

void EnsureConfigDefaults() {
  auto& configuration = getConfig();
  auto& document = configuration.config;
  if (!document.IsObject()) document.SetObject();
  auto& allocator = document.GetAllocator();
  auto enabled = document.FindMember("enabled");
  auto safetyReset = document.FindMember("safetyReset_0_1_1");

  // Cinema 0.1.0 could run teardown code inside every Unity scene transition.
  // Reset old configurations once so installing this recovery build cannot
  // immediately re-enable video playback before the user opts in again.
  bool const mustApplySafetyReset =
      safetyReset == document.MemberEnd() || !safetyReset->value.IsBool() ||
      !safetyReset->value.GetBool();
  if (mustApplySafetyReset) {
    if (enabled == document.MemberEnd()) {
      document.AddMember("enabled", false, allocator);
    } else {
      enabled->value.SetBool(false);
    }
    if (safetyReset == document.MemberEnd()) {
      document.AddMember("safetyReset_0_1_1", true, allocator);
    } else {
      safetyReset->value.SetBool(true);
    }
    gEnabled = false;
    configuration.Write();
    PaperLogger.warn(
        "Cinema 0.1.1 safety reset applied; enable it explicitly from Mods > Cinema after confirming normal maps are stable");
    return;
  }

  if (enabled != document.MemberEnd() && enabled->value.IsBool()) {
    gEnabled = enabled->value.GetBool();
    return;
  }
  if (enabled == document.MemberEnd()) {
    document.AddMember("enabled", false, allocator);
  } else {
    enabled->value.SetBool(false);
  }
  gEnabled = false;
  configuration.Write();
}

void InstallFileLogSink() {
  if (gLogSinkInstalled) return;
  gLogSinkInstalled = true;
  std::error_code error;
  std::filesystem::create_directories(std::string(kLogDirectory), error);
  gLogFile.open(std::string(kLogPath), std::ios::out | std::ios::trunc);
  if (!gLogFile.is_open()) {
    PaperLogger.warn("Cinema could not open {} (logcat remains active)", kLogPath);
    return;
  }
  gLogFile << "=== Cinema " << VERSION << " session log ===\n";
  gLogFile.flush();
  Paper::Logger::AddLogSink([](Paper::LogData const& data) {
    if (!data.tag.has_value() || *data.tag != std::string_view("Cinema")) return;
    std::lock_guard<std::mutex> lock(gLogMutex);
    if (!gLogFile.is_open()) return;
    gLogFile << '[' << Paper::format_as(data.level) << "] " << data.message << '\n';
    ++gLinesSinceFlush;
    bool const urgent = data.level == Paper::LogLevel::WRN ||
                        data.level == Paper::LogLevel::ERR ||
                        data.level == Paper::LogLevel::CRIT;
    if (urgent || gLinesSinceFlush >= 24) {
      gLogFile.flush();
      gLinesSinceFlush = 0;
    }
  });
}

void RegisterSettingsMenu() {
  bool const registered = BSML::Register::RegisterSettingsMenu(
      "Cinema",
      [](HMUI::ViewController* viewController, bool firstActivation, bool, bool) {
        if (!firstActivation || viewController == nullptr) return;
        auto* container =
            BSML::Lite::CreateScrollableSettingsContainer(viewController->get_transform());
        if (container == nullptr) return;
        BSML::Lite::CreateToggle(
            container->get_transform(), u"Enabled", GetCinemaEnabled(),
            [](bool value) { SetCinemaEnabled(value); });
      },
      false);
  PaperLogger.info("Cinema Mods settings menu registration {}",
                   registered ? "succeeded" : "was deferred/rejected by BSML");
}
}  // namespace

Configuration& getConfig() {
  static Configuration configuration(modInfo);
  return configuration;
}

bool GetCinemaEnabled() { return gEnabled; }

void SetCinemaEnabled(bool enabled) {
  auto& configuration = getConfig();
  auto& document = configuration.config;
  if (!document.IsObject()) document.SetObject();
  auto iterator = document.FindMember("enabled");
  if (iterator == document.MemberEnd()) {
    document.AddMember("enabled", enabled, document.GetAllocator());
  } else {
    iterator->value.SetBool(enabled);
  }
  gEnabled = enabled;
  configuration.Write();
  CinemaQuest::Runtime::Instance().SetEnabled(enabled);
  if (enabled) CinemaQuest::InstallHooks();
  PaperLogger.info("Cinema enabled={}", enabled);
}

MOD_EXTERN_FUNC void setup(CModInfo* info) noexcept {
  *info = modInfo.to_c();
  getConfig().Load();
  EnsureConfigDefaults();
  InstallFileLogSink();
  PaperLogger.info("Cinema {} setup for Beat Saber 1.40.8_7379 enabled={}",
                   VERSION, gEnabled);
}

MOD_EXTERN_FUNC void late_load() noexcept {
  il2cpp_functions::Init();
  custom_types::Register::AutoRegister();
  RegisterSettingsMenu();
  CinemaQuest::Runtime::Instance().LateLoad();
  if (gEnabled) {
    CinemaQuest::InstallHooks();
  } else {
    PaperLogger.info(
        "Cinema hooks remain uninstalled until the Mods > Cinema toggle is enabled");
  }
}
