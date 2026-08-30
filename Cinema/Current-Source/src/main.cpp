#include "main.hpp"

#include "CinemaRuntime.hpp"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>

#include "HMUI/ViewController.hpp"
#include "bsml/shared/BSML-Lite/Creation/Layout.hpp"
#include "bsml/shared/BSML-Lite/Creation/Settings.hpp"
#include "bsml/shared/BSML-Lite/Creation/Text.hpp"
#include "bsml/shared/BSML.hpp"
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
  auto safetyReset = document.FindMember("safetyReset_0_2_0");

  // Cinema 0.1.x could register and initialize runtime work during Scotland2's
  // first Unity destruction callback. Reset once so 0.2.0 always gets a clean,
  // menu-only first boot before the user opts in.
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
      document.AddMember("safetyReset_0_2_0", true, allocator);
    } else {
      safetyReset->value.SetBool(true);
    }
    gEnabled = false;
    configuration.Write();
    PaperLogger.warn(
        "Cinema 0.2.0 safety reset applied; first boot is menu-only until Cinema is enabled explicitly");
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
    // Startup diagnostics must survive an immediate native crash. Cinema's
    // log volume is low enough that flushing every line is the safer tradeoff.
    gLogFile.flush();
    gLinesSinceFlush = 0;
  });
}

void PopulateMenu(UnityEngine::Transform* parent) {
  if (parent == nullptr) return;
  auto* container = BSML::Lite::CreateScrollableSettingsContainer(parent);
  if (container == nullptr) return;
  auto transform = container->get_transform();
  BSML::Lite::CreateText(
      transform,
      u"Quest standalone: local map videos only. Re-select the map after enabling.",
      3.2f);
  BSML::Lite::CreateToggle(
      transform, u"Enabled", GetCinemaEnabled(),
      [](bool value) { SetCinemaEnabled(value); });
}

void RegisterMenus() {
  bool const registered = BSML::Register::RegisterSettingsMenu(
      "Cinema",
      [](HMUI::ViewController* viewController, bool firstActivation, bool, bool) {
        if (!firstActivation || viewController == nullptr) return;
        PopulateMenu(viewController->get_transform());
      },
      false);
  bool const gameplayRegistered = BSML::Register::RegisterGameplaySetupTab(
      "Cinema",
      [](UnityEngine::GameObject* root, bool firstActivation) {
        if (!firstActivation || root == nullptr) return;
        PopulateMenu(root->get_transform());
      },
      BSML::MenuType::All);
  BSML::Register::RegisterMainMenu(
      "Cinema", "Cinema", "Cinema Quest local-video settings",
      [](HMUI::ViewController* viewController, bool firstActivation, bool, bool) {
        if (!firstActivation || viewController == nullptr) return;
        PopulateMenu(viewController->get_transform());
      });
  PaperLogger.info("Cinema Mods settings menu registration {}",
                   registered ? "succeeded" : "was deferred/rejected by BSML");
  PaperLogger.info("Cinema Gameplay Setup tab registration {}",
                   gameplayRegistered ? "succeeded" : "was deferred/rejected by BSML");
  PaperLogger.info("Cinema main-menu button registration requested");
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
  if (enabled) {
    CinemaQuest::InstallHooks();
    CinemaQuest::Runtime::Instance().LateLoad();
  }
  CinemaQuest::Runtime::Instance().SetEnabled(enabled);
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
  PaperLogger.info("Cinema late_load entered; no Unity runtime objects are created here");
  RegisterMenus();
  if (gEnabled) {
    CinemaQuest::InstallHooks();
    CinemaQuest::Runtime::Instance().LateLoad();
  } else {
    PaperLogger.info(
        "Cinema disabled startup complete: no hooks, SongCore callbacks, AssetBundle, RenderTexture, VideoPlayer or custom type registration");
  }
}
