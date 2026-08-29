#include "VivifyRuntimeInternal.hpp"
#include "VivifyComponents.hpp"
#include "UnityEngine/Rendering/CameraEvent.hpp"
#include "UnityEngine/Rendering/TextureDimension.hpp"
#include "UnityEngine/Graphics.hpp"
#include "UnityEngine/Shader.hpp"
#include "UnityEngine/Vector2.hpp"

namespace Vivify {

namespace {
UnityEngine::Rendering::RenderTargetIdentifier ToTargetId(UnityEngine::Texture* texture) {
  return UnityEngine::Rendering::RenderTargetIdentifier::op_Implicit___UnityEngine__Rendering__RenderTargetIdentifier(texture);
}
UnityEngine::Rendering::RenderTargetIdentifier CameraTargetId() {
  return UnityEngine::Rendering::RenderTargetIdentifier::op_Implicit___UnityEngine__Rendering__RenderTargetIdentifier(
      UnityEngine::Rendering::BuiltinRenderTextureType::CameraTarget);
}

bool UsesTextureArrayStereo() {
  if (!UnityEngine::XR::XRSettings::get_enabled()) return false;
  int const mode = UnityEngine::XR::XRSettings::get_stereoRenderingMode().value__;
  return mode == 2 || mode == 3;
}

void ConfigureStereoRenderTextureDescriptor(UnityEngine::RenderTextureDescriptor& descriptor,
                                            bool stereo) {
  if (!stereo) return;
  descriptor.set_vrUsage(UnityEngine::VRTextureUsage::TwoEyes);
  if (UsesTextureArrayStereo()) {
    descriptor.set_dimension(UnityEngine::Rendering::TextureDimension::Tex2DArray);
    descriptor.set_volumeDepth(2);
  }
}

bool IsTextureArray(UnityEngine::RenderTextureDescriptor descriptor) {
  return descriptor.get_dimension().value__ ==
             UnityEngine::Rendering::TextureDimension::Tex2DArray.value__ &&
         descriptor.get_volumeDepth() >= 2;
}

bool CanCopyStereoArray(UnityEngine::RenderTexture* source,
                        UnityEngine::RenderTexture* destination) {
  if (!IsManagedAlive(source) || !IsManagedAlive(destination)) return false;
  auto sourceDescriptor = source->get_descriptor();
  auto destinationDescriptor = destination->get_descriptor();
  return IsTextureArray(sourceDescriptor) && IsTextureArray(destinationDescriptor) &&
         sourceDescriptor.get_width() == destinationDescriptor.get_width() &&
         sourceDescriptor.get_height() == destinationDescriptor.get_height() &&
         sourceDescriptor.get_volumeDepth() == destinationDescriptor.get_volumeDepth() &&
         sourceDescriptor.get_graphicsFormat().value__ ==
             destinationDescriptor.get_graphicsFormat().value__ &&
         sourceDescriptor.get_msaaSamples() == 1 &&
         destinationDescriptor.get_msaaSamples() == 1;
}

bool CopyStereoArrayNow(UnityEngine::RenderTexture* source,
                        UnityEngine::RenderTexture* destination) {
  if (!CanCopyStereoArray(source, destination)) return false;
  try {
    // A regular full-screen Blit is allowed to address only the active layer
    // on Quest Multiview. A full CopyTexture retains both array elements and
    // is the correct operation for a material-free transfer.
    UnityEngine::Graphics::CopyTexture(source, destination);
    return true;
  } catch (...) {
    return false;
  }
}

bool QueueStereoArrayCopy(UnityEngine::Rendering::CommandBuffer* commandBuffer,
                          UnityEngine::RenderTexture* source,
                          UnityEngine::RenderTexture* destination) {
  if (commandBuffer == nullptr || !CanCopyStereoArray(source, destination)) return false;
  commandBuffer->CopyTexture(ToTargetId(source), ToTargetId(destination));
  return true;
}

bool QueueStereoMaterialBlit(UnityEngine::Rendering::CommandBuffer* commandBuffer,
                             UnityEngine::RenderTexture* source,
                             UnityEngine::RenderTexture* destination,
                             UnityEngine::Material* material, int pass) {
  if (commandBuffer == nullptr || !IsManagedAlive(source) ||
      !IsManagedAlive(destination) || !IsManagedAlive(material) ||
      !UsesTextureArrayStereo()) {
    return false;
  }

  auto sourceDescriptor = source->get_descriptor();
  auto destinationDescriptor = destination->get_descriptor();
  if (!IsTextureArray(sourceDescriptor) || !IsTextureArray(destinationDescriptor)) {
    return false;
  }

  // Most Vivify materials were authored as ordinary 2D post-processing
  // shaders (their bundles quite correctly report no stereo keyword).  A
  // regular CommandBuffer.Blit against Quest's Tex2DArray can then update
  // only the active/left slice.  Unity exposes the depth-slice overload for
  // exactly this case: run the same authored pass once per eye, preserving
  // the map's material/pass semantics without asking the shader to implement
  // Multiview itself.
  auto sourceId = ToTargetId(source);
  auto destinationId = ToTargetId(destination);
  UnityEngine::Vector2 const scale(1.0f, 1.0f);
  UnityEngine::Vector2 const offset(0.0f, 0.0f);
  commandBuffer->Blit_Identifier(sourceId, destinationId, material, pass,
                                 scale, offset, 0, 0);
  commandBuffer->Blit_Identifier(sourceId, destinationId, material, pass,
                                 scale, offset, 1, 1);
  return true;
}

int PerEyeBlitPropertyId() {
  static int id = UnityEngine::Shader::PropertyToID(u"_VivifyPerEyeBlit");
  return id;
}

int StereoActiveEyePropertyId() {
  static int id = UnityEngine::Shader::PropertyToID(u"_StereoActiveEye");
  return id;
}

// CopyTexture preserves the native Tex2DArray layout without running another
// full-screen fragment pass. Trust the layouts Unity actually created rather
// than the requested XR descriptor: Quest can return a mono secondary-camera
// source/destination even while the main camera is using Multiview.
bool CopyCapturedTexture(UnityEngine::RenderTexture* source,
                         UnityEngine::RenderTexture* destination,
                         std::string_view cameraName,
                         std::string_view textureKind,
                         int& failureLogCount) {
  if (!IsManagedAlive(source) || !IsManagedAlive(destination)) return false;
  try {
    auto sourceDescriptor = source->get_descriptor();
    auto destinationDescriptor = destination->get_descriptor();
    bool const sourceIsArray = IsTextureArray(sourceDescriptor);
    bool const destinationIsArray = IsTextureArray(destinationDescriptor);
    if (!sourceIsArray && destinationIsArray) {
      UnityEngine::Graphics::CopyTexture(source, 0, 0, destination, 0, 0);
      UnityEngine::Graphics::CopyTexture(source, 0, 0, destination, 1, 0);
    } else if (sourceIsArray && !destinationIsArray) {
      UnityEngine::Graphics::CopyTexture(source, 0, 0, destination, 0, 0);
    } else {
      UnityEngine::Graphics::CopyTexture(source, destination);
    }
    return true;
  } catch (std::exception const& ex) {
    if (failureLogCount < 3) {
      failureLogCount++;
      PaperLogger.warn("Vivify secondary {} copy failed: camera='{}' error={}",
                       std::string(textureKind), std::string(cameraName), ex.what());
    }
  } catch (...) {
    if (failureLogCount < 3) {
      failureLogCount++;
      PaperLogger.warn("Vivify secondary {} copy failed: camera='{}' non-std exception",
                       std::string(textureKind), std::string(cameraName));
    }
  }
  return false;
}
}  // namespace

bool Runtime::CopyStereoRenderTexture(UnityEngine::RenderTexture* src,
                                      UnityEngine::RenderTexture* dest) {
  return CopyStereoArrayNow(src, dest);
}

void Runtime::ApplyBlits(UnityEngine::RenderTexture* src, UnityEngine::RenderTexture* dest,
                         std::string const& cameraName, int phase) {
  if (!IsAlive(src) || (dest != nullptr && !IsAlive(dest))) return;

  bool const passthroughOnly = GetDisableAllBlits() || _isResetting || _pauseMenuActive ||
                               _currentBeatmapData == nullptr ||
                               (phase == 1 ? _preEffects.empty()
                                           : (phase == 2 ? _postEffects.empty()
                                                         : (_preEffects.empty() && _postEffects.empty())));
  auto destId = (dest != nullptr) ? ToTargetId(dest) : CameraTargetId();
  if (passthroughOnly) {
    if (!CopyStereoArrayNow(src, dest)) UnityEngine::Graphics::Blit(src, dest);
    return;
  }
  auto* main = EnsureCachedBlitTexture(_mainBlitTexture, src);
  auto* scratch = EnsureCachedBlitTexture(_scratchBlitTexture, src);
  if (!IsAlive(main) || !IsAlive(scratch)) {
    if (!CopyStereoArrayNow(src, dest)) UnityEngine::Graphics::Blit(src, dest);
    return;
  }

  auto* cb = AcquireImageBlitCommandBuffer();
  if (cb == nullptr) {
    if (!CopyStereoArrayNow(src, dest)) UnityEngine::Graphics::Blit(src, dest);
    return;
  }
  for (auto* temp : _imageSelfBlitTemps) {
    if (IsAlive(temp)) UnityEngine::RenderTexture::ReleaseTemporary(temp);
  }
  _imageSelfBlitTemps.clear();
  // Multiview-aware materials stay on the normal path. Ordinary authored
  // materials are redirected through QueueStereoMaterialBlit below so each
  // Quest eye receives the same post-processing pass.
  if (UsesTextureArrayStereo()) {
    cb->SetGlobalFloat(StereoActiveEyePropertyId(), 0.0f);
  }
  if (!QueueStereoArrayCopy(cb, src, main)) cb->Blit(ToTargetId(src), ToTargetId(main));
  auto* mainCurrent = main;
  auto* mainScratch = scratch;

  auto emitBlit = [&](UnityEngine::Rendering::RenderTargetIdentifier srcId,
                      UnityEngine::RenderTexture* sourceTexture,
                      UnityEngine::Rendering::RenderTargetIdentifier dstId,
    UnityEngine::RenderTexture* destinationTexture,
                      UnityEngine::Material* material, int pass) {
    if (material == nullptr) {
      if (!QueueStereoArrayCopy(cb, sourceTexture, destinationTexture)) cb->Blit(srcId, dstId);
    }
    else if (QueueStereoMaterialBlit(cb, sourceTexture, destinationTexture, material, pass)) {
      // The depth-slice overload above is the stereo-safe path for ordinary
      // (non-Multiview) Vivify materials on Quest.
    }
    else if (pass >= 0) cb->Blit(srcId, dstId, material, pass);
    else cb->Blit(srcId, dstId, material);
  };
  auto renderEffects = [&](std::vector<ActiveBlitEffect> const& effects) {
    for (auto it = effects.rbegin(); it != effects.rend(); ++it) {
      auto const& data = it->data;
      if (GetVivifyDebugLogging() && IsAlive(data.material) &&
          _loggedBlitStereoMaterials.emplace(data.material).second) {
        auto desc = src->get_descriptor();
        auto mainCamera = UnityEngine::Camera::get_main();
        auto* mainCameraPtr = mainCamera.unsafePtr();
        int const activeEye = IsAlive(mainCameraPtr) ? mainCameraPtr->get_stereoActiveEye().value__ : -1;
        PaperLogger.info(
            "Vivify Blit stereo runtime: camera='{}' asset='{}' mode={} eye={} size={}x{} vrUsage={} dimension={} slices={} global[multiview={} instancing={} singlePass={}] material[multiview={} instancing={} singlePass={} multipass={}]",
            cameraName, data.asset,
            UnityEngine::XR::XRSettings::get_stereoRenderingMode().value__, activeEye,
            desc.get_width(), desc.get_height(), src->get_vrUsage().value__,
            desc.get_dimension().value__, desc.get_volumeDepth(),
            BoolText(UnityEngine::Shader::IsKeywordEnabled(u"STEREO_MULTIVIEW_ON")),
            BoolText(UnityEngine::Shader::IsKeywordEnabled(u"STEREO_INSTANCING_ON")),
            BoolText(UnityEngine::Shader::IsKeywordEnabled(u"UNITY_SINGLE_PASS_STEREO")),
            BoolText(data.material->IsKeywordEnabled(u"STEREO_MULTIVIEW_ON")),
            BoolText(data.material->IsKeywordEnabled(u"STEREO_INSTANCING_ON")),
            BoolText(data.material->IsKeywordEnabled(u"UNITY_SINGLE_PASS_STEREO")),
            BoolText(data.material->IsKeywordEnabled(u"MULTIPASS_ENABLED")));
        if (data.material->HasProperty(PerEyeBlitPropertyId())) {
          PaperLogger.info(
              "Vivify Blit per-eye compatibility: camera='{}' asset='{}' requested={} arrayStereo={} sourceArray={}",
              cameraName, data.asset,
              BoolText(data.material->GetFloat(PerEyeBlitPropertyId()) > 0.5f),
              BoolText(UsesTextureArrayStereo()), BoolText(IsTextureArray(desc)));
        }
      }
      if (data.material != nullptr && !CanUseBlitMaterial(data.material, data.pass)) continue;
      bool const sourceIsMain = data.source == kMainCameraId;
      UnityEngine::RenderTexture* blitSrc = nullptr;
      if (sourceIsMain) blitSrc = mainCurrent;
      else if (auto found = _declaredTextures.find(data.source); found != _declaredTextures.end()) blitSrc = found->second.texture;
      else if (auto found = _secondaryCameras.find(data.source); found != _secondaryCameras.end()) blitSrc = SecondaryCameraColorRT(found->second);
      if (!IsAlive(blitSrc)) continue;
      auto blitSrcId = ToTargetId(blitSrc);
      for (auto const& targetName : data.targets) {
        if (targetName == kMainCameraId) {
          if (sourceIsMain) {
            emitBlit(blitSrcId, blitSrc, ToTargetId(mainScratch), mainScratch,
                     data.material, data.pass);
            std::swap(mainCurrent, mainScratch);
            blitSrc = mainCurrent;
            blitSrcId = ToTargetId(mainCurrent);
          } else {
            emitBlit(blitSrcId, blitSrc, ToTargetId(mainCurrent), mainCurrent,
                     data.material, data.pass);
          }
        } else if (auto found = _declaredTextures.find(targetName); found != _declaredTextures.end()) {
          auto* target = found->second.texture;
          if (!IsAlive(target)) continue;
          if (target == blitSrc) {
            auto desc = target->get_descriptor();
            auto temp = UnityEngine::RenderTexture::GetTemporary(desc);
            auto* tempPtr = temp.unsafePtr();
            if (!IsManagedAlive(tempPtr)) continue;
            _imageSelfBlitTemps.emplace_back(tempPtr);
            emitBlit(blitSrcId, blitSrc, ToTargetId(tempPtr), tempPtr,
                     data.material, data.pass);
            if (!QueueStereoArrayCopy(cb, tempPtr, target)) {
              cb->Blit(ToTargetId(tempPtr), ToTargetId(target));
            }
          } else {
            emitBlit(blitSrcId, blitSrc, ToTargetId(target), target,
                     data.material, data.pass);
          }
        }
      }
    }
  };
  if (phase == 0 || phase == 1) renderEffects(_preEffects);
  if (phase == 0 || phase == 2) renderEffects(_postEffects);

  if (dest == nullptr || !QueueStereoArrayCopy(cb, mainCurrent, dest)) {
    cb->Blit(ToTargetId(mainCurrent), destId);
  }
  UnityEngine::Graphics::ExecuteCommandBuffer(cb);
  for (auto* temp : _imageSelfBlitTemps) {
    if (IsAlive(temp)) UnityEngine::RenderTexture::ReleaseTemporary(temp);
  }
  _imageSelfBlitTemps.clear();
}

void Runtime::CacheMainRenderDescriptor(UnityEngine::RenderTexture* src) {
  if (!IsAlive(src)) return;
  auto desc = src->get_descriptor();

  desc.set_msaaSamples(1);
  desc.set_depthBufferBits(0);
  _cachedMainDescriptor = desc;
  _cachedMainVrUsage = src->get_vrUsage().value__;
  _hasMainDescriptor = true;
  EnsureDeclaredTextures();
}

void Runtime::EnsureDeclaredTextures() {
  if (!_hasMainDescriptor || _declaredTextures.empty()) return;

  int const maxTextureSize = std::max(1, UnityEngine::SystemInfo::get_maxTextureSize());
  for (auto& [name, data] : _declaredTextures) {
    auto descriptor = _cachedMainDescriptor;
    int width = std::clamp(data.width.value_or(descriptor.get_width()), 1, maxTextureSize);
    int height = std::clamp(data.height.value_or(descriptor.get_height()), 1, maxTextureSize);
    width = std::clamp(static_cast<int>(width / data.xRatio), 1, maxTextureSize);
    height = std::clamp(static_cast<int>(height / data.yRatio), 1, maxTextureSize);
    descriptor.set_width(width);
    descriptor.set_height(height);
    descriptor.set_msaaSamples(1);
    descriptor.set_depthBufferBits(0);
    if (data.format.has_value()) {
      descriptor.set_colorFormat(
          SupportedRenderTextureFormat(data.format.value(), "CreateScreenTexture:" + name));
    }

    bool recreate = !IsAlive(data.texture) || !data.texture->IsCreated();
    if (!recreate) {
      auto current = data.texture->get_descriptor();
      recreate = current.get_width() != descriptor.get_width() ||
                 current.get_height() != descriptor.get_height() ||
                 current.get_graphicsFormat().value__ != descriptor.get_graphicsFormat().value__ ||
                 current.get_depthStencilFormat().value__ != descriptor.get_depthStencilFormat().value__ ||
                 current.get_dimension().value__ != descriptor.get_dimension().value__ ||
                 current.get_volumeDepth() != descriptor.get_volumeDepth() ||
                 current.get_msaaSamples() != descriptor.get_msaaSamples() ||
                 data.texture->get_vrUsage().value__ != _cachedMainVrUsage;
    }
    if (recreate) {
      if (IsAlive(data.texture)) UnityEngine::Object::Destroy(data.texture);
      data.texture = UnityEngine::RenderTexture::New_ctor(descriptor);
      if (IsAlive(data.texture) && data.filterMode.has_value()) {
        data.texture->set_filterMode(data.filterMode.value());
      }
      if (!IsAlive(data.texture) || !data.texture->Create()) {
        if (GetVivifyDebugLogging()) {
          PaperLogger.warn(
              "Vivify declared texture allocation failed: id='{}' size={}x{} format={} dim={} slices={}",
              name, descriptor.get_width(), descriptor.get_height(),
              descriptor.get_colorFormat().value__, descriptor.get_dimension().value__,
              descriptor.get_volumeDepth());
        }
        if (IsAlive(data.texture)) UnityEngine::Object::Destroy(data.texture);
        data.texture = nullptr;
        UnityEngine::Shader::SetGlobalTexture(
            data.propertyId, static_cast<UnityEngine::Texture*>(nullptr));
        continue;
      }
      ClearRenderTexture(data.texture);
      if (GetVivifyDebugLogging()) {
        PaperLogger.info(
            "Vivify declared texture ready: id='{}' size={}x{} format={} dim={} slices={} vrUsage={} filter={}",
            name, descriptor.get_width(), descriptor.get_height(),
            descriptor.get_colorFormat().value__, descriptor.get_dimension().value__,
            descriptor.get_volumeDepth(), data.texture->get_vrUsage().value__,
            data.filterMode.has_value() ? data.filterMode->value__ : data.texture->get_filterMode().value__);
      }
    }
    UnityEngine::Shader::SetGlobalTexture(
        data.propertyId, static_cast<UnityEngine::Texture*>(data.texture));
  }
}

bool Runtime::EnsureMidRenderTextures() {
  if (!_hasMainDescriptor) return false;
  auto matchesMainDescriptor = [this](UnityEngine::RenderTexture* texture) {
    if (!IsAlive(texture) || !texture->IsCreated()) return false;
    auto actual = texture->get_descriptor();
    return actual.get_width() == _cachedMainDescriptor.get_width() &&
           actual.get_height() == _cachedMainDescriptor.get_height() &&
           actual.get_graphicsFormat().value__ == _cachedMainDescriptor.get_graphicsFormat().value__ &&
           actual.get_dimension().value__ == _cachedMainDescriptor.get_dimension().value__ &&
           actual.get_volumeDepth() == _cachedMainDescriptor.get_volumeDepth() &&
           texture->get_vrUsage().value__ == _cachedMainVrUsage;
  };
  bool const mismatch = !matchesMainDescriptor(_midMainRT) ||
                        !matchesMainDescriptor(_midScratchRT);
  if (!mismatch) return true;
  // Persistent command buffers retain these render-target identifiers across
  // frames. Detach them before replacing a descriptor-owned texture.
  RemoveMidRenderCommandBuffers();
  ReleaseMidRenderTextures();
  _midMainRT = UnityEngine::RenderTexture::New_ctor(_cachedMainDescriptor);
  _midScratchRT = UnityEngine::RenderTexture::New_ctor(_cachedMainDescriptor);
  if (!IsAlive(_midMainRT) || !IsAlive(_midScratchRT) || !_midMainRT->Create() || !_midScratchRT->Create()) {
    ReleaseMidRenderTextures();
    return false;
  }
  ClearRenderTexture(_midMainRT);
  ClearRenderTexture(_midScratchRT);
  return true;
}

void Runtime::ReleaseMidRenderTextures() {
  if (IsAlive(_midMainRT)) {
    UnityEngine::Object::Destroy(_midMainRT);
  }
  if (IsAlive(_midScratchRT)) {
    UnityEngine::Object::Destroy(_midScratchRT);
  }
  _midMainRT = nullptr;
  _midScratchRT = nullptr;
}

UnityEngine::Rendering::CommandBuffer* Runtime::AcquireImageBlitCommandBuffer() {
  if (!_imageBlitCommandBuffer) {
    auto* created = UnityEngine::Rendering::CommandBuffer::New_ctor();
    if (created == nullptr) return nullptr;
    _imageBlitCommandBuffer.emplace(created);
    _imageCommandBufferCreates++;
  }
  auto* commandBuffer = _imageBlitCommandBuffer.ptr();
  if (commandBuffer != nullptr) {
    commandBuffer->Clear();
    _imageBlitExecutions++;
  }
  return commandBuffer;
}

void Runtime::ReleaseImageBlitCommandBuffer(bool canTouchNative) {
  if (canTouchNative) {
    for (auto* temp : _imageSelfBlitTemps) {
      if (IsAlive(temp)) UnityEngine::RenderTexture::ReleaseTemporary(temp);
    }
  }
  _imageSelfBlitTemps.clear();
  if (_imageBlitCommandBuffer) {
    auto* commandBuffer = _imageBlitCommandBuffer.ptr();
    if (canTouchNative && commandBuffer != nullptr) {
      commandBuffer->ReleaseBuffer();
      commandBuffer->Dispose();
    }
    _imageBlitCommandBuffer.clear();
  }
}

UnityEngine::Rendering::CommandBuffer* Runtime::BuildMidRenderCommandBuffer(
    std::vector<ActiveBlitEffect> const& effects) {
  using RTI = UnityEngine::Rendering::RenderTargetIdentifier;
  auto* cb = UnityEngine::Rendering::CommandBuffer::New_ctor();
  if (cb == nullptr) return nullptr;
  // Same safety rule as the OnRenderImage chain: Unity's Multiview instance
  // ID selects the eye, while the legacy selector remains neutral.
  if (UsesTextureArrayStereo()) {
    cb->SetGlobalFloat(StereoActiveEyePropertyId(), 0.0f);
  }
  RTI srcId{};
  srcId._ctor(UnityEngine::Rendering::BuiltinRenderTextureType::CurrentActive);
  RTI dstId{};
  dstId._ctor(UnityEngine::Rendering::BuiltinRenderTextureType::CameraTarget);
  cb->Blit(srcId, ToTargetId(_midMainRT));

  auto* mainCurrent = _midMainRT;
  auto* mainScratch = _midScratchRT;

  auto emitBlit = [&](RTI blitSrc, UnityEngine::RenderTexture* sourceTexture,
                      RTI blitDst, UnityEngine::RenderTexture* destinationTexture,
                      UnityEngine::Material* material, int pass) {
    if (material == nullptr) {
      if (!QueueStereoArrayCopy(cb, sourceTexture, destinationTexture)) {
        cb->Blit(blitSrc, blitDst);
      }
    } else if (pass >= 0) {
      if (!QueueStereoMaterialBlit(cb, sourceTexture, destinationTexture, material, pass)) {
        cb->Blit(blitSrc, blitDst, material, pass);
      }
    } else {
      if (!QueueStereoMaterialBlit(cb, sourceTexture, destinationTexture, material, -1)) {
        cb->Blit(blitSrc, blitDst, material);
      }
    }
  };

  for (auto it = effects.rbegin(); it != effects.rend(); ++it) {
    auto const& data = it->data;
    if (data.material != nullptr && !CanUseBlitMaterial(data.material, data.pass)) continue;
    bool const sourceIsMain = data.source == kMainCameraId;
    UnityEngine::RenderTexture* blitSrcTexture = nullptr;
    if (sourceIsMain) {
      blitSrcTexture = mainCurrent;
    } else if (auto found = _declaredTextures.find(data.source); found != _declaredTextures.end()) {
      blitSrcTexture = found->second.texture;
    } else if (auto found = _secondaryCameras.find(data.source); found != _secondaryCameras.end()) {
      blitSrcTexture = SecondaryCameraColorRT(found->second);
    }
    if (!IsAlive(blitSrcTexture)) continue;
    auto blitSrcId = ToTargetId(blitSrcTexture);
    for (auto const& targetName : data.targets) {
      if (targetName == kMainCameraId) {
        if (sourceIsMain) {
          emitBlit(blitSrcId, blitSrcTexture, ToTargetId(mainScratch), mainScratch,
                   data.material, data.pass);
          std::swap(mainCurrent, mainScratch);
          blitSrcTexture = mainCurrent;
          blitSrcId = ToTargetId(mainCurrent);
        } else {

          emitBlit(blitSrcId, blitSrcTexture, ToTargetId(mainCurrent), mainCurrent,
                   data.material, data.pass);
        }
      } else if (auto found = _declaredTextures.find(targetName); found != _declaredTextures.end()) {
        auto* target = found->second.texture;
        if (!IsAlive(target)) continue;
        if (target == blitSrcTexture) {
          auto descriptor = target->get_descriptor();
          descriptor.set_msaaSamples(1);
          descriptor.set_depthBufferBits(0);
          auto* tempPtr = UnityEngine::RenderTexture::New_ctor(descriptor);
          if (!IsAlive(tempPtr) || !tempPtr->Create()) {
            if (IsAlive(tempPtr)) UnityEngine::Object::Destroy(tempPtr);
            continue;
          }
          ClearRenderTexture(tempPtr);
          _midSelfBlitTemps.emplace_back(tempPtr);
          emitBlit(blitSrcId, blitSrcTexture, ToTargetId(tempPtr), tempPtr,
                   data.material, data.pass);
          if (!QueueStereoArrayCopy(cb, tempPtr, target)) {
            cb->Blit(ToTargetId(tempPtr), ToTargetId(target));
          }
        } else {
          emitBlit(blitSrcId, blitSrcTexture, ToTargetId(target), target,
                   data.material, data.pass);
        }
      }
    }
  }

  cb->Blit(ToTargetId(mainCurrent), dstId);
  return cb;
}

std::size_t Runtime::ComputeMidRenderSignature() {
  std::size_t signature = 0xcbf29ce484222325ULL;
  auto combine = [&signature](std::size_t value) {
    signature ^= value + 0x9e3779b97f4a7c15ULL + (signature << 6) + (signature >> 2);
  };
  auto combineString = [&combine](std::string const& value) {
    combine(std::hash<std::string>{}(value));
  };
  auto resolveTexture = [this](std::string const& name) -> UnityEngine::RenderTexture* {
    if (name == kMainCameraId) return _midMainRT;
    if (auto found = _declaredTextures.find(name); found != _declaredTextures.end()) {
      return found->second.texture;
    }
    if (auto found = _secondaryCameras.find(name); found != _secondaryCameras.end()) {
      return SecondaryCameraColorRT(found->second);
    }
    return nullptr;
  };

  combine(static_cast<std::size_t>(_cachedMainDescriptor.get_width()));
  combine(static_cast<std::size_t>(_cachedMainDescriptor.get_height()));
  combine(static_cast<std::size_t>(_cachedMainDescriptor.get_graphicsFormat().value__));
  combine(static_cast<std::size_t>(_cachedMainDescriptor.get_dimension().value__));
  combine(static_cast<std::size_t>(_cachedMainDescriptor.get_volumeDepth()));
  combine(static_cast<std::size_t>(_cachedMainVrUsage));
  combine(reinterpret_cast<std::uintptr_t>(_midMainRT));
  combine(reinterpret_cast<std::uintptr_t>(_midScratchRT));

  for (std::size_t bucket = 0; bucket < _midEffects.size(); ++bucket) {
    combine(bucket);
    combine(_midEffects[bucket].size());
    for (auto const& effect : _midEffects[bucket]) {
      auto const& data = effect.data;
      combine(reinterpret_cast<std::uintptr_t>(data.material));
      combine(static_cast<std::size_t>(data.pass));
      combine(static_cast<std::size_t>(data.priority));
      combine(std::hash<float>{}(effect.expireTime));
      combineString(data.source);
      combine(reinterpret_cast<std::uintptr_t>(resolveTexture(data.source)));
      combine(data.targets.size());
      for (auto const& target : data.targets) {
        combineString(target);
        combine(reinterpret_cast<std::uintptr_t>(resolveTexture(target)));
      }
    }
  }
  return signature;
}

void Runtime::AddMidRenderCommandBuffers(UnityEngine::Camera* camera, CameraApplier* owner) {
  if (!IsCameraApplierCurrent(owner)) return;
  if (!IsAlive(camera) || GetDisableAllBlits() ||
      _isResetting || _pauseMenuActive || _currentBeatmapData == nullptr ||
      !HasMidRenderEffects()) {
    RemoveMidRenderCommandBuffers();
    return;
  }

  if (!EnsureMidRenderTextures()) {
    RemoveMidRenderCommandBuffers();
    return;
  }

  std::size_t const signature = ComputeMidRenderSignature();
  if (_midCommandBufferCamera == camera && _midCommandBufferOwner == owner &&
      !_activeMidCommandBuffers.empty() &&
      _midRenderCommandCache.Matches(_lifecycle.Generation(),
                                     reinterpret_cast<std::uintptr_t>(owner), signature)) {
    _midCommandBufferCacheHits++;
    return;
  }

  RemoveMidRenderCommandBuffers();

  static constexpr std::array<int32_t, kMidRenderOrderCount> kOrderEvents = {
      static_cast<int32_t>(UnityEngine::Rendering::CameraEvent::__CameraEvent_Unwrapped::__E_BeforeSkybox),
      static_cast<int32_t>(UnityEngine::Rendering::CameraEvent::__CameraEvent_Unwrapped::__E_AfterSkybox),
      static_cast<int32_t>(UnityEngine::Rendering::CameraEvent::__CameraEvent_Unwrapped::__E_BeforeForwardOpaque),
      static_cast<int32_t>(UnityEngine::Rendering::CameraEvent::__CameraEvent_Unwrapped::__E_AfterForwardOpaque),
      static_cast<int32_t>(UnityEngine::Rendering::CameraEvent::__CameraEvent_Unwrapped::__E_BeforeForwardAlpha),
      static_cast<int32_t>(UnityEngine::Rendering::CameraEvent::__CameraEvent_Unwrapped::__E_AfterForwardAlpha),
  };
  for (int bucket = 0; bucket < kMidRenderOrderCount; bucket++) {
    auto const& effects = _midEffects[bucket];
    if (effects.empty()) continue;
    auto* cb = BuildMidRenderCommandBuffer(effects);
    if (cb == nullptr) continue;
    camera->AddCommandBuffer(UnityEngine::Rendering::CameraEvent(kOrderEvents[bucket]), cb);

    _activeMidCommandBuffers.emplace_back(kOrderEvents[bucket],
                                          SafePtr<UnityEngine::Rendering::CommandBuffer>(cb));
  }
  _midCommandBufferCamera = camera;
  _midCommandBufferOwner = owner;
  if (!_activeMidCommandBuffers.empty()) {
    _midRenderCommandCache.Commit(_lifecycle.Generation(),
                                  reinterpret_cast<std::uintptr_t>(owner), signature);
    _midCommandBufferRebuilds++;
  }
}

void Runtime::RemoveMidRenderCommandBuffers(CameraApplier* owner) {
  if (owner != nullptr && owner != _midCommandBufferOwner) return;
  for (auto& [cameraEvent, cb] : _activeMidCommandBuffers) {
    if (!cb) continue;
    if (IsAlive(_midCommandBufferCamera)) {
      _midCommandBufferCamera->RemoveCommandBuffer(UnityEngine::Rendering::CameraEvent(cameraEvent), cb.ptr());
    }
    cb->Dispose();
  }
  _activeMidCommandBuffers.clear();
  _midCommandBufferCamera = nullptr;
  _midCommandBufferOwner = nullptr;
  _midRenderCommandCache.Invalidate();

  for (auto* temp : _midSelfBlitTemps) {
    if (IsAlive(temp)) UnityEngine::Object::Destroy(temp);
  }
  _midSelfBlitTemps.clear();
}

UnityEngine::RenderTexture* Runtime::EnsureCachedBlitTexture(UnityEngine::RenderTexture*& texture,
                                                             UnityEngine::RenderTexture* src) {
  auto desc = src->get_descriptor();
  desc.set_msaaSamples(1);
  desc.set_depthBufferBits(0);
  if (IsAlive(texture)) {
    auto actual = texture->get_descriptor();
    bool const matches = texture->IsCreated() &&
                         actual.get_width() == desc.get_width() &&
                         actual.get_height() == desc.get_height() &&
                         actual.get_graphicsFormat().value__ == desc.get_graphicsFormat().value__ &&
                         actual.get_dimension().value__ == desc.get_dimension().value__ &&
                         actual.get_volumeDepth() == desc.get_volumeDepth() &&
                         texture->get_vrUsage().value__ == src->get_vrUsage().value__;
    if (matches) {
      return texture;
    }
    ReleaseRenderTexture(texture);
  }
  texture = UnityEngine::RenderTexture::New_ctor(desc);
  if (!IsAlive(texture)) { texture = nullptr; return nullptr; }
  if (!texture->Create()) { ReleaseRenderTexture(texture); return nullptr; }
  ClearRenderTexture(texture);
  return texture;
}

void Runtime::ReleaseRenderTexture(UnityEngine::RenderTexture*& texture) {
  if (IsAlive(texture)) {
    UnityEngine::Object::Destroy(texture);
  }
  texture = nullptr;
}

void Runtime::ReleaseCachedBlitTextures() {
  ReleaseRenderTexture(_mainBlitTexture);
  ReleaseRenderTexture(_scratchBlitTexture);
  ReleaseImageBlitCommandBuffer(true);
}

bool Runtime::CanUseBlitMaterial(UnityEngine::Material* material, int pass) const {
  if (!IsAlive(material)) {
    return false;
  }
  auto cachedShaderValidity = _blitMaterialValidCache.find(material);
  bool const shaderValidityCached =
      cachedShaderValidity != _blitMaterialValidCache.end();
  if (cachedShaderValidity != _blitMaterialValidCache.end() &&
      !cachedShaderValidity->second) {
    return false;
  }
  if (_genericFallbackMaterials.contains(material)) {
    _blitMaterialValidCache[material] = false;
    if (GetVivifyDebugLogging() && !shaderValidityCached) {
      PaperLogger.warn(
          "Vivify blit skipped: material '{}' uses a generic visible-object fallback",
          ToStdString(material->get_name()));
    }
    return false;
  }
  if (!shaderValidityCached) {
    auto shader = material->get_shader();
    auto* rawShader = shader.unsafePtr();
    auto shaderName = ShaderNameForLog(rawShader);
    if (!IsAlive(rawShader) || !rawShader->get_isSupported() ||
        IsInternalErrorShaderName(shaderName)) {
      if (GetVivifyDebugLogging()) {
        PaperLogger.warn("Vivify blit material invalid: material='{}' shader='{}' supported={} internalError={} pass={}",
                         ToStdString(material->get_name()), shaderName,
                         BoolText(IsAlive(rawShader) && rawShader->get_isSupported()),
                         BoolText(IsInternalErrorShaderName(shaderName)), pass);
      }
      _blitMaterialValidCache[material] = false;
      return false;
    }
    _blitMaterialValidCache[material] = true;
  }
  int const passCount = material->get_passCount();
  if (passCount <= 0 || (pass >= 0 && pass >= passCount)) {
    std::size_t warningKey = std::hash<UnityEngine::Material*>{}(material);
    warningKey ^= std::hash<int>{}(pass) + 0x9e3779b9U +
                  (warningKey << 6U) + (warningKey >> 2U);
    if (GetVivifyDebugLogging() &&
        _warnedInvalidBlitPasses.emplace(warningKey).second) {
      PaperLogger.warn("Vivify blit material invalid pass: material='{}' pass={} passCount={}",
                       ToStdString(material->get_name()), pass, passCount);
    }
    return false;
  }
  return true;
}

PostProcessingOrder Runtime::ParsePostProcessingOrder(rapidjson::Value const& json) {
  auto order = ReadStringView(json, "order");
  if (!order.has_value()) return PostProcessingOrder::AfterMainEffect;
  std::string normalized = NormalizeAssetKey(*order);
  if (normalized == "beforemaineffect") return PostProcessingOrder::BeforeMainEffect;
  if (normalized == "beforeskybox") return PostProcessingOrder::BeforeSkybox;
  if (normalized == "afterskybox") return PostProcessingOrder::AfterSkybox;
  if (normalized == "beforeopaque") return PostProcessingOrder::BeforeOpaque;
  if (normalized == "afteropaque") return PostProcessingOrder::AfterOpaque;
  if (normalized == "beforealpha") return PostProcessingOrder::BeforeAlpha;
  if (normalized == "afteralpha") return PostProcessingOrder::AfterAlpha;
  return PostProcessingOrder::AfterMainEffect;
}

void Runtime::AddOrUpdateBlitEffect(std::vector<ActiveBlitEffect>& effects, ActiveBlitEffect effect) {
  // Identical events are distinct authored passes in desktop Vivify. Keeping
  // only one changes compositing strength and breaks maps that intentionally
  // stack the same material/pass more than once.
  // Desktop inserts a new equal-priority pass before older ones, then renders
  // the list in reverse. lower_bound preserves that exact chronological order.
  auto position = std::lower_bound(
      effects.begin(), effects.end(), effect,
      [](ActiveBlitEffect const& active, ActiveBlitEffect const& incoming) {
        return active.data.priority < incoming.data.priority;
      });
  effects.insert(position, std::move(effect));
}

void Runtime::HandleBlit(CustomJSONData::CustomEventData* customEventData, rapidjson::Value const& json) {
  auto asset = ReadStringView(json, "asset");
  std::string normalizedAsset = asset.has_value() ? NormalizeAssetKey(*asset) : std::string();
  if (GetDisableAllBlits()) {
    if (GetVivifyDebugLogging()) {
      PaperLogger.info("Vivify Blit skipped at time {}: Blit effects disabled asset='{}'",
                       customEventData->time,
                       asset.has_value() ? std::string(*asset) : std::string("<none>"));
    }
    return;
  }
  if (asset.has_value() && GetDisableBeat0FilmgrainBlit() && std::fabs(customEventData->time) < 0.01f &&
      normalizedAsset.find("filmgrain") != std::string::npos) {
    if (GetVivifyDebugLogging()) {
      PaperLogger.info("Vivify Blit skipped at time {}: beat-0 filmgrain isolation asset='{}'",
                       customEventData->time, std::string(*asset));
    }
    return;
  }
  float const durationBeats = ReadFloat(json, "duration").value_or(0.0f);
  float const duration = DurationBeatsToSeconds(durationBeats);
  UnityEngine::Material* material = nullptr;
  if (asset.has_value()) {
    material = GetAssetAs<UnityEngine::Material>(*asset);
    if (material == nullptr) {
      if (GetVivifyDebugLogging()) {
        PaperLogger.warn("Vivify Blit material load failed at time {}: asset='{}'",
                         customEventData->time, std::string(*asset));
      }
      return;
    }
    RepairMaterialShader(material, *asset);
    LogMaterialShader("Blit", *asset, material);
    auto properties = ParseMaterialProperties(json);
    if (!properties.empty()) {
      Functions easing = ParseEasing(ReadStringView(json, "easing").value_or("easeLinear"));
      float const startTime = customEventData->time;
      float const currentSongTime = CurrentSongTime();
      bool const completed = duration <= 0.0f || startTime + duration <= currentSongTime;
      float const initialProgress = completed ? 1.0f : 0.0f;
      for (auto const& property : properties) ApplyMaterialProperty(material, property, initialProgress);
      if (!completed) {
        QueueMaterialPropertyAnimation(material, properties, startTime, duration, easing);
      }
    }
  }
  int const priority = ReadInt(json, "priority").value_or(0);
  int const pass = ReadInt(json, "pass").value_or(-1);
  std::string sourceStr{ReadStringView(json, "source").value_or(kMainCameraId)};
  std::vector<std::string> targets = ReadStringListOrSingle(json, "destination");
  if (targets.empty()) targets.emplace_back(kMainCameraId);
  PostProcessingOrder order = ParsePostProcessingOrder(json);
  auto& effects = [&]() -> std::vector<ActiveBlitEffect>& {
    switch (order) {
      case PostProcessingOrder::BeforeMainEffect: return _preEffects;
      case PostProcessingOrder::AfterMainEffect: return _postEffects;
      default: return _midEffects[static_cast<int>(order)];
    }
  }();
  BlitMaterialData bd{material, priority, std::move(sourceStr), std::move(targets), pass,
                      std::nullopt, std::move(normalizedAsset), customEventData->time};
  if (GetVivifyDebugLogging()) {
    PaperLogger.info("Vivify Blit event: time={} asset='{}' source='{}' targets={} priority={} pass={} order={} durationBeats={}",
                     customEventData->time,
                     asset.has_value() ? std::string(*asset) : std::string("<none>"),
                     bd.source, bd.targets.size(), priority, pass,
                     static_cast<int>(order), durationBeats);
  }
  float const songTime = CurrentSongTime();
  if (durationBeats == 0.0f) {

    bd.frame = static_cast<int>(UnityEngine::Time::get_frameCount());
    AddOrUpdateBlitEffect(effects, ActiveBlitEffect{std::move(bd), 0.0f});
  } else if (durationBeats > 0.0f && songTime <= customEventData->time + duration) {
    AddOrUpdateBlitEffect(effects, ActiveBlitEffect{std::move(bd), customEventData->time + duration});
  }

}

void Runtime::UpdateBlitEffects() {
  if (GetDisableAllBlits()) {
    if ((!_preEffects.empty() || !_postEffects.empty() || HasMidRenderEffects()) && GetVivifyDebugLogging()) {
      PaperLogger.info("Vivify Blit effects cleared: Blit effects disabled pre={} post={}",
                       _preEffects.size(), _postEffects.size());
    }
    _preEffects.clear();
    _postEffects.clear();
    for (auto& bucket : _midEffects) bucket.clear();
    return;
  }
  float const songTime = CurrentSongTime();
  int const frame = static_cast<int>(UnityEngine::Time::get_frameCount());
  bool const dropBeat0Filmgrain = GetDisableBeat0FilmgrainBlit();
  auto cleanup = [&](std::vector<ActiveBlitEffect>& effects) {
    effects.erase(std::remove_if(effects.begin(), effects.end(), [&](ActiveBlitEffect const& e) {
      if (dropBeat0Filmgrain && std::fabs(e.data.eventBeat) < 0.01f &&
          e.data.asset.find("filmgrain") != std::string::npos) {
        return true;
      }
      if (e.data.frame.has_value()) return e.data.frame.value() != frame;
      return e.expireTime > 0.0f && songTime >= e.expireTime;
    }), effects.end());
  };
  cleanup(_preEffects);
  cleanup(_postEffects);
  for (auto& bucket : _midEffects) cleanup(bucket);
}

void Runtime::HandleCreateScreenTexture(rapidjson::Value const& json) {
  auto id = ReadStringView(json, "id");
  if (!id.has_value()) return;
  std::string name(*id);
  // Desktop stores declarations with Dictionary.Add. A duplicate declaration
  // is rejected and leaves the first texture alive; silently replacing it can
  // invalidate Blits that still reference the original allocation.
  if (_declaredTextures.contains(name)) {
    PaperLogger.warn("Vivify CreateScreenTexture ignored duplicate id '{}'", name);
    return;
  }
  DeclaredTextureData dt;
  dt.name = name;
  dt.propertyId = UnityEngine::Shader::PropertyToID(StringW(name));
  dt.xRatio = ReadFloat(json, "xRatio").value_or(1.0f);
  dt.yRatio = ReadFloat(json, "yRatio").value_or(1.0f);
  if (auto w = ReadInt(json, "width")) dt.width = *w;
  if (auto h = ReadInt(json, "height")) dt.height = *h;
  auto formatName = ReadStringView(json, "format");
  if (!formatName.has_value()) formatName = ReadStringView(json, "colorFormat");
  if (formatName.has_value()) {
    auto fmt = formatName;
    dt.format = [&]() -> std::optional<UnityEngine::RenderTextureFormat> {
      auto s = *fmt;
      if (s == "ARGB32") return UnityEngine::RenderTextureFormat::ARGB32;
      if (s == "Depth") return UnityEngine::RenderTextureFormat::Depth;
      if (s == "ARGBHalf") return UnityEngine::RenderTextureFormat::ARGBHalf;
      if (s == "Shadowmap") return UnityEngine::RenderTextureFormat::Shadowmap;
      if (s == "RGB565") return UnityEngine::RenderTextureFormat::RGB565;
      if (s == "ARGB4444") return UnityEngine::RenderTextureFormat::ARGB4444;
      if (s == "ARGB1555") return UnityEngine::RenderTextureFormat::ARGB1555;
      if (s == "Default") return UnityEngine::RenderTextureFormat::Default;
      if (s == "ARGB2101010") return UnityEngine::RenderTextureFormat::ARGB2101010;
      if (s == "ARGB64") return UnityEngine::RenderTextureFormat::ARGB64;
      if (s == "ARGBFloat") return UnityEngine::RenderTextureFormat::ARGBFloat;
      if (s == "RGFloat") return UnityEngine::RenderTextureFormat::RGFloat;
      if (s == "RGHalf") return UnityEngine::RenderTextureFormat::RGHalf;
      if (s == "RFloat") return UnityEngine::RenderTextureFormat::RFloat;
      if (s == "RHalf") return UnityEngine::RenderTextureFormat::RHalf;
      if (s == "R8") return UnityEngine::RenderTextureFormat::R8;
      if (s == "DefaultHDR") return UnityEngine::RenderTextureFormat::DefaultHDR;
      if (s == "ARGBInt") return UnityEngine::RenderTextureFormat::ARGBInt;
      if (s == "RGInt") return UnityEngine::RenderTextureFormat::RGInt;
      if (s == "RInt") return UnityEngine::RenderTextureFormat::RInt;
      if (s == "BGRA32") return UnityEngine::RenderTextureFormat::BGRA32;
      if (s == "RGB111110Float") return UnityEngine::RenderTextureFormat::RGB111110Float;
      if (s == "RG32") return UnityEngine::RenderTextureFormat::RG32;
      if (s == "RGBAUShort") return UnityEngine::RenderTextureFormat::RGBAUShort;
      if (s == "RG16") return UnityEngine::RenderTextureFormat::RG16;
      if (s == "BGRA10101010_XR") return UnityEngine::RenderTextureFormat::BGRA10101010_XR;
      if (s == "BGR101010_XR") return UnityEngine::RenderTextureFormat::BGR101010_XR;
      if (s == "R16") return UnityEngine::RenderTextureFormat::R16;
      return std::nullopt;
    }();
  }
  auto filterName = ReadStringView(json, "filter");
  if (!filterName.has_value()) filterName = ReadStringView(json, "filterMode");
  if (filterName.has_value()) {
    auto fm = filterName;
    dt.filterMode = [&]() -> std::optional<UnityEngine::FilterMode> {
      auto s = *fm;
      if (s == "Point") return UnityEngine::FilterMode::Point;
      if (s == "Bilinear") return UnityEngine::FilterMode::Bilinear;
      if (s == "Trilinear") return UnityEngine::FilterMode::Trilinear;
      return std::nullopt;
    }();
  }
  if (dt.xRatio <= 0.0f) dt.xRatio = 1.0f;
  if (dt.yRatio <= 0.0f) dt.yRatio = 1.0f;
  _declaredTextures[name] = std::move(dt);
  // Desktop Vivify creates declared textures from the actual render source
  // descriptor. Keeping the declaration alive until that descriptor exists
  // prevents a beat-0 2D allocation from poisoning Quest Multiview maps.
  EnsureDeclaredTextures();
}

void Runtime::ReleaseDeclaredTextureData(DeclaredTextureData& data) {

  if (data.propertyId != 0) {
    UnityEngine::Shader::SetGlobalTexture(data.propertyId, static_cast<UnityEngine::Texture*>(nullptr));
  }
  if (IsAlive(data.texture)) {
    UnityEngine::Object::Destroy(data.texture);
  }
  data.texture = nullptr;
}

bool Runtime::DestroyDeclaredTextureById(std::string const& id) {
  auto it = _declaredTextures.find(id);
  if (it == _declaredTextures.end()) return false;
  ReleaseDeclaredTextureData(it->second);
  _declaredTextures.erase(it);
  return true;
}

void Runtime::HandleCreateCamera(rapidjson::Value const& json) {
  auto id = ReadStringView(json, "id");
  if (!id.has_value()) return;
  std::string name(*id);
  // Match CameraDatas.Add on desktop: a duplicate is invalid until a
  // DestroyObject event removes the first concrete camera.
  if (_secondaryCameras.contains(name)) {
    PaperLogger.warn("Vivify CreateCamera ignored duplicate id '{}'", name);
    return;
  }
  if (GetDisableCreateCameraDepth()) {
    if (GetVivifyDebugLogging()) {
      PaperLogger.info("Vivify CreateCamera skipped: isolation toggle enabled id='{}'", name);
    }
    return;
  }
  SecondaryCameraData cam;
  cam.name = name;
  if (auto tex = ReadStringView(json, "texture")) {
    cam.textureName = std::string(*tex);
    cam.texturePropertyId = UnityEngine::Shader::PropertyToID(StringW(*tex));
  }
  if (auto dtex = ReadStringView(json, "depthTexture")) {
    cam.depthTextureName = std::string(*dtex);
    cam.depthTexturePropertyId = UnityEngine::Shader::PropertyToID(StringW(*dtex));
  }

  // CameraPropertyManager on desktop is independent from the lifetime of a
  // concrete secondary camera. Recreating an id after DestroyObject inherits
  // prior SetCameraProperty state, then overlays CreateCamera properties.
  if (auto stored = _cameraProperties.find(name); stored != _cameraProperties.end()) {
    cam.properties = stored->second;
  }
  auto* propsVal = ReadValuePtr(json, "properties");
  if (propsVal != nullptr && propsVal->IsObject()) {
    ParseCameraPropertyData(*propsVal, cam.properties);
  }
  _cameraProperties[name] = cam.properties;
  auto mainCam = UnityEngine::Camera::get_main();
  auto* mainCamPtr = mainCam.unsafePtr();
  auto* go = UnityEngine::GameObject::New_ctor(StringW("VivifyCamera_" + name));
  if (IsAlive(mainCamPtr)) {
    go->get_transform()->SetParent(mainCam->get_transform(), false);
  }
  cam.camera = go->AddComponent<UnityEngine::Camera*>();
  if (IsAlive(mainCamPtr)) {
    cam.camera->CopyFrom(mainCamPtr);

    // CreateCamera on desktop copies BloomPrePass onto every secondary camera.
    // Camera::CopyFrom only copies Camera state, so the old Quest base silently
    // ignored the authored bloomPrePass property and rendered a different scene.
    auto* sourceBloom = mainCamPtr->get_gameObject()->GetComponent<GlobalNamespace::BloomPrePass*>();
    if (IsAlive(sourceBloom)) {
      auto* secondaryBloom = go->AddComponent<GlobalNamespace::BloomPrePass*>();
      if (IsAlive(secondaryBloom)) {
        secondaryBloom->____bloomPrepassRenderer = sourceBloom->____bloomPrepassRenderer;
        secondaryBloom->____bloomPrePassEffectContainer = sourceBloom->____bloomPrePassEffectContainer;
        secondaryBloom->____bloomPrePassRenderData = sourceBloom->____bloomPrePassRenderData;
        secondaryBloom->____mode = sourceBloom->____mode;
      }
    }
  }
  cam.baseDepthTextureMode = cam.camera->get_depthTextureMode().value__;
  cam.baseClearFlags = cam.camera->get_clearFlags().value__;
  cam.baseBackgroundColor = cam.camera->get_backgroundColor();
  cam.camera->set_depth(IsAlive(mainCamPtr) ? mainCam->get_depth() - 1 : -2.0f);
  EnsureMultipassKeywordController(go);
  int w = 1024, h = 512;
  if (IsAlive(mainCamPtr)) {
    w = std::max(1, mainCam->get_pixelWidth());
    h = std::max(1, mainCam->get_pixelHeight());
  }
  bool const wantColor = cam.texturePropertyId.has_value();
  bool const wantDepth = cam.depthTexturePropertyId.has_value();
  if (!wantColor && !wantDepth) {
    cam.camera->set_enabled(false);
  } else {
    // Setting targetTexture or SetTargetBuffers disables stereo rendering on a
    // Quest camera. Keep this helper on the headset render path and capture
    // the per-eye source in SecondaryCameraController::OnRenderImage. The main
    // camera, which renders one depth step later, overwrites the helper output.
    cam.camera->set_targetTexture(nullptr);
    cam.camera->set_enabled(true);
    if (wantDepth) {
      // This is a Quest rendering requirement, not an authored override. Keep
      // it in the baseline so a later explicit null still leaves depth output
      // functional, exactly as desktop restores its controller baseline.
      cam.baseDepthTextureMode = cam.baseDepthTextureMode.value_or(0) |
                                 UnityEngine::DepthTextureMode::Depth.value__;
    }
    auto* controller = go->AddComponent<SecondaryCameraController*>();
    if (IsAlive(controller)) controller->cameraName = name;
  }
  if (GetVivifyDebugLogging()) {
    PaperLogger.info("Vivify CreateCamera: id='{}' colorTexture='{}' depthTexture='{}' expectedSize={}x{} targetlessStereo={} stereoMode={} mainEffect={}",
                     name, cam.textureName.value_or("<none>"), cam.depthTextureName.value_or("<none>"),
                     w, h, BoolText(wantColor || wantDepth),
                     UnityEngine::XR::XRSettings::get_stereoRenderingMode().value__,
                     BoolText(cam.properties.mainEffect.value_or(true)));
  }
  ApplySecondaryCameraProperties(cam);
  ApplyCameraGameObjectProperties(cam.camera, cam.properties);
  cam.enabledBeforePause = false;
  _secondaryCameras[name] = std::move(cam);
}

void Runtime::ReleaseSecondaryCameraData(SecondaryCameraData& data) {
  // Stop callbacks before releasing any captured output.
  if (IsAlive(data.camera)) {
    auto* go = data.camera->get_gameObject().unsafePtr();
    if (IsAlive(go)) {
      auto* culling = go->GetComponent<CullingCameraController*>();
      if (IsAlive(culling)) {
        // A stage transition can destroy a secondary camera between
        // OnPreCull and OnPostRender. Restore every moved renderer layer
        // before destroying it, otherwise real notes can remain hidden while
        // Beat Saber and its audio keep running.
        culling->CullingPostRender();
      }
    }
    data.camera->set_enabled(false);
    data.camera->set_targetTexture(nullptr);
    UnityEngine::Object::Destroy(data.camera->get_gameObject());
    data.camera = nullptr;
  }
  if (data.texturePropertyId.has_value()) {
    UnityEngine::Shader::SetGlobalTexture(data.texturePropertyId.value(), static_cast<UnityEngine::Texture*>(nullptr));
  }
  if (data.depthTexturePropertyId.has_value()) {
    UnityEngine::Shader::SetGlobalTexture(data.depthTexturePropertyId.value(), static_cast<UnityEngine::Texture*>(nullptr));
  }
  for (auto*& rt : data.colorRTs) {
    if (IsAlive(rt)) UnityEngine::Object::Destroy(rt);
    rt = nullptr;
  }
  for (auto*& rt : data.depthRTs) {
    if (IsAlive(rt)) UnityEngine::Object::Destroy(rt);
    rt = nullptr;
  }
  // depthRT is retained only as a compatibility field for reset paths created
  // by older code; the targetless capture path does not allocate one.
  if (IsAlive(data.depthRT)) {
    UnityEngine::Object::Destroy(data.depthRT);
  }
  data.colorRT = nullptr;
  data.depthRT = nullptr;
}

UnityEngine::RenderTexture* Runtime::SecondaryCameraColorRT(SecondaryCameraData const& cam) {
  return IsAlive(cam.colorRT) ? cam.colorRT : nullptr;
}

void Runtime::CaptureSecondaryCameraFrame(std::string const& cameraName, UnityEngine::Camera* camera,
                                          UnityEngine::RenderTexture* src) {
  if (_isResetting || _pauseMenuActive || _currentBeatmapData == nullptr || !IsAlive(camera) || !IsAlive(src)) return;
  auto found = _secondaryCameras.find(cameraName);
  if (found == _secondaryCameras.end()) return;
  auto& cam = found->second;
  if (cam.camera != camera) return;

  auto sourceColorDescriptor = src->get_descriptor();
  bool const stereoArrayCapture = IsTextureArray(sourceColorDescriptor);
  // In single-pass instanced/multiview Unity commonly reports Left (0) from
  // stereoActiveEye even though src contains both array slices.  Store that
  // complete texture in the Mono/shared slot; slots 0/1 are only for true
  // multipass callbacks.
  int eye = stereoArrayCapture ? 2 : camera->get_stereoActiveEye().value__;
  if (eye < 0 || eye > 2) eye = 2;
  cam.activeEye = eye;

  if (cam.texturePropertyId.has_value()) {
    auto srcDesc = sourceColorDescriptor;
    srcDesc.set_msaaSamples(1);
    srcDesc.set_depthBufferBits(0);
    ConfigureStereoRenderTextureDescriptor(srcDesc, stereoArrayCapture);
    auto*& output = cam.colorRTs[eye];
    bool recreate = !IsAlive(output);
    if (!recreate) {
      auto outputDesc = output->get_descriptor();
      recreate = outputDesc.get_width() != srcDesc.get_width() ||
                 outputDesc.get_height() != srcDesc.get_height() ||
                 outputDesc.get_graphicsFormat().value__ != srcDesc.get_graphicsFormat().value__ ||
                 outputDesc.get_dimension().value__ != srcDesc.get_dimension().value__ ||
                 outputDesc.get_volumeDepth() != srcDesc.get_volumeDepth();
    }
    if (recreate) {
      if (IsAlive(output)) UnityEngine::Object::Destroy(output);
      output = UnityEngine::RenderTexture::New_ctor(srcDesc);
      if (!IsAlive(output) || !output->Create()) {
        if (GetVivifyDebugLogging()) {
          PaperLogger.warn("Vivify secondary capture allocation failed: camera='{}' eye={} size={}x{} dim={} slices={}",
                           cameraName, eye, srcDesc.get_width(), srcDesc.get_height(),
                           srcDesc.get_dimension().value__, srcDesc.get_volumeDepth());
        }
        if (IsAlive(output)) UnityEngine::Object::Destroy(output);
        output = nullptr;
      } else {
        ClearRenderTexture(output);
      }
    }
    if (IsAlive(output)) {
      if (CopyCapturedTexture(src, output, cameraName, "color",
                              cam.captureCopyFailureLogCount)) {
        cam.colorRT = output;
        UnityEngine::Shader::SetGlobalTexture(cam.texturePropertyId.value(),
                                              static_cast<UnityEngine::Texture*>(output));
      }
    }
  }

  if (cam.depthTexturePropertyId.has_value()) {
    static int const cameraDepthTextureId = UnityEngine::Shader::PropertyToID(u"_CameraDepthTexture");
    auto depth = UnityEngine::Shader::GetGlobalTexture(cameraDepthTextureId);
    auto* depthPtr = depth.unsafePtr();
    auto* sourceDepthRT = IsAlive(depthPtr)
                              ? il2cpp_utils::try_cast<UnityEngine::RenderTexture>(depthPtr).value_or(nullptr)
                              : nullptr;
    if (IsAlive(sourceDepthRT)) {
      auto sourceDepthDesc = sourceDepthRT->get_descriptor();
      auto depthDesc = sourceDepthDesc;
      bool const stereoDepthCapture = IsTextureArray(sourceDepthDesc);
      ConfigureStereoRenderTextureDescriptor(depthDesc, stereoDepthCapture);
      int const depthEye = stereoDepthCapture ? 2 : eye;
      auto*& outputDepth = cam.depthRTs[depthEye];
      bool recreate = !IsAlive(outputDepth);
      if (!recreate) {
        auto outputDesc = outputDepth->get_descriptor();
        recreate = outputDesc.get_width() != depthDesc.get_width() ||
                   outputDesc.get_height() != depthDesc.get_height() ||
                   outputDesc.get_msaaSamples() != depthDesc.get_msaaSamples() ||
                   outputDesc.get_graphicsFormat().value__ != depthDesc.get_graphicsFormat().value__ ||
                   outputDesc.get_depthStencilFormat().value__ != depthDesc.get_depthStencilFormat().value__ ||
                   outputDesc.get_dimension().value__ != depthDesc.get_dimension().value__ ||
                   outputDesc.get_volumeDepth() != depthDesc.get_volumeDepth();
      }
      if (recreate) {
        if (IsAlive(outputDepth)) UnityEngine::Object::Destroy(outputDepth);
        outputDepth = UnityEngine::RenderTexture::New_ctor(depthDesc);
        if (!IsAlive(outputDepth) || !outputDepth->Create()) {
          if (GetVivifyDebugLogging()) {
            PaperLogger.warn("Vivify secondary depth allocation failed: camera='{}' eye={} size={}x{} dim={} slices={}",
                             cameraName, eye, depthDesc.get_width(), depthDesc.get_height(),
                             depthDesc.get_dimension().value__, depthDesc.get_volumeDepth());
          }
          if (IsAlive(outputDepth)) UnityEngine::Object::Destroy(outputDepth);
          outputDepth = nullptr;
        }
      }
      if (IsAlive(outputDepth)) {
        // Unity reuses its global _CameraDepthTexture for the next camera.
        // Keep an owned copy so the main camera cannot overwrite NotesCam's
        // depth before the authored compositor samples it.
        if (CopyCapturedTexture(sourceDepthRT, outputDepth, cameraName, "depth",
                                cam.captureCopyFailureLogCount)) {
          UnityEngine::Shader::SetGlobalTexture(cam.depthTexturePropertyId.value(), outputDepth);
        }
        if (cam.depthCaptureLogCount < 3 && GetVivifyDebugLogging()) {
          cam.depthCaptureLogCount++;
          auto outputDepthDesc = outputDepth->get_descriptor();
          PaperLogger.info(
              "Vivify secondary depth layout: camera='{}' eye={} source={}x{} vrUsage={} dim={} slices={} output={}x{} vrUsage={} dim={} slices={}",
              cameraName, depthEye, sourceDepthDesc.get_width(), sourceDepthDesc.get_height(),
              sourceDepthRT->get_vrUsage().value__, sourceDepthDesc.get_dimension().value__,
              sourceDepthDesc.get_volumeDepth(), outputDepthDesc.get_width(),
              outputDepthDesc.get_height(), outputDepth->get_vrUsage().value__,
              outputDepthDesc.get_dimension().value__, outputDepthDesc.get_volumeDepth());
        }
      }
    } else if (cam.missingDepthLogCount < 2 && GetVivifyDebugLogging()) {
      cam.missingDepthLogCount++;
      PaperLogger.warn("Vivify secondary depth capture is not a RenderTexture: camera='{}' eye={} depthMode={}",
                       cameraName, eye, camera->get_depthTextureMode().value__);
    }
  }

  if (cam.captureLogCount < 3 && GetVivifyDebugLogging()) {
    cam.captureLogCount++;
    auto desc = src->get_descriptor();
    PaperLogger.info("Vivify secondary capture: camera='{}' eye={} size={}x{} vrUsage={} dimension={} slices={} color={} depth={}",
                     cameraName, eye, desc.get_width(), desc.get_height(), src->get_vrUsage().value__,
                     desc.get_dimension().value__, desc.get_volumeDepth(),
                     BoolText(cam.texturePropertyId.has_value() && IsAlive(cam.colorRTs[eye])),
                     BoolText(cam.depthTexturePropertyId.has_value() && IsAlive(cam.depthRTs[eye])));
  }
}

void Runtime::BindSecondaryCameraTextures() {
  if (_secondaryCameras.empty()) return;
  auto mainCamera = UnityEngine::Camera::get_main();
  auto* mainCameraPtr = mainCamera.unsafePtr();
  int eye = UsesTextureArrayStereo()
                ? 2
                : (IsAlive(mainCameraPtr) ? mainCameraPtr->get_stereoActiveEye().value__ : 2);
  if (eye < 0 || eye > 2) eye = 2;

  auto chooseEyeTexture = [eye](auto const& textures) -> UnityEngine::RenderTexture* {
    std::array<int, 3> order = eye == 2 ? std::array<int, 3>{2, 0, 1}
                                        : (eye == 0 ? std::array<int, 3>{0, 2, 1}
                                                    : std::array<int, 3>{1, 2, 0});
    for (int slot : order) {
      if (IsManagedAlive(textures[slot])) return textures[slot];
    }
    return nullptr;
  };
  for (auto& [n, cam] : _secondaryCameras) {
    if (!IsAlive(cam.camera)) continue;
    auto* color = chooseEyeTexture(cam.colorRTs);
    cam.colorRT = color;
    cam.activeEye = eye;
    if (cam.texturePropertyId.has_value()) {
      UnityEngine::Shader::SetGlobalTexture(cam.texturePropertyId.value(),
                                            static_cast<UnityEngine::Texture*>(color));
    }
    if (cam.depthTexturePropertyId.has_value()) {
      auto* depth = chooseEyeTexture(cam.depthRTs);
      UnityEngine::Shader::SetGlobalTexture(cam.depthTexturePropertyId.value(), depth);
    }
  }
}

void Runtime::RestoreSecondaryCullingLayers() {
  for (auto& [name, cam] : _secondaryCameras) {
    if (!IsAlive(cam.camera)) continue;
    auto* go = cam.camera->get_gameObject().unsafePtr();
    if (!IsAlive(go)) continue;
    auto* culling = go->GetComponent<CullingCameraController*>();
    if (IsAlive(culling)) {
      culling->CullingPostRender();
    }
  }
}

bool Runtime::DestroySecondaryCameraById(std::string const& id) {
  auto it = _secondaryCameras.find(id);
  if (it == _secondaryCameras.end()) return false;
  ReleaseSecondaryCameraData(it->second);
  _secondaryCameras.erase(it);
  // CameraPropertyManager keeps per-id state after the concrete controller is
  // removed, so a later CreateCamera with the same id receives it again.
  return true;
}

void Runtime::ApplySecondaryCameraMainEffects(GlobalNamespace::MainEffectController* mainEffectController) {
  if (_secondaryCameras.empty() || _isResetting || _pauseMenuActive || _currentBeatmapData == nullptr) return;
  if (!IsManagedAlive(mainEffectController) ||
      mainEffectController->_mainEffectContainer.ptr() == nullptr ||
      !IsManagedAlive(mainEffectController->_mainEffectContainer)) {
    return;
  }
  auto mainEffectSO = mainEffectController->_mainEffectContainer->get_mainEffect();
  if (mainEffectSO.ptr() == nullptr || !IsManagedAlive(mainEffectSO) || !mainEffectSO->get_hasPostProcessEffect()) {
    return;
  }
  for (auto& [name, cam] : _secondaryCameras) {

    // CameraPropertyController on desktop resolves an unset/null MainEffect
    // value to true.  The old Quest base used false here, so every secondary
    // camera silently skipped Beat Saber's main effect unless a map opted in
    // explicitly.  Keep the upstream default and let authored false disable it.
    if (!cam.properties.mainEffect.value_or(true)) continue;
    if (!IsAlive(cam.colorRT)) continue;
    auto desc = cam.colorRT->get_descriptor();
    desc.set_msaaSamples(1);
    desc.set_depthBufferBits(0);
    auto temp = UnityEngine::RenderTexture::GetTemporary(desc);
    auto* tempPtr = temp.unsafePtr();
    if (!IsManagedAlive(tempPtr)) continue;
    mainEffectController->OnPreRender();
    mainEffectSO->Render(cam.colorRT, tempPtr, mainEffectController->____fadeValue);
    mainEffectController->OnPostRender();
    // A material-free final copy must preserve both Multiview array slices.
    // Graphics.Blit can update only the active eye and caused intermittent
    // Aether/42-flux left-right asymmetry after secondary-camera main effects.
    if (!CopyStereoRenderTexture(tempPtr, cam.colorRT)) {
      UnityEngine::Graphics::Blit(static_cast<UnityEngine::Texture*>(tempPtr), cam.colorRT);
    }
    UnityEngine::RenderTexture::ReleaseTemporary(tempPtr);
  }
}

std::optional<UnityEngine::DepthTextureMode> Runtime::ParseDepthTextureModeName(std::string_view s) {
  if (s == "None") return UnityEngine::DepthTextureMode::None;
  if (s == "Depth") return UnityEngine::DepthTextureMode::Depth;
  if (s == "DepthNormals") return UnityEngine::DepthTextureMode::DepthNormals;
  if (s == "MotionVectors") return UnityEngine::DepthTextureMode::MotionVectors;
  return std::nullopt;
}

std::optional<UnityEngine::DepthTextureMode> Runtime::ParseDepthTextureModeValue(rapidjson::Value const& value) {
  if (value.IsString()) {
    return ParseDepthTextureModeName(std::string_view(value.GetString(), value.GetStringLength()));
  }
  if (!value.IsArray()) {
    return std::nullopt;
  }
  int flags = 0;
  for (auto const& item : value.GetArray()) {
    if (!item.IsString()) return std::nullopt;
    auto mode = ParseDepthTextureModeName(std::string_view(item.GetString(), item.GetStringLength()));
    if (!mode.has_value()) return std::nullopt;
    flags |= mode->value__;
  }
  // Desktop Aggregate starts at DepthTextureMode.None, so an authored empty
  // array is a valid explicit None rather than an invalid/missing property.
  return UnityEngine::DepthTextureMode(flags);
}

void Runtime::ParseCameraPropertyData(rapidjson::Value const& json, CameraPropertyData& out) {
  if (auto* dtm = ReadValuePtr(json, "depthTextureMode")) {
    if (dtm->IsNull()) {
      out.depthTextureMode.reset();
    } else if (auto parsed = ParseDepthTextureModeValue(*dtm); parsed.has_value()) {
      out.depthTextureMode = *parsed;
    } else {
      PaperLogger.warn("Vivify SetCameraProperty ignored invalid depthTextureMode");
    }
    if (GetDisableCreateCameraDepth()) out.depthTextureMode.reset();
  }
  if (auto* cf = ReadValuePtr(json, "clearFlags")) {
    if (cf->IsNull()) {
      out.clearFlags.reset();
    } else if (cf->IsString()) {
      std::string_view s(cf->GetString(), cf->GetStringLength());
      if (s == "Skybox") out.clearFlags = UnityEngine::CameraClearFlags::Skybox;
      else if (s == "Color" || s == "SolidColor") out.clearFlags = UnityEngine::CameraClearFlags::Color;
      else if (s == "Depth") out.clearFlags = UnityEngine::CameraClearFlags::Depth;
      else if (s == "Nothing") out.clearFlags = UnityEngine::CameraClearFlags::Nothing;
      else PaperLogger.warn("Vivify SetCameraProperty ignored invalid clearFlags '{}'", std::string(s));
    } else {
      PaperLogger.warn("Vivify SetCameraProperty ignored non-string clearFlags");
    }
  }
  if (auto* bgVal = ReadValuePtr(json, "backgroundColor")) {
    if (bgVal->IsNull()) {
      out.backgroundColor.reset();
    } else if (auto color = ReadColorArray(*bgVal); color.has_value()) {
      out.backgroundColor = *color;
    } else {
      PaperLogger.warn("Vivify SetCameraProperty ignored invalid backgroundColor");
    }
  }
  if (auto* cullingVal = ReadValuePtr(json, "culling")) {
    if (cullingVal->IsNull()) {
      out.culling.reset();
    } else if (cullingVal->IsObject()) {
      CullingMaskData culling;
      bool const v2 = _currentBeatmapData != nullptr && _currentBeatmapData->v2orEarlier;
      culling.tracks = ReadTracks(*cullingVal, v2);
      culling.whitelist = ReadBool(*cullingVal, "whitelist").value_or(false);
      out.culling = std::move(culling);
    } else {
      PaperLogger.warn("Vivify SetCameraProperty ignored invalid culling");
    }
  }
  if (auto* bp = ReadValuePtr(json, "bloomPrePass")) {
    if (bp->IsNull()) out.bloomPrePass.reset();
    else if (bp->IsBool()) out.bloomPrePass = bp->GetBool();
    else PaperLogger.warn("Vivify SetCameraProperty ignored invalid bloomPrePass");
  }
  if (auto* me = ReadValuePtr(json, "mainEffect")) {
    if (me->IsNull()) out.mainEffect.reset();
    else if (me->IsBool()) out.mainEffect = me->GetBool();
    else PaperLogger.warn("Vivify SetCameraProperty ignored invalid mainEffect");
  }
}

void Runtime::ApplyCameraProperties(UnityEngine::Camera* camera, CameraPropertyData const& props) {
  if (!IsAlive(camera)) return;
  bool const isMainCamera = camera->get_gameObject().unsafePtr() == _lastMainCameraGO;
  if (props.depthTextureMode.has_value()) {
    int mode = props.depthTextureMode->value__;

    if (isMainCamera && _mainCamOriginalDepthMode.has_value()) {
      mode |= _mainCamOriginalDepthMode.value();
    }
    camera->set_depthTextureMode(UnityEngine::DepthTextureMode(mode));
  } else if (isMainCamera && _mainCamOriginalDepthMode.has_value()) {
    camera->set_depthTextureMode(UnityEngine::DepthTextureMode(_mainCamOriginalDepthMode.value()));
  }
  if (props.clearFlags.has_value()) {
    camera->set_clearFlags(props.clearFlags.value());
  } else if (isMainCamera && _mainCamOriginalClearFlags.has_value()) {
    camera->set_clearFlags(UnityEngine::CameraClearFlags(_mainCamOriginalClearFlags.value()));
  }
  if (props.backgroundColor.has_value()) {
    camera->set_backgroundColor(props.backgroundColor.value());
  } else if (isMainCamera && _mainCamOriginalBackgroundColor.has_value()) {
    camera->set_backgroundColor(_mainCamOriginalBackgroundColor.value());
  }
}

void Runtime::ApplySecondaryCameraProperties(SecondaryCameraData& data) {
  if (!IsAlive(data.camera)) return;

  int depthMode = data.baseDepthTextureMode.value_or(data.camera->get_depthTextureMode().value__);
  if (data.properties.depthTextureMode.has_value()) {
    depthMode |= data.properties.depthTextureMode->value__;
  }
  data.camera->set_depthTextureMode(UnityEngine::DepthTextureMode(depthMode));

  if (data.properties.clearFlags.has_value()) {
    data.camera->set_clearFlags(*data.properties.clearFlags);
  } else if (data.baseClearFlags.has_value()) {
    data.camera->set_clearFlags(UnityEngine::CameraClearFlags(*data.baseClearFlags));
  }

  if (data.properties.backgroundColor.has_value()) {
    data.camera->set_backgroundColor(*data.properties.backgroundColor);
  } else if (data.baseBackgroundColor.has_value()) {
    data.camera->set_backgroundColor(*data.baseBackgroundColor);
  }
}

void Runtime::ApplyCameraGameObjectProperties(UnityEngine::Camera* camera, CameraPropertyData const& props) {
  if (!IsAlive(camera)) return;
  auto* cameraGO = camera->get_gameObject().unsafePtr();
  if (!IsAlive(cameraGO)) return;
  ApplyCullingProperty(cameraGO, props.culling);
  auto* bloomPrePass = cameraGO->GetComponent<GlobalNamespace::BloomPrePass*>();
  if (IsAlive(bloomPrePass)) {
    bloomPrePass->set_enabled(props.bloomPrePass.value_or(true));
  }
}

CullingCameraController* Runtime::EnsureCullingController(UnityEngine::GameObject* gameObject) {
  if (!IsAlive(gameObject)) return nullptr;
  auto* controller = gameObject->GetComponent<CullingCameraController*>();
  if (!IsAlive(controller)) {
    controller = gameObject->AddComponent<CullingCameraController*>();
  }
  return controller;
}

void Runtime::ApplyCullingProperty(UnityEngine::GameObject* cameraGO, std::optional<CullingMaskData> const& culling) {
  if (!IsAlive(cameraGO)) return;
  if (culling.has_value()) {
    auto* controller = EnsureCullingController(cameraGO);
    if (controller != nullptr) {
      controller->SetCullingData(culling->whitelist, culling->tracks);
      controller->set_enabled(true);
    }
  } else {
    auto* controller = cameraGO->GetComponent<CullingCameraController*>();
    if (IsAlive(controller)) {
      controller->ClearCullingData();
      controller->set_enabled(false);
    }
  }
}

void Runtime::HandleSetCameraProperty(rapidjson::Value const& json) {
  auto id = ReadStringView(json, "id");
  std::string camId = id.has_value() ? std::string(*id) : std::string(kMainCameraId);
  auto* propsVal = ReadValuePtr(json, "properties");
  if (propsVal == nullptr || !propsVal->IsObject()) return;
  if (GetDisableCreateCameraDepth() && camId != kMainCameraId) {
    if (GetVivifyDebugLogging()) {
      PaperLogger.info("Vivify SetCameraProperty skipped: CreateCamera/Depth isolation camera='{}'", camId);
    }
    return;
  }

  auto& stored = _cameraProperties[camId];
  ParseCameraPropertyData(*propsVal, stored);
  if (GetVivifyDebugLogging()) {
    PaperLogger.info("Vivify SetCameraProperty: camera='{}' depthTextureMode={} clearFlags={} hasBackground={} culling={} bloomPrePass={} mainEffect={}",
                     camId,
                     stored.depthTextureMode.has_value() ? stored.depthTextureMode->value__ : -1,
                     stored.clearFlags.has_value() ? stored.clearFlags->value__ : -1,
                     BoolText(stored.backgroundColor.has_value()),
                     BoolText(stored.culling.has_value()),
                     stored.bloomPrePass.has_value() ? BoolText(*stored.bloomPrePass) : "unset",
                     stored.mainEffect.has_value() ? BoolText(*stored.mainEffect) : "unset");
  }
  if (camId == kMainCameraId) {
    _mainCameraPropsDirty = true;
    auto mainCam = UnityEngine::Camera::get_main();
    auto* mainCamPtr = mainCam.unsafePtr();
    if (IsAlive(mainCamPtr)) {
      CaptureMainCameraOriginals(mainCamPtr, mainCamPtr->get_gameObject().unsafePtr());
      ApplyCameraProperties(mainCamPtr, stored);
      ApplyCameraGameObjectProperties(mainCamPtr, stored);
    }
  } else {
    auto it = _secondaryCameras.find(camId);
    if (it != _secondaryCameras.end()) {
      // CameraPropertyManager owns one state object per id on desktop. Keep the
      // concrete Quest camera as a projection of that state rather than
      // independently reparsing and risking divergence.
      it->second.properties = stored;
      ApplySecondaryCameraProperties(it->second);
      ApplyCameraGameObjectProperties(it->second.camera, it->second.properties);
    }
  }
}

void Runtime::CaptureMainCameraOriginals(UnityEngine::Camera* mainCam, UnityEngine::GameObject* mainCamGO) {
  if (!IsAlive(mainCam) || mainCamGO != _lastMainCameraGO) return;
  if (!_mainCamOriginalDepthMode.has_value()) {
    _mainCamOriginalDepthMode = mainCam->get_depthTextureMode().value__;
  }
  if (!_mainCamOriginalClearFlags.has_value()) {
    _mainCamOriginalClearFlags = mainCam->get_clearFlags().value__;
  }
  if (!_mainCamOriginalBackgroundColor.has_value()) {
    _mainCamOriginalBackgroundColor = mainCam->get_backgroundColor();
  }
}

void Runtime::RestoreMainCameraOriginals() {
  // Do not dereference _lastMainCameraGO directly.  Unity can destroy the old
  // gameplay camera before the first custom event for the next gameplay scene
  // reaches PrepareBeatmap.  The IL2CPP wrapper can still pass Object's
  // implicit-bool check after its native GameObject has gone away; calling
  // GetComponent on that stale wrapper caused the Quest SIGSEGV recorded in
  // GameObject::QueryComponentByType.  Resolve Camera.main first and only
  // restore when its live GameObject is exactly the object we captured.
  auto mainCamera = UnityEngine::Camera::get_main();
  auto* mainCam = mainCamera.unsafePtr();
  auto* liveMainCameraGO =
      IsAlive(mainCam) ? mainCam->get_gameObject().unsafePtr() : nullptr;
  if (IsAlive(liveMainCameraGO) && liveMainCameraGO == _lastMainCameraGO) {
    if (_mainCamOriginalDepthMode.has_value()) {
      mainCam->set_depthTextureMode(UnityEngine::DepthTextureMode(_mainCamOriginalDepthMode.value()));
    }
    if (_mainCamOriginalClearFlags.has_value()) {
      mainCam->set_clearFlags(UnityEngine::CameraClearFlags(_mainCamOriginalClearFlags.value()));
    }
    if (_mainCamOriginalBackgroundColor.has_value()) {
      mainCam->set_backgroundColor(_mainCamOriginalBackgroundColor.value());
    }
    ApplyCullingProperty(liveMainCameraGO, std::nullopt);
    auto* bloomPrePass = liveMainCameraGO->GetComponent<GlobalNamespace::BloomPrePass*>();
    if (IsAlive(bloomPrePass)) {
      bloomPrePass->set_enabled(true);
    }
  }
  // Force the next gameplay camera refresh to capture a fresh native object
  // and fresh originals, even when ResetRuntime was a same-scene rewind.
  _lastMainCameraGO = nullptr;
  _mainCamOriginalDepthMode.reset();
  _mainCamOriginalClearFlags.reset();
  _mainCamOriginalBackgroundColor.reset();
}

void Runtime::RefreshCameraComponents(bool allowCameraApplier) {
  auto mainCam = UnityEngine::Camera::get_main();
  auto* mainCamPtr = mainCam.unsafePtr();
  auto* mainCamGO = IsAlive(mainCamPtr) ? mainCam->get_gameObject().unsafePtr() : nullptr;
  // Never attach gameplay render components to the menu camera. Runtime::Update
  // performs the pointer-free transition reset; this guard closes the render
  // callback race if Camera.main changes in the middle of a frame.
  if (_currentBeatmapData != nullptr && IsAlive(mainCamGO) &&
      mainCamGO->get_name() == "MenuMainCamera") {
    return;
  }
  bool const cameraChanged = mainCamGO != _lastMainCameraGO;
  if (cameraChanged) {
    _lastMainCameraGO = mainCamGO;
    _mainCamOriginalDepthMode.reset();
    _mainCamOriginalClearFlags.reset();
    _mainCamOriginalBackgroundColor.reset();
    if (IsAlive(mainCamPtr)) {
      auto* camTransform = mainCamPtr->get_transform().unsafePtr();
      auto pos = IsAlive(camTransform) ? camTransform->get_position() : UnityEngine::Vector3(0.0f, 0.0f, 0.0f);
      auto fwd = IsAlive(camTransform) ? camTransform->get_forward() : UnityEngine::Vector3(0.0f, 0.0f, 0.0f);
      PaperLogger.info("Vivify main camera: name='{}' cullingMask=0x{:08x} stereoMode={} multipassSetting={} pos=({:.2f},{:.2f},{:.2f}) fwd=({:.2f},{:.2f},{:.2f})",
                       IsAlive(mainCamGO) ? ToStdString(mainCamGO->get_name()) : std::string("?"),
                       static_cast<uint32_t>(mainCamPtr->get_cullingMask()),
                       static_cast<int>(UnityEngine::XR::XRSettings::get_stereoRenderingMode().value__),
                       BoolText(GetMultipassRenderingEnabled()),
                       pos.x, pos.y, pos.z, fwd.x, fwd.y, fwd.z);
    }
  }

  if (IsAlive(mainCamPtr)) {
    static int sLastLoggedCullingMask = -2;
    int const mask = mainCamPtr->get_cullingMask();
    if (mask != sLastLoggedCullingMask) {
      sLastLoggedCullingMask = mask;
      PaperLogger.info("Vivify main camera cullingMask is now 0x{:08x}", static_cast<uint32_t>(mask));
    }
  }

  RefreshMultipassRendering(mainCamGO);
  static std::string const kMainCameraKey(kMainCameraId);
  bool appliedMainProperties = false;
  if (IsAlive(mainCamPtr)) {
    if (auto props = _cameraProperties.find(kMainCameraKey);
        props != _cameraProperties.end() && (cameraChanged || _mainCameraPropsDirty)) {
      CaptureMainCameraOriginals(mainCamPtr, mainCamGO);
      ApplyCameraProperties(mainCamPtr, props->second);
      ApplyCameraGameObjectProperties(mainCamPtr, props->second);
      appliedMainProperties = true;
    }
  }
  if (appliedMainProperties || !_cameraProperties.contains(kMainCameraKey)) {
    _mainCameraPropsDirty = false;
  }
  RefreshCameraApplier(mainCamGO, allowCameraApplier);
}

void Runtime::RefreshMultipassRendering(UnityEngine::GameObject* mainCamGO) {

  if (IsAlive(mainCamGO) && _currentBeatmapData != nullptr && !_isResetting) {
    _multipassController = EnsureMultipassKeywordController(mainCamGO);
    return;
  }
  if (_multipassController != nullptr && UnityEngine::Object::op_Implicit_bool(_multipassController)) {
    _multipassController->set_enabled(false);
    UnityEngine::Object::Destroy(_multipassController);
  }
  _multipassController = nullptr;
  SetMultipassShaderState(false);
}

MultipassKeywordController* Runtime::EnsureMultipassKeywordController(UnityEngine::GameObject* gameObject) {
  if (!IsAlive(gameObject)) return nullptr;
  auto* controller = IsAlive(_multipassController) &&
                             _multipassController->get_gameObject().unsafePtr() == gameObject
                         ? _multipassController
                         : gameObject->GetComponent<MultipassKeywordController*>();
  if (!IsAlive(controller)) {
    controller = gameObject->AddComponent<MultipassKeywordController*>();
  }
  if (IsAlive(controller)) {
    // _StereoActiveEye is authored only for true MultiPass. Multiview selects
    // its eye from the texture-array layer and must not use this controller.
    bool const enabled = GetMultipassRenderingEnabled() &&
                         UnityEngine::XR::XRSettings::get_enabled();
    if (controller->get_enabled() != enabled) {
      controller->set_enabled(enabled);
    }
    if (!enabled) {
      SetMultipassShaderState(false);
    }
  }
  return controller;
}

void Runtime::RefreshCameraApplier(UnityEngine::GameObject* mainCamGO, bool allowCameraApplier) {
  if (_pauseMenuActive) allowCameraApplier = false;
  if (!allowCameraApplier) {
    if (_cameraApplier != nullptr && UnityEngine::Object::op_Implicit_bool(_cameraApplier) &&
        _cameraApplier->get_enabled()) {
      _cameraApplier->set_enabled(false);
    }
    return;
  }
  if (!IsAlive(mainCamGO)) {
    DestroyCameraApplier();
    return;
  }
  bool createdApplier = false;
  if (_cameraApplier == nullptr || !UnityEngine::Object::op_Implicit_bool(_cameraApplier) ||
      _cameraApplierGO != mainCamGO ||
      _cameraApplier->sessionGeneration != _lifecycle.Generation()) {
    DestroyCameraApplier();
    _cameraApplier = mainCamGO->AddComponent<CameraApplier*>();
    _cameraApplier->set_enabled(false);
    _cameraApplierGO = mainCamGO;
    _cameraApplier->sessionGeneration = _lifecycle.Generation();
    createdApplier = true;
  }

  if (createdApplier || !IsAlive(_cameraApplier->imageEffectController)) {
    _cameraApplier->imageEffectController =
        mainCamGO->GetComponent<GlobalNamespace::ImageEffectController*>();
  }
  if (createdApplier || !IsAlive(_cameraApplier->mainEffectController)) {
    _cameraApplier->mainEffectController =
        mainCamGO->GetComponent<GlobalNamespace::MainEffectController*>();
  }
  _cameraApplier->hasMainEffect =
      IsAlive(_cameraApplier->imageEffectController) &&
      _cameraApplier->imageEffectController->get_isActiveAndEnabled() &&
      _cameraApplier->imageEffectController->__cordl_internal_get__renderImageCallback() != nullptr;

  bool const needsSecondaryCameraApplier = !_secondaryCameras.empty();
  // A beat-0 CreateScreenTexture/Blit can arrive before the first camera
  // callback (notably when bloom is disabled). Keep CameraApplier alive long
  // enough to cache the actual Quest XR descriptor and drain that queue.
  bool const needsStartupPostProcessing =
      !_deferredStartupPostProcessingEvents.empty();
  bool const needsBlit = needsSecondaryCameraApplier ||
                         needsStartupPostProcessing ||
                         (!GetDisableAllBlits() &&
                          (!_preEffects.empty() || !_postEffects.empty() ||
                           HasMidRenderEffects()));
  if (_cameraApplier->get_enabled() != needsBlit) _cameraApplier->set_enabled(needsBlit);
}

void Runtime::DestroyCameraApplier() {
  RemoveMidRenderCommandBuffers(_cameraApplier);
  if (_cameraApplier != nullptr && UnityEngine::Object::op_Implicit_bool(_cameraApplier)) {
    _cameraApplier->set_enabled(false);
    _cameraApplier->sessionGeneration = 0;
    _cameraApplier->hasMainEffect = false;
    _cameraApplier->imageEffectController = nullptr;
    _cameraApplier->mainEffectController = nullptr;
    UnityEngine::Object::Destroy(_cameraApplier);
  }
  _cameraApplier = nullptr;
  _cameraApplierGO = nullptr;
}

}
