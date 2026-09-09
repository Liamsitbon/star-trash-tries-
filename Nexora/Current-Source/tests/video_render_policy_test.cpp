#include "VideoRenderPolicy.hpp"

// Fresh installs and upgrades with the old diagnostic flag disabled both use
// direct rendering. Explicit opt-out remains possible, and the diagnostic
// override still works independently of the release setting.
static_assert(Nexora::kDefaultDirectVideoRendering);
static_assert(Nexora::UseDirectVideoRendering(
    Nexora::kDefaultDirectVideoRendering, false));
static_assert(Nexora::UseDirectVideoRendering(true, false));
static_assert(Nexora::UseDirectVideoRendering(true, true));
static_assert(Nexora::UseDirectVideoRendering(false, true));
static_assert(!Nexora::UseDirectVideoRendering(false, false));

int main() {}
