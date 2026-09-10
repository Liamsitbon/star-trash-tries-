// SPDX-License-Identifier: MIT
#include "Addon.hpp"
#include "CutoutPolicy.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "GlobalNamespace/CutoutEffect.hpp"
#include "GlobalNamespace/MaterialPropertyBlockController.hpp"
#include "UnityEngine/MaterialPropertyBlock.hpp"
#include "UnityEngine/Vector4.hpp"

namespace NEFixed {
namespace {
using GlobalNamespace::CutoutEffect;
using UnityEngine::Vector3;

MAKE_HOOK_MATCH(NEFixed_CutoutEffect_SetCutout,
    static_cast<void (CutoutEffect::*)(float, Vector3)>(&CutoutEffect::SetCutout),
    void, CutoutEffect* self, float requested, Vector3 offset) {
  // Always preserve the existing hook chain. No trampoline theft or recursive
  // SetCutout call: the one-argument overload tail-calls this very overload.
  NEFixed_CutoutEffect_SetCutout(self, requested, offset);
  if (!self || !Enabled() || !CutoutPrecision() || GetBaseStatus() != BaseStatus::Ready ||
      !NeedsCutoutRepair(requested, self->_cutout)) return;
  if (!std::isfinite(offset.x) || !std::isfinite(offset.y) || !std::isfinite(offset.z)) return;
  auto controller = self->_materialPropertyBlockController;
  if (!controller) return;
  auto* block = controller->get_materialPropertyBlock();
  if (!block) return;

  // Same property update path as the 1.40.8_7379 game method, checked against
  // the captured IL2CPP binary (see MIGRATION.md). Preserve the offset passed
  // to this call; _cutoutOffset is a separate configured field, not a cache.
  il2cpp_functions::runtime_class_init(classof(CutoutEffect*));
  block->SetVector(CutoutEffect::getStaticF__cutoutTexOffsetPropertyID(),
                   UnityEngine::Vector4(offset.x, offset.y, offset.z, 0));
  block->SetFloat(CutoutEffect::getStaticF__cutoutPropertyID(), requested);
  controller->ApplyChanges();
  self->_cutout = requested;
}
}  // namespace

void InstallCutoutPrecision() {
  // This method is 0xe4 bytes in the supported game, not the unsafe 8-byte
  // CutoutAnimateEffect.Start. The original NE hook remains in the chain.
  INSTALL_HOOK(Logger, NEFixed_CutoutEffect_SetCutout);
  Logger.info("Cutout precision post-hook installed after NE: exact missed updates only, partial dissolve preserved");
}
}  // namespace NEFixed
