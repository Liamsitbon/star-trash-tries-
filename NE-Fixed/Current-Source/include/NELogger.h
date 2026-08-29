#pragma once
#include "beatsaber-hook/shared/utils/logging.hpp"
#include "beatsaber-hook/shared/config/rapidjson-utils.hpp"

namespace NoodleExtensions {
static inline const std::u16string REQUIREMENTNAME = u"Noodle Extensions";
static inline const std::string U8_REQUIREMENTNAME = "Noodle Extensions";

static inline const std::string U8_ME_REQUIREMENTNAME = "Mapping Extensions";
static inline const std::u16string ME_REQUIREMENTNAME = u"Mapping Extensions";

// Vivify is intentionally treated as compatible. This constant is used only for
// diagnostics; Vivify remains responsible for its own requirement/capability.
static inline const std::string U8_VIVIFY_REQUIREMENTNAME = "Vivify";
// TODO: Move all JSON property references to constants?
} // namespace NoodleExtensions

class NELogger {
public:
  static constexpr auto Logger = Paper::ConstLoggerContext("NoodleExtensions");
};

void PrintJSONValue(rapidjson::Value const& json);
void PrintBacktrace(size_t maxLines);
