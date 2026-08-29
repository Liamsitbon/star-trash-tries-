#include "NEConfig.h"
#include "NEHooks.h"
#include "NELogger.h"

#include "beatsaber-hook/shared/utils/il2cpp-functions.hpp"
#include "custom-types/shared/register.hpp"
#include "scotland2/shared/loader.hpp"

namespace {
static modloader::ModInfo modInfo{MOD_ID, VERSION, 0};
}

extern "C" __attribute__((visibility("default"))) void setup(CModInfo* info) {
  *info = modInfo.to_c();
  NEConfig_t::Init({ MOD_ID, VERSION, 0 });
  NELogger::Logger.info("NoodleExtensions {} setup complete", VERSION);
}

extern "C" __attribute__((visibility("default"))) void late_load() {
  il2cpp_functions::Init();
  custom_types::Register::AutoRegister();
  InstallAndRegisterAll();
}
