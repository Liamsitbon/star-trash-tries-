#include "config.hpp"
#include "ShaderProperties.hpp"
#include "_config.hpp"
#include "beatsaber-hook/shared/config/config-utils.hpp"

config_t config;

Configuration& get_config() {
    static Configuration config(modInfo);
    config.Load();
    return config;
}

#define Save(identifier) doc.AddMember(#identifier, config.identifier, allocator)

void SaveConfig() {
    AudioLinkLogger.info("Saving Configuration...");
    rapidjson::Document& doc = get_config().config;

    doc.RemoveAllMembers();
    doc.SetObject();
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();
    Save(showTestPlane);
    get_config().Write();
    AudioLinkLogger.info("Saved Configuration!");
}

#define GET_BOOL(identifier)                                                     \
    do {                                                                         \
        auto identifier##_itr = doc.FindMember(#identifier);                     \
        if (identifier##_itr != doc.MemberEnd() && identifier##_itr->value.IsBool()) { \
            config.identifier = identifier##_itr->value.GetBool();               \
        } else {                                                                 \
            foundEverything = false;                                             \
            AudioLinkLogger.warn("Invalid or missing config value '{}'; using default.", #identifier); \
        }                                                                        \
    } while (false)

bool LoadConfig() {
    AudioLinkLogger.info("Loading Configuration...");
    bool foundEverything = true;
    rapidjson::Document& doc = get_config().config;

    if (!doc.IsObject()) {
        AudioLinkLogger.warn("AudioLink config root is not an object; regenerating defaults.");
        return false;
    }

    GET_BOOL(showTestPlane);

    if (foundEverything)
        AudioLinkLogger.info("Loaded Configuration!");
    return foundEverything;
}
