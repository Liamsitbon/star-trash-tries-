#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "NELogger.h"

class Hooks {
private:
  /// Store function names and install function pointers so hooks are installed in a deterministic order.
  static inline std::vector<std::pair<std::string, void (*)()>> installFuncs;
  static inline bool NoodleHookEnabled = false;
  static inline bool hooksInstalled = false;

public:
  static void AddInstallFunc(std::string name, void (*installFunc)()) {
    auto duplicate = std::find_if(installFuncs.begin(), installFuncs.end(),
                                  [&](auto const& entry) { return entry.first == name; });
    if (duplicate != installFuncs.end()) {
      NELogger::Logger.warn("Ignored duplicate hook installer registration: {}", name);
      return;
    }

    installFuncs.emplace_back(std::move(name), installFunc);
  }

  static void InstallHooks() {
    if (hooksInstalled) {
      NELogger::Logger.warn("InstallHooks was called more than once; duplicate installation was blocked");
      return;
    }

    std::sort(installFuncs.begin(), installFuncs.end(),
              [](auto const& a, auto const& b) { return a.first < b.first; });

    NELogger::Logger.info("Installing {} NoodleExtensions hook groups", installFuncs.size());
    for (auto const& installFunc : installFuncs) {
      NELogger::Logger.debug("Installing hook group: {}", installFunc.first);
      installFunc.second();
    }

    hooksInstalled = true;
  }

  static bool isNoodleHookEnabled() {
    return NoodleHookEnabled;
  }

  static void setNoodleHookEnabled(bool noodleHookEnabled) {
    if (NoodleHookEnabled == noodleHookEnabled) return;

    NoodleHookEnabled = noodleHookEnabled;
    NELogger::Logger.debug("Noodle runtime hooks are now {}", noodleHookEnabled ? "enabled" : "disabled");
  }

  static std::size_t RegisteredHookGroupCount() {
    return installFuncs.size();
  }
};

#define NEInstallHooks(func)                                                                                           \
  struct __NERegister##func {                                                                                          \
    __NERegister##func() {                                                                                             \
      Hooks::AddInstallFunc(std::string(#func), func);                                                                 \
      NELogger::Logger.debug("Registered NE hook installer: " #func);                                                 \
    }                                                                                                                  \
  };                                                                                                                   \
  static __NERegister##func __NERegisterInstance##func;

void InstallAndRegisterAll();
