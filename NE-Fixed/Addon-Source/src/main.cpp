// SPDX-License-Identifier: MIT
#include "Addon.hpp"
#include "beatsaber-hook/shared/config/config-utils.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-functions.hpp"
#include "beatsaber-hook/shared/utils/typedefs-wrappers.hpp"
#include "bsml/shared/BSML-Lite.hpp"
#include "bsml/shared/BSML/Settings/BSMLSettings.hpp"
#include "HMUI/ViewController.hpp"
#include "TMPro/TextMeshProUGUI.hpp"
#include <exception>

namespace NEFixed {
namespace {
const modloader::ModInfo modInfo{"ne-fixed", VERSION, 0};
BaseStatus baseStatus = BaseStatus::MissingOrFailed;
bool enabled = true;
bool defaultNotes = true;
bool cutoutPrecision = true;
bool registered = false;
std::string baseDescription = "Base NE has not been checked";
std::string lastScene = "No gameplay scene inspected yet";
SafePtrUnity<HMUI::CurvedTextMeshPro> statusText;

Configuration& Config() {
  static Configuration config(modInfo);
  return config;
}

bool ReadBool(char const* key, bool fallback) {
  auto const& doc = Config().config;
  if (!doc.IsObject()) return fallback;
  auto it = doc.FindMember(key);
  return it != doc.MemberEnd() && it->value.IsBool() ? it->value.GetBool() : fallback;
}

void Save() {
  auto& doc = Config().config;
  if (!doc.IsObject()) doc.SetObject();
  for (auto const& entry : {std::pair{"enabled", enabled}, std::pair{"defaultNotesForNoteEffects", defaultNotes},
                           std::pair{"cutoutPrecision", cutoutPrecision}}) {
    auto it = doc.FindMember(entry.first);
    if (it == doc.MemberEnd())
      doc.AddMember(rapidjson::Value(entry.first, doc.GetAllocator()), rapidjson::Value(entry.second), doc.GetAllocator());
    else it->value.SetBool(entry.second);
  }
  Config().Write();
}

void Menu(HMUI::ViewController* view, bool first, bool, bool) {
  if (!view) return;
  auto refresh = [] {
    if (statusText.isAlive()) statusText->set_text(StringW(baseDescription +
      "\nIndependent add-on; normal NE owns mapping, fake notes and scoring."
      "\nCandidate: note prefab compatibility + cutout precision, not all legacy fixes."
      "\nChanges apply to the next gameplay scene.\nLast scene: " + lastScene));
  };
  if (!first) { refresh(); return; }
  auto parent = BSML::Lite::CreateScrollableSettingsContainer(view->get_transform())->get_transform();
  auto* text = BSML::Lite::CreateText(parent, u"NE Fixed Add-on", TMPro::FontStyles::Normal, 3.1f);
  statusText = text;
  text->set_richText(false);
  refresh();
  BSML::Lite::CreateToggle(parent, u"Enable NE Fixed add-on", enabled, [](bool value) {
    enabled = value;
    try { Save(); } catch (std::exception const& e) { Logger.error("Could not save add-on settings: {}", e.what()); }
  });
  BSML::Lite::CreateToggle(parent, u"Default notes on note-effect maps", defaultNotes, [](bool value) {
    defaultNotes = value;
    try { Save(); } catch (std::exception const& e) { Logger.error("Could not save add-on settings: {}", e.what()); }
  });
  BSML::Lite::CreateToggle(parent, u"Repair missed cutout updates", cutoutPrecision, [](bool value) {
    cutoutPrecision = value;
    try { Save(); } catch (std::exception const& e) { Logger.error("Could not save add-on settings: {}", e.what()); }
  });
}
}  // namespace

modloader::ModInfo const& Info() { return modInfo; }
BaseStatus GetBaseStatus() { return baseStatus; }
bool Enabled() { return enabled; }
bool DefaultNotes() { return defaultNotes; }
bool CutoutPrecision() { return cutoutPrecision; }
void SetLastScene(std::string message) { lastScene = std::move(message); }

void Setup() {
  Config().Load();
  enabled = ReadBool("enabled", true);
  defaultNotes = ReadBool("defaultNotesForNoteEffects", true);
  cutoutPrecision = ReadBool("cutoutPrecision", true);
  // Writes only ne-fixed.json, never the original NE or cosmetic mod config.
  Save();
}

void Load() {
  if (registered) return;
  registered = true;
  il2cpp_functions::Init();
  // Load the public extension services before registering callbacks/types.
  // Metadata versions differ from some package tags, so the QMOD manifest
  // pins Lapiz while the loader request uses its documented package ID.
  for (auto id : {"lapiz", "bsml"}) {
    CModInfo service{id, "", 0};
    if (modloader_require_mod(&service, MatchType_IdOnly) != MatchType_Loaded) {
      Logger.error("Required add-on service {} could not load; no patches installed", id);
      return;
    }
  }
  CModInfo base{"NoodleExtensions", "1.6.3", 0};
  // Public loader API only. Do not link/replace libnoodleextensions, unload NE,
  // register its capability again, or reach into its private hooks/caches.
  auto result = modloader_require_mod(&base, MatchType_IdVersion);
  auto found = modloader_get_mod(&base, MatchType_IdOnly);
  baseStatus = CheckBase(result == MatchType_Loaded && found.handle,
                        found.info.id ? found.info.id : "",
                        found.info.version ? found.info.version : "");
  baseDescription = baseStatus == BaseStatus::Ready
    ? "NE 1.6.3 detected; add-on available"
    : "Add-on inactive: requires official NE 1.6.3 (not the legacy 1.8.x fork)";
  Logger.info("{}; detected version={}", baseDescription, found.info.version ? found.info.version : "missing");
  if (baseStatus == BaseStatus::Ready) {
    InstallNoteCompatibility();
    InstallCutoutPrecision();
  }
  auto menu = BSML::BSMLSettings::get_instance()->TryAddSettingsMenu(
    [](HMUI::ViewController* view, bool first, bool added, bool enabling) {
      try { Menu(view, first, added, enabling); }
      catch (std::exception const& e) { Logger.error("Add-on menu failed: {}", e.what()); }
    }, "NE Fixed Add-on", false);
  Logger.info("Add-on {} registered; menu={}; enabled={}; no duplicate NE core/injector", VERSION, menu, enabled);
}
}  // namespace NEFixed

extern "C" __attribute__((visibility("default"))) void setup(CModInfo* info) {
  if (info) *info = NEFixed::Info().to_c();
  try { NEFixed::Setup(); }
  catch (std::exception const& e) { NEFixed::Logger.error("Add-on config load failed: {}", e.what()); }
}

extern "C" __attribute__((visibility("default"))) void late_load() {
  try { NEFixed::Load(); }
  catch (std::exception const& e) { NEFixed::Logger.error("Add-on load failed: {}; original NE is not replaced", e.what()); }
}
