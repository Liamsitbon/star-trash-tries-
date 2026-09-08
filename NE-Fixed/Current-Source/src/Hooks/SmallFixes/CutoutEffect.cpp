#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"

#include "GlobalNamespace/DisappearingArrowControllerBase_1.hpp"
#include "GlobalNamespace/GameNoteController.hpp"
#include "GlobalNamespace/CutoutEffect.hpp"
#include "GlobalNamespace/CutoutAnimateEffect.hpp"
#include "UnityEngine/Vector3.hpp"
#include "UnityEngine/Mathf.hpp"

#include "Animation/AnimationHelper.h"
#include "Animation/ParentObject.h"
#include "AssociatedData.h"
#include "NEHooks.h"
#include "custom-json-data/shared/CustomBeatmapData.h"

using namespace GlobalNamespace;
using namespace UnityEngine;
using namespace TrackParenting;

MAKE_HOOK_MATCH(CutoutEffect_SetCutout,
                static_cast<void (GlobalNamespace::CutoutEffect::*)(float, UnityEngine::Vector3)>(
                    &GlobalNamespace::CutoutEffect::SetCutout),
                void, CutoutEffect* self, float cutout, UnityEngine::Vector3 cutoutOffset) {
  // Do not run SetCutout if the new value is the same as old.
  // Match Heck's comparison. A 0.005 dead zone discards the final transition
  // to fully hidden and can leave thin rows of future/fake notes on screen.
  if (UnityEngine::Mathf::Approximately(cutout, self->_cutout)) return;

  CutoutEffect_SetCutout(self, cutout, cutoutOffset);
}

// Do not inline-hook CutoutAnimateEffect.Start: the 1.40.8 Quest method is
// only 8 bytes, below beatsaber-hook's 20-byte patch requirement. PC Heck's
// Harmony SkipStart patch cannot be transplanted here safely. The existing
// SetArrowTransparency hook reapplies the arrow cutout even on a cache hit.

void InstallCutoutEffectHooks() {
  INSTALL_HOOK(NELogger::Logger, CutoutEffect_SetCutout);
}

NEInstallHooks(InstallCutoutEffectHooks);
