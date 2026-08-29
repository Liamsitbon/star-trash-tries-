#include "VivifyRuntimeInternal.hpp"
#include "VivifyComponents.hpp"
#include "UnityEngine/Renderer.hpp"
#include "UnityEngine/Matrix4x4.hpp"

DEFINE_TYPE(Vivify, OffsetBladeMovementData);
DEFINE_TYPE(Vivify, FollowedSaberTrail);
DEFINE_TYPE(Vivify, RuntimeBehaviour);
DEFINE_TYPE(Vivify, MultipassKeywordController);
DEFINE_TYPE(Vivify, CameraApplier);
DEFINE_TYPE(Vivify, CullingCameraController);
DEFINE_TYPE(Vivify, SecondaryCameraController);

namespace Vivify {

namespace {
inline int StereoActiveEyePropertyId() {

  static int id = UnityEngine::Shader::PropertyToID(u"_StereoActiveEye");
  return id;
}
}

void SetMultipassShaderState(bool enabled, UnityEngine::XR::XRSettings_StereoRenderingMode stereoMode, int eye) {
  // _StereoActiveEye is a physical-eye selector only for true MultiPass.
  // Quest commonly runs Multiview (mode 3), where the two eyes are slices of
  // the same Tex2DArray. Feeding Camera.stereoActiveEye into a multiview shader
  // can make one eye sample/render a different slice and produce duplicated or
  // divergent whole-frame output. Keep the global neutral in every non-
  // MultiPass mode; explicit per-eye array Blits temporarily override it in
  // their own command buffer and restore it to zero afterwards.
  bool const trueMultipass = enabled && UnityEngine::XR::XRSettings::get_enabled() &&
                             stereoMode.value__ == 0;

  static int lastEye = -2;
  static bool lastEnabled = false;
  static int lastStereoMode = -1;
  int const selectedEye = trueMultipass ? (eye > 0 ? 1 : 0) : 0;
  if (lastEye == selectedEye && lastEnabled == trueMultipass &&
      lastStereoMode == stereoMode.value__) return;
  lastEye = selectedEye;
  lastEnabled = trueMultipass;
  lastStereoMode = stereoMode.value__;

  UnityEngine::Shader::SetGlobalInt(StereoActiveEyePropertyId(), selectedEye);
}

void SetMultipassShaderStateForCamera(UnityEngine::Camera* camera) {
  bool const hasCamera = IsManagedAlive(camera);
  auto const stereoMode = UnityEngine::XR::XRSettings::get_stereoRenderingMode();
  bool const trueMultipass = UnityEngine::XR::XRSettings::get_enabled() && stereoMode.value__ == 0;
  bool const isStereoCamera = hasCamera && trueMultipass &&
                              camera->get_stereoTargetEye().value__ != UnityEngine::StereoTargetEyeMask::None.value__;
  int const activeEye = isStereoCamera ? camera->get_stereoActiveEye().value__ : 0;
  SetMultipassShaderState(isStereoCamera, stereoMode, activeEye);
}

void OffsetBladeMovementData::Init(GlobalNamespace::IBladeMovementData* followed, UnityEngine::Transform* parent,
                                   UnityEngine::Vector3 topPos, UnityEngine::Vector3 bottomPos) {
  _followed = followed;
  _parent = parent;
  _topPos = topPos;
  _bottomPos = bottomPos;
}

float_t OffsetBladeMovementData::get_bladeSpeed() {
  return 0.0f;
}

GlobalNamespace::BladeMovementDataElement OffsetBladeMovementData::get_lastAddedData() {
  if (_followed == nullptr) return {};
  return Modify(_followed->get_lastAddedData());
}

GlobalNamespace::BladeMovementDataElement OffsetBladeMovementData::get_prevAddedData() {
  if (_followed == nullptr) return {};
  return Modify(_followed->get_prevAddedData());
}

GlobalNamespace::BladeMovementDataElement OffsetBladeMovementData::Modify(GlobalNamespace::BladeMovementDataElement original) {
  if (!IsManagedAlive(_parent)) return original;
  return GlobalNamespace::BladeMovementDataElement(
      original.time,
      original.segmentAngle,
      AddVectors(original.bottomPos, _parent->TransformVector(_topPos)),
      AddVectors(original.bottomPos, _parent->TransformVector(_bottomPos)),
      original.segmentNormal);
}

void FollowedSaberTrail::Awake() {}

void FollowedSaberTrail::InitFollowed(GlobalNamespace::SaberTrail* followed, UnityEngine::Transform* parent,
                                      UnityEngine::Material* material, UnityEngine::Vector3 topPos,
                                      UnityEngine::Vector3 bottomPos, float_t duration,
                                      int32_t samplingFrequency, int32_t granularity) {
  if (!IsManagedAlive(followed) || !IsManagedAlive(parent) || !IsManagedAlive(material)) return;
  auto* followedRenderer = followed->____trailRenderer.unsafePtr();
  auto* rendererPrefab = followed->____trailRendererPrefab.unsafePtr();
  if (!IsManagedAlive(followedRenderer) || !IsManagedAlive(rendererPrefab)) return;

  _followed = followed;
  if (_offsetMovementData == nullptr) {
    _offsetMovementData = OffsetBladeMovementData::New_ctor();
  }
  _offsetMovementData->Init(followed->____movementData, parent, topPos, bottomPos);

  ____trailDuration = duration;
  ____samplingFrequency = samplingFrequency;
  ____granularity = granularity;
  ____whiteSectionMaxDuration = followed->____whiteSectionMaxDuration;
  ____movementData = reinterpret_cast<GlobalNamespace::IBladeMovementData*>(_offsetMovementData);
  ____color = followed->____color;

  auto* trailRenderer = ____trailRenderer.unsafePtr();
  if (!IsManagedAlive(trailRenderer)) {
    trailRenderer = UnityEngine::Object::Instantiate<GlobalNamespace::SaberTrailRenderer*>(
        rendererPrefab, UnityEngine::Vector3(0.0f, 0.0f, 0.0f), UnityEngine::Quaternion::get_identity());
    ____trailRenderer = trailRenderer;
    if (!IsManagedAlive(trailRenderer)) return;
    auto sourceParent = followedRenderer->get_transform()->get_parent();
    if (IsManagedAlive(sourceParent.unsafePtr())) {
      trailRenderer->get_transform()->SetParent(sourceParent.unsafePtr());
    }
  }

  if (trailRenderer->____meshRenderer) {
    trailRenderer->____meshRenderer->set_material(material);
  }

  Init();
  Update();
  auto gameObject = get_gameObject();
  if (IsManagedAlive(gameObject.unsafePtr())) {
    gameObject->SetActive(true);
  }
}

void FollowedSaberTrail::Update() {
  if (!IsManagedAlive(_followed)) return;
  auto* trailRenderer = ____trailRenderer.unsafePtr();
  if (!IsManagedAlive(trailRenderer) || !trailRenderer->____meshRenderer) return;
  ____color = _followed->____color;
  if (_hasLastAppliedColor && NearlySameColor(____color, _lastAppliedColor)) return;

  if (!_colorPropertyBlock) {
    auto* created = UnityEngine::MaterialPropertyBlock::New_ctor();
    if (created != nullptr) _colorPropertyBlock.emplace(created);
  }
  auto* block = _colorPropertyBlock ? _colorPropertyBlock.ptr() : nullptr;
  if (block == nullptr) return;
  block->SetColor(ColorPropertyId(), ____color);
  trailRenderer->____meshRenderer->SetPropertyBlock(block);
  _lastAppliedColor = ____color;
  _hasLastAppliedColor = true;
}

void FollowedSaberTrail::LateUpdate() {
  auto* trailRenderer = ____trailRenderer.unsafePtr();
  auto* meshRenderer = IsManagedAlive(trailRenderer) ? trailRenderer->____meshRenderer.unsafePtr() : nullptr;
  if (!IsManagedAlive(_followed) || ____movementData == nullptr || !IsManagedAlive(meshRenderer)) {
    Cleanup();
    auto gameObject = get_gameObject();
    if (IsManagedAlive(gameObject.unsafePtr())) {
      gameObject->SetActive(false);
    }
    return;
  }
  static_cast<GlobalNamespace::SaberTrail*>(this)->LateUpdate();
}

void FollowedSaberTrail::Cleanup() {
  auto* trailRenderer = ____trailRenderer.unsafePtr();
  if (IsManagedAlive(trailRenderer)) {
    UnityEngine::Object::Destroy(trailRenderer->get_gameObject());
  }
  ____trailRenderer = nullptr;
  _followed = nullptr;
  _colorPropertyBlock.clear();
  _hasLastAppliedColor = false;
}

namespace {

constexpr int kCullingLayer = 22;

bool HasSecondaryCameraController(UnityEngine::MonoBehaviour* self) {
  auto go = self->get_gameObject();
  if (!IsManagedAlive(go.unsafePtr())) return false;
  return go->GetComponent<SecondaryCameraController*>() != nullptr;
}

bool IsMenuMainCamera(UnityEngine::MonoBehaviour* self) {
  if (self == nullptr) return false;
  auto go = self->get_gameObject();
  return IsManagedAlive(go.unsafePtr()) && go->get_name() == "MenuMainCamera";
}
}

void CullingCameraController::OnPreCull() {
  if (HasSecondaryCameraController(this)) return;
  CullingPreCull();
}

void CullingCameraController::OnPostRender() {
  if (HasSecondaryCameraController(this)) return;
  CullingPostRender();
}

void CullingCameraController::CullingPreCull() {
  if (_camera == nullptr) _camera = GetComponent<UnityEngine::Camera*>();
  if (!IsManagedAlive(_camera)) return;
  if (!_hasCullingData) return;
  bool const diagnostics = GetVivifyDebugLogging();
  float const startedAt = diagnostics ? UnityEngine::Time::get_realtimeSinceStartup() : 0.0f;

  CullingPostRender();

  // Preserve the map-authored whitelist/blacklist isolation and restore every
  // temporary layer mutation as soon as the helper camera finishes.

  if (_whitelist) {
    _cachedMask = _camera->get_cullingMask();
    _camera->set_cullingMask(1 << kCullingLayer);
  }

  _seenTrackedRoots.clear();
  _seenRendererGameObjects.clear();
  int const frame = static_cast<int>(UnityEngine::Time::get_frameCount());
  int trackedGameObjects = 0;
  int cachedRenderers = 0;
  int refreshedRoots = 0;
  for (auto const& track : _tracks) {
    if (!track) continue;
    for (auto* go : track.GetGameObjects()) {
      if (!IsManagedAlive(go)) continue;
      if (!_seenTrackedRoots.emplace(go).second) continue;
      trackedGameObjects++;

      auto transform = go->get_transform();
      auto* transformPtr = transform.unsafePtr();
      int const directChildCount = IsManagedAlive(transformPtr) ? transformPtr->get_childCount() : -1;
      auto [cacheIt, inserted] = _rendererCache.try_emplace(go);
      auto& cache = cacheIt->second;
      bool const needsRefresh = inserted || cache.directChildCount != directChildCount ||
                                frame >= cache.nextRefreshFrame;
      if (needsRefresh) {
        cache.renderers.clear();
        auto renderers = go->GetComponentsInChildren<UnityEngine::Renderer*>(true);
        if (renderers) {
          cache.renderers.reserve(renderers.size());
          for (auto* renderer : renderers) {
            if (IsManagedAlive(renderer)) cache.renderers.emplace_back(renderer);
          }
        }
        cache.directChildCount = directChildCount;
        cache.nextRefreshFrame = frame +
            StaggeredRefreshDelay(reinterpret_cast<std::uintptr_t>(go));
        refreshedRoots++;
      }

      cachedRenderers += static_cast<int>(cache.renderers.size());
      for (auto* renderer : cache.renderers) {
        if (!IsManagedAlive(renderer)) continue;
        // Disabled/inactive renderers cannot contribute to this camera. Not
        // moving them avoids thousands of native SetLayer calls on dense
        // pooled-note tracks while preserving the authored visible result.
        if (!renderer->get_enabled()) continue;
        auto renderGo = renderer->get_gameObject();
        auto* renderGoPtr = renderGo.unsafePtr();
        if (!IsManagedAlive(renderGoPtr) || !renderGoPtr->get_activeInHierarchy()) continue;
        if (!_seenRendererGameObjects.emplace(renderGoPtr).second) continue;
        _cachedLayers.emplace_back(renderGoPtr, renderGoPtr->get_layer());
        renderGoPtr->set_layer(kCullingLayer);
      }
    }
  }

  if (_diagLogCount < 3 && diagnostics) {
    _diagLogCount++;
    auto go = get_gameObject();
    PaperLogger.info("Vivify culling pass on '{}': whitelist={} tracks={} trackedGOs={} cachedRenderers={} refreshedRoots={} activeMovedGOs={} camMask=0x{:08x}",
                     IsManagedAlive(go.unsafePtr()) ? ToStdString(go->get_name()) : std::string("?"),
                     BoolText(_whitelist), _tracks.size(), trackedGameObjects, cachedRenderers, refreshedRoots,
                     _cachedLayers.size(),
                     static_cast<uint32_t>(_camera->get_cullingMask()));
  }
  if (diagnostics && _stallLogCount < 5) {
    float const elapsedMs = (UnityEngine::Time::get_realtimeSinceStartup() - startedAt) * 1000.0f;
    if (elapsedMs >= 3.0f) {
      _stallLogCount++;
      auto go = get_gameObject();
      PaperLogger.warn("Vivify culling CPU stall on '{}': {:.2f}ms trackedGOs={} cachedRenderers={} refreshedRoots={} activeMovedGOs={}",
                       IsManagedAlive(go.unsafePtr()) ? ToStdString(go->get_name()) : std::string("?"),
                       elapsedMs, trackedGameObjects, cachedRenderers, refreshedRoots, _cachedLayers.size());
    }
  }
}

void CullingCameraController::OnDisable() {
  CullingPostRender();
}

void CullingCameraController::CullingPostRender() {
  auto& runtime = Runtime::Instance();
  if (runtime.GetCurrentBeatmapData() == nullptr) {
    // A transition reset intentionally invalidates all old scene wrappers.
    // Forget cached layers without probing them; Unity is already destroying
    // those objects and any op_Implicit/SetLayer call can dereference freed
    // native memory.
    _cachedMask.reset();
    _cachedLayers.clear();
    _camera = nullptr;
    return;
  }
  if (_camera != nullptr && UnityEngine::Object::op_Implicit_bool(_camera) && _cachedMask.has_value()) {
    _camera->set_cullingMask(_cachedMask.value());
    _cachedMask.reset();
  }
  for (auto const& [go, layer] : _cachedLayers) {
    if (IsManagedAlive(go)) {
      go->set_layer(layer);
    }
  }
  _cachedLayers.clear();
}

void SecondaryCameraController::OnPreCull() {
  auto* camera = GetCamera();
  if (!IsManagedAlive(camera)) return;
  auto mainCam = UnityEngine::Camera::get_main();
  auto* other = mainCam.unsafePtr();
  if (IsManagedAlive(other) && other != camera) {
    // Preserve the same stereo target and matrices as the main camera.
    if (camera->get_stereoTargetEye().value__ != other->get_stereoTargetEye().value__ ||
        camera->get_cullingMask() != other->get_cullingMask() ||
        camera->get_fieldOfView() != other->get_fieldOfView() ||
        camera->get_nearClipPlane() != other->get_nearClipPlane() ||
        camera->get_farClipPlane() != other->get_farClipPlane()) {
      camera->set_stereoTargetEye(other->get_stereoTargetEye());
      camera->set_fieldOfView(other->get_fieldOfView());
      camera->set_aspect(other->get_aspect());
      camera->set_depth(other->get_depth() - 1.0f);
      camera->set_nearClipPlane(other->get_nearClipPlane());
      camera->set_farClipPlane(other->get_farClipPlane());
      camera->set_layerCullDistances(other->get_layerCullDistances());
      // Do not clear the secondary camera output here. CreateCamera may have
      // attached a RenderTexture or explicit color/depth buffers. Clearing the
      // target during OnPreCull makes the camera render to the headset instead
      // of the map texture, leaving composited notes/sabers invisible on Quest.
      camera->set_cullingMask(other->get_cullingMask());
    }
    auto transform = get_transform();
    if (IsManagedAlive(transform.unsafePtr())) {
      transform->set_localPosition(UnityEngine::Vector3::get_zero());
      transform->set_localRotation(UnityEngine::Quaternion::get_identity());
    }
    bool const actualMultipass = UnityEngine::XR::XRSettings::get_enabled() &&
                                 UnityEngine::XR::XRSettings::get_stereoRenderingMode().value__ == 0;
    int const stereoMode = UnityEngine::XR::XRSettings::get_stereoRenderingMode().value__;
    bool const textureArrayStereo = UnityEngine::XR::XRSettings::get_enabled() &&
                                    (stereoMode == 2 || stereoMode == 3) &&
                                    other->get_stereoTargetEye().value__ !=
                                        UnityEngine::StereoTargetEyeMask::None.value__;
    if (textureArrayStereo) {
      // SetStereoProjectionMatrix is only legal in physical MultiPass. Quest
      // reports an error for it in Multiview and can retain a partial override,
      // making the two eyes diverge. Leave both eye matrices to XR and copy only
      // the camera's generic state used by authored shaders.
      try {
        if (_stereoMatricesApplied) {
          camera->ResetStereoProjectionMatrices();
          camera->ResetStereoViewMatrices();
          _stereoMatricesApplied = false;
        }
        camera->set_projectionMatrix(other->get_projectionMatrix());
        camera->set_nonJitteredProjectionMatrix(other->get_nonJitteredProjectionMatrix());
        camera->set_worldToCameraMatrix(other->get_worldToCameraMatrix());
        camera->ResetCullingMatrix();

        if (_stereoSyncLogCount < 2 && GetVivifyDebugLogging()) {
          _stereoSyncLogCount++;
          PaperLogger.info(
              "Vivify secondary stereo sync: camera='{}' mode={} perEyeMatrices=xr-managed culling=xr-managed",
              cameraName, stereoMode);
        }
      } catch (std::exception const& ex) {
        // Never leave only one eye overridden after a partial failure.
        camera->ResetStereoProjectionMatrices();
        camera->ResetStereoViewMatrices();
        camera->ResetCullingMatrix();
        _stereoMatricesApplied = false;
        if (_stereoSyncFailureLogCount < 3) {
          _stereoSyncFailureLogCount++;
          PaperLogger.warn("Vivify secondary stereo sync failed: camera='{}' mode={} error={}",
                           cameraName, stereoMode, ex.what());
        }
      } catch (...) {
        camera->ResetStereoProjectionMatrices();
        camera->ResetStereoViewMatrices();
        camera->ResetCullingMatrix();
        _stereoMatricesApplied = false;
        if (_stereoSyncFailureLogCount < 3) {
          _stereoSyncFailureLogCount++;
          PaperLogger.warn("Vivify secondary stereo sync failed: camera='{}' mode={} non-std exception",
                           cameraName, stereoMode);
        }
      }
    } else if (!actualMultipass) {
      if (_stereoMatricesApplied) {
        camera->ResetStereoProjectionMatrices();
        camera->ResetStereoViewMatrices();
        _stereoMatricesApplied = false;
      }
      camera->set_cullingMatrix(
          UnityEngine::Matrix4x4::op_Multiply(other->get_projectionMatrix(), other->get_worldToCameraMatrix()));
      camera->set_projectionMatrix(other->get_projectionMatrix());
      camera->set_nonJitteredProjectionMatrix(other->get_nonJitteredProjectionMatrix());
      camera->set_worldToCameraMatrix(other->get_worldToCameraMatrix());
    } else if (_stereoMatricesApplied) {
      camera->ResetStereoProjectionMatrices();
      camera->ResetStereoViewMatrices();
      camera->ResetCullingMatrix();
      _stereoMatricesApplied = false;
    }
  }

  auto* culling = GetComponent<CullingCameraController*>();
  if (IsManagedAlive(culling)) {
    culling->CullingPreCull();
  }
}

void SecondaryCameraController::OnPostRender() {
  auto* culling = GetComponent<CullingCameraController*>();
  if (IsManagedAlive(culling)) {
    culling->CullingPostRender();
  }
}

void SecondaryCameraController::OnRenderImage(UnityEngine::RenderTexture* src,
                                              UnityEngine::RenderTexture* dest) {
  (void)dest;
  auto* camera = GetCamera();
  if (!IsManagedAlive(camera) || !IsManagedAlive(src) || cameraName.empty()) return;
  Runtime::Instance().CaptureSecondaryCameraFrame(cameraName, camera, src);
  // Deliberately do not blit to dest. This targetless helper camera renders
  // immediately before the main camera, which overwrites the headset target.
  // A second blit here adds a full-screen GPU pass without affecting the map's
  // captured texture (matching upstream Vivify's secondary-camera path).
}

UnityEngine::Camera* SecondaryCameraController::GetCamera() {
  if (!IsManagedAlive(_camera)) {
    auto go = get_gameObject();
    _camera = IsManagedAlive(go.unsafePtr()) ? go->GetComponent<UnityEngine::Camera*>() : nullptr;
  }
  return _camera;
}

void SecondaryCameraController::OnDisable() {
  auto& runtime = Runtime::Instance();
  if (runtime.GetCurrentBeatmapData() == nullptr) return;
  auto* culling = GetComponent<CullingCameraController*>();
  if (IsManagedAlive(culling)) {
    culling->CullingPostRender();
  }
}

void SecondaryCameraController::OnDestroy() {
  auto& runtime = Runtime::Instance();
  if (runtime.GetCurrentBeatmapData() != nullptr) {
    auto* culling = GetComponent<CullingCameraController*>();
    if (IsManagedAlive(culling)) culling->CullingPostRender();
  }
  _camera = nullptr;
  _stereoMatricesApplied = false;
  cameraName.clear();
}

void MultipassKeywordController::Awake() {
  auto gameObject = get_gameObject();
  if (IsManagedAlive(gameObject.unsafePtr())) {
    _camera = gameObject->GetComponent<UnityEngine::Camera*>();
  }
}

void MultipassKeywordController::OnPreRender() {
  if (!IsManagedAlive(_camera)) {
    auto gameObject = get_gameObject();
    _camera = IsManagedAlive(gameObject.unsafePtr()) ? gameObject->GetComponent<UnityEngine::Camera*>() : nullptr;
  }
  SetMultipassShaderStateForCamera(_camera);
}

void MultipassKeywordController::OnDisable() {
  SetMultipassShaderState(false);
}

void RuntimeBehaviour::Update() {
  Runtime::Instance().Update();
}

void RuntimeBehaviour::OnApplicationPause(bool paused) {
  Runtime::Instance().SetApplicationPaused(paused);
}

void RuntimeBehaviour::OnApplicationFocus(bool focused) {
  Runtime::Instance().SetApplicationFocused(focused);
}

void RuntimeBehaviour::OnDestroy() {
  Runtime::Instance().OnBehaviourDestroyed(this);
}

void CameraApplier::OnPreCull() {
  auto& runtime = Runtime::Instance();
  if (IsMenuMainCamera(this) || !runtime.IsCameraApplierCurrent(this)) return;
  runtime.RestoreSecondaryCullingLayers();
}

void CameraApplier::OnPreRender() {

  auto& runtime = Runtime::Instance();
  if (IsMenuMainCamera(this) || !runtime.IsCameraApplierCurrent(this)) return;
  runtime.BindSecondaryCameraTextures();
  runtime.ApplySecondaryCameraMainEffects(mainEffectController);

  auto go = get_gameObject();
  auto* camera = IsManagedAlive(go.unsafePtr()) ? go->GetComponent<UnityEngine::Camera*>() : nullptr;
  runtime.AddMidRenderCommandBuffers(camera, this);
}

void CameraApplier::OnPostRender() {
  // Mid-render buffers are immutable for a given generation/render signature
  // and remain attached across identical frames. They are rebuilt when an
  // event, texture, camera or descriptor changes, and are still removed
  // synchronously by OnDisable/OnDestroy and every runtime reset.
}

void CameraApplier::OnDisable() {
  // Unity may disable the gameplay camera without delivering the matching
  // OnPostRender callback (level completion, retry and menu transitions all do
  // this). Remove authored command buffers synchronously so they cannot retain
  // map materials into a later menu/result render.
  Runtime::Instance().RemoveMidRenderCommandBuffers(this);
}

void CameraApplier::OnDestroy() {
  Runtime::Instance().OnCameraApplierDestroyed(this);
  sessionGeneration = 0;
  hasMainEffect = false;
  imageEffectController = nullptr;
  mainEffectController = nullptr;
}

void CameraApplier::OnRenderImage(UnityEngine::RenderTexture* src, UnityEngine::RenderTexture* dest) {
  auto& runtime = Runtime::Instance();
  if (IsMenuMainCamera(this) || !runtime.BeginCameraRender(this)) {
    if (!runtime.CopyStereoRenderTexture(src, dest)) {
      UnityEngine::Graphics::Blit(src, dest);
    }
    return;
  }

  struct RenderScope final {
    Runtime& runtime;
    CameraApplier* owner;
    ~RenderScope() { runtime.EndCameraRender(owner); }
  } renderScope{runtime, this};

  runtime.CacheMainRenderDescriptor(src);

  // Desktop Vivify routes Beat Saber's registered ImageEffect callback between
  // the authored BeforeMainEffect and AfterMainEffect chains. Calling the
  // MainEffectSO directly bypasses other effects registered with this
  // controller and also lets the original OnRenderImage run a second time.
  auto* mainEffectCallback =
      hasMainEffect && IsManagedAlive(imageEffectController)
          ? imageEffectController->__cordl_internal_get__renderImageCallback()
          : nullptr;
  if (mainEffectCallback != nullptr) {
    if (runtime.IsResetting() || GetDisableAllBlits() ||
        (runtime.GetPreEffectsEmpty() && runtime.GetPostEffectsEmpty())) {
      mainEffectCallback->Invoke(src, dest);
      return;
    }
    auto desc = src->get_descriptor();
    desc.set_msaaSamples(1);
    desc.set_depthBufferBits(0);
    auto temp = UnityEngine::RenderTexture::GetTemporary(desc);
    auto temp2 = UnityEngine::RenderTexture::GetTemporary(desc);

    bool const tempValid = IsManagedAlive(temp.unsafePtr());
    bool const temp2Valid = IsManagedAlive(temp2.unsafePtr());

    if (tempValid && temp2Valid) {
      runtime.ApplyBlits(src, temp.unsafePtr(), cameraName, 1);
      mainEffectCallback->Invoke(temp.unsafePtr(), temp2.unsafePtr());
      runtime.ApplyBlits(temp2.unsafePtr(), dest, cameraName, 2);
    } else if (tempValid) {
      runtime.ApplyBlits(src, temp.unsafePtr(), cameraName, 1);
      mainEffectCallback->Invoke(temp.unsafePtr(), dest);
      // Reuse the pre-effect temporary only after the main effect finished.
      // Never call Graphics.Blit with the same RenderTexture as source and
      // destination: Unity's GLES backend can retain a destroyed surface and
      // later crash while releasing it during the next level selection.
      runtime.ApplyBlits(dest, temp.unsafePtr(), cameraName, 2);
      if (!runtime.CopyStereoRenderTexture(temp.unsafePtr(), dest)) {
        UnityEngine::Graphics::Blit(temp.unsafePtr(), dest);
      }
    } else {
      mainEffectCallback->Invoke(src, dest);
      // Allocation failure means the device is already under pressure. Keep
      // the valid Beat Saber frame and skip optional post effects rather than
      // attempting an undefined self-blit.
    }

    if (tempValid) {
      UnityEngine::RenderTexture::ReleaseTemporary(temp.unsafePtr());
    }
    if (temp2Valid) {
      UnityEngine::RenderTexture::ReleaseTemporary(temp2.unsafePtr());
    }
    return;
  }
  runtime.ApplyBlits(src, dest, cameraName, 0);
}

}
