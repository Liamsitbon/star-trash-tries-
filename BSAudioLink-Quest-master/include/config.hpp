#pragma once

#include "paper2_scotland2/shared/logger.hpp"
#include "scotland2/shared/loader.hpp"

struct config_t {
    bool showTestPlane = false;
};

void SaveConfig();
bool LoadConfig();

extern config_t config;

static modloader::ModInfo modInfo{MOD_ID, VERSION, 0};

constexpr auto AudioLinkLogger = Paper::ConstLoggerContext("AudioLink");
