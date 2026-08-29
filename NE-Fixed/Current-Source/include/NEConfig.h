#pragma once
#include "config-utils/shared/config-utils.hpp"

#include "beatsaber-hook/shared/utils/typedefs-string.hpp"

enum struct MaterialBehaviour { SMART_COLOR = 0, SEMI_BASIC = 1, BASIC = 2 };

inline std::vector<std::string> getMaterialBehaviourValues() {
  return { { "Smart Color", "SemiBasic", "Basic" } };
}

DECLARE_CONFIG(NEConfig) {
  CONFIG_VALUE(enableNoteDissolve, bool, "Enable note dissolve", true);
  CONFIG_VALUE(enableMirrorNoteDissolve, bool, "Enable mirror note dissolve", true,
              "If enabled, allows note mirrors to dissolve. When disabled, hides the notes if dissolved");
  CONFIG_VALUE(enableObstacleDissolve, bool, "Enable obstacle dissolve", true);
  CONFIG_VALUE(qosmeticsModelDisable, bool, "Disable Qosmetics models on NE maps", true,
              "If enabled, NE will disable qosmetics walls and notes to improve performance");
  CONFIG_VALUE(materialBehaviour, int, "Obstacle material behaviour", 0);
  CONFIG_VALUE(autoResetRuntimeState, bool, "Reset runtime state between maps", true,
              "Clears note, wall, track and movement caches during level/scene transitions to prevent stale state");
  CONFIG_VALUE(runtimeDiagnostics, bool, "Runtime diagnostics", true,
              "Logs map activation decisions and cache reset summaries to help diagnose broken modcharts");
  CONFIG_VALUE(disableOnMappingExtensionsConflict, bool, "Block NE + ME conflicts", true,
              "Blocks selection and Noodle runtime activation when a difficulty requires both Noodle Extensions and Mapping Extensions");
};