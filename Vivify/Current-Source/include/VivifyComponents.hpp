#pragma once
#include "VivifyTypes.hpp"
#include "VivifyQuestPerformance.hpp"
#include "UnityEngine/Rendering/CommandBuffer.hpp"
#include "UnityEngine/Rendering/RenderTargetIdentifier.hpp"
#include "UnityEngine/Rendering/BuiltinRenderTextureType.hpp"
#include "beatsaber-hook/shared/utils/typedefs-wrappers.hpp"

DECLARE_CLASS_CODEGEN_INTERFACES(Vivify, OffsetBladeMovementData, System::Object, GlobalNamespace::IBladeMovementData*) {
public:
  DECLARE_DEFAULT_CTOR();
  DECLARE_SIMPLE_DTOR();
  DECLARE_INSTANCE_METHOD(void, Init, GlobalNamespace::IBladeMovementData* followed, UnityEngine::Transform* parent,
                          UnityEngine::Vector3 topPos, UnityEngine::Vector3 bottomPos);
  DECLARE_OVERRIDE_METHOD_MATCH(float_t, get_bladeSpeed, &GlobalNamespace::IBladeMovementData::get_bladeSpeed);
  DECLARE_OVERRIDE_METHOD_MATCH(GlobalNamespace::BladeMovementDataElement, get_lastAddedData,
                                &GlobalNamespace::IBladeMovementData::get_lastAddedData);
  DECLARE_OVERRIDE_METHOD_MATCH(GlobalNamespace::BladeMovementDataElement, get_prevAddedData,
                                &GlobalNamespace::IBladeMovementData::get_prevAddedData);
private:
  GlobalNamespace::IBladeMovementData* _followed = nullptr;
  UnityEngine::Transform* _parent = nullptr;
  UnityEngine::Vector3 _topPos = UnityEngine::Vector3(0.0f, 0.0f, 1.0f);
  UnityEngine::Vector3 _bottomPos = UnityEngine::Vector3(0.0f, 0.0f, 0.0f);

  GlobalNamespace::BladeMovementDataElement Modify(GlobalNamespace::BladeMovementDataElement original);
};

DECLARE_CLASS_CODEGEN(Vivify, FollowedSaberTrail, GlobalNamespace::SaberTrail) {
public:
  DECLARE_DEFAULT_CTOR();
  DECLARE_SIMPLE_DTOR();
  DECLARE_INSTANCE_METHOD(void, Awake);
  DECLARE_INSTANCE_METHOD(void, InitFollowed, GlobalNamespace::SaberTrail* followed, UnityEngine::Transform* parent,
                          UnityEngine::Material* material, UnityEngine::Vector3 topPos,
                          UnityEngine::Vector3 bottomPos, float_t duration, int32_t samplingFrequency,
                          int32_t granularity);
  DECLARE_INSTANCE_METHOD(void, Update);
  DECLARE_INSTANCE_METHOD(void, LateUpdate);
  DECLARE_INSTANCE_METHOD(void, Cleanup);

private:
  GlobalNamespace::SaberTrail* _followed = nullptr;
  Vivify::OffsetBladeMovementData* _offsetMovementData = nullptr;
  SafePtr<UnityEngine::MaterialPropertyBlock> _colorPropertyBlock;
  UnityEngine::Color _lastAppliedColor;
  bool _hasLastAppliedColor = false;
};

DECLARE_CLASS_CODEGEN(Vivify, RuntimeBehaviour, UnityEngine::MonoBehaviour) {
  DECLARE_DEFAULT_CTOR();
  DECLARE_SIMPLE_DTOR();
  DECLARE_INSTANCE_METHOD(void, Update);
  DECLARE_INSTANCE_METHOD(void, OnApplicationPause, bool paused);
  DECLARE_INSTANCE_METHOD(void, OnApplicationFocus, bool focused);
  DECLARE_INSTANCE_METHOD(void, OnDestroy);
};

DECLARE_CLASS_CODEGEN(Vivify, CullingCameraController, UnityEngine::MonoBehaviour) {
public:
  DECLARE_DEFAULT_CTOR();
  DECLARE_SIMPLE_DTOR();
  DECLARE_INSTANCE_METHOD(void, OnPreCull);
  DECLARE_INSTANCE_METHOD(void, OnPostRender);
  DECLARE_INSTANCE_METHOD(void, OnDisable);

public:

  void CullingPreCull();
  void CullingPostRender();

  void SetCullingData(bool whitelist, std::vector<TrackW> const& tracks) {
    // Camera properties can be refreshed after authored events or a camera
    // replacement. Keeping the existing state when the data is unchanged avoids
    // a dense-map renderer rescan when the same property is reasserted.
    if (_hasCullingData && _whitelist == whitelist && _tracks == tracks) return;

    CullingPostRender();
    _whitelist = whitelist;
    _tracks = tracks;
    _hasCullingData = true;
    _rendererCache.clear();
    _seenTrackedRoots.clear();
    _seenRendererGameObjects.clear();
    _diagLogCount = 0;
    _stallLogCount = 0;
  }

  void ClearCullingData() {
    if (!_hasCullingData && !_cachedMask.has_value() && _cachedLayers.empty()) return;

    CullingPostRender();
    _whitelist = false;
    _tracks.clear();
    _rendererCache.clear();
    _seenTrackedRoots.clear();
    _seenRendererGameObjects.clear();
    _hasCullingData = false;
  }

  bool HasCullingData() const { return _hasCullingData; }

private:
  struct RendererCacheEntry {
    int directChildCount = -1;
    int nextRefreshFrame = -1000;
    std::vector<UnityEngine::Renderer*> renderers;
  };

  UnityEngine::Camera* _camera = nullptr;
  std::optional<int> _cachedMask;
  bool _whitelist = false;
  std::vector<TrackW> _tracks;

  std::vector<std::pair<UnityEngine::GameObject*, int>> _cachedLayers;
  std::unordered_map<UnityEngine::GameObject*, RendererCacheEntry> _rendererCache;
  std::unordered_set<UnityEngine::GameObject*> _seenTrackedRoots;
  std::unordered_set<UnityEngine::GameObject*> _seenRendererGameObjects;
  bool _hasCullingData = false;
  int _diagLogCount = 0;
  int _stallLogCount = 0;
};

DECLARE_CLASS_CODEGEN(Vivify, MultipassKeywordController, UnityEngine::MonoBehaviour) {
public:
  DECLARE_DEFAULT_CTOR();
  DECLARE_SIMPLE_DTOR();
  DECLARE_INSTANCE_METHOD(void, Awake);
  DECLARE_INSTANCE_METHOD(void, OnPreRender);
  DECLARE_INSTANCE_METHOD(void, OnDisable);

private:
  UnityEngine::Camera* _camera = nullptr;
};

DECLARE_CLASS_CODEGEN(Vivify, SecondaryCameraController, UnityEngine::MonoBehaviour) {
public:
  DECLARE_DEFAULT_CTOR();
  DECLARE_SIMPLE_DTOR();
  DECLARE_INSTANCE_METHOD(void, OnPreCull);
  DECLARE_INSTANCE_METHOD(void, OnPostRender);
  DECLARE_INSTANCE_METHOD(void, OnRenderImage, UnityEngine::RenderTexture* src, UnityEngine::RenderTexture* dest);
  DECLARE_INSTANCE_METHOD(void, OnDisable);
  DECLARE_INSTANCE_METHOD(void, OnDestroy);

public:
  UnityEngine::Camera* GetCamera();
  std::string cameraName;

private:
  UnityEngine::Camera* _camera = nullptr;
  bool _stereoMatricesApplied = false;
  int _stereoSyncLogCount = 0;
  int _stereoSyncFailureLogCount = 0;
};

DECLARE_CLASS_CODEGEN(Vivify, CameraApplier, UnityEngine::MonoBehaviour) {
  DECLARE_DEFAULT_CTOR();
  DECLARE_SIMPLE_DTOR();
  DECLARE_INSTANCE_METHOD(void, OnPreCull);
  DECLARE_INSTANCE_METHOD(void, OnPreRender);
  DECLARE_INSTANCE_METHOD(void, OnPostRender);
  DECLARE_INSTANCE_METHOD(void, OnRenderImage, UnityEngine::RenderTexture* src, UnityEngine::RenderTexture* dest);
  DECLARE_INSTANCE_METHOD(void, OnDisable);
  DECLARE_INSTANCE_METHOD(void, OnDestroy);
public:
  std::string cameraName = "_Main";
  std::uint64_t sessionGeneration = 0;
  bool hasMainEffect = false;
  GlobalNamespace::ImageEffectController* imageEffectController = nullptr;
  GlobalNamespace::MainEffectController* mainEffectController = nullptr;
};

namespace Vivify {

void SetMultipassShaderState(bool enabled,
                             UnityEngine::XR::XRSettings_StereoRenderingMode stereoMode =
                                 UnityEngine::XR::XRSettings_StereoRenderingMode::MultiPass,
                             int eye = 0);
void SetMultipassShaderStateForCamera(UnityEngine::Camera* camera);
}
