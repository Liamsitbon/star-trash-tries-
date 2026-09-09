#include "NexoraRuntime.hpp"
#include "VideoPixelStats.hpp"
#include "main.hpp"

#include <algorithm>
#include <exception>

#include "UnityEngine/Color32.hpp"
#include "UnityEngine/Experimental/Rendering/TextureCreationFlags.hpp"
#include "UnityEngine/Rect.hpp"
#include "UnityEngine/RenderTextureDescriptor.hpp"
#include "UnityEngine/Shader.hpp"
#include "UnityEngine/SystemInfo.hpp"
#include "UnityEngine/Texture2D.hpp"
#include "UnityEngine/Rendering/TextureDimension.hpp"

namespace Nexora {
namespace {

bool Alive(UnityEngine::Object* object) {
  return object != nullptr && UnityEngine::Object::op_Implicit_bool(object);
}

int ObjectId(UnityEngine::Object* object) {
  return Alive(object) ? object->GetInstanceID() : 0;
}

// ReadPixels changes the active target. Restore it on both success and failure;
// never clear/write the decoder target or change global stereo shader state.
struct PixelReadbackScope {
  UnityEngine::RenderTexture* previous = nullptr;
  UnityEngine::Texture2D* pixels = nullptr;
  bool targetChanged = false;

  ~PixelReadbackScope() noexcept {
    try {
      if (targetChanged) {
        UnityEngine::RenderTexture::set_active(Alive(previous) ? previous : nullptr);
      }
    } catch (...) {
      PaperLogger.warn("Nexora video diagnostic could not restore its render target");
    }
    try {
      if (Alive(pixels)) UnityEngine::Object::Destroy(pixels);
    } catch (...) {
      PaperLogger.warn("Nexora video diagnostic could not release its CPU sample");
    }
  }
};

}  // namespace

void Runtime::CapturePausedVideoDiagnostics() {
  // ReadPixels synchronizes the GPU. Keep it opt-in, off the playing path and
  // limited to one attempt per loaded video; do not run during headset sleep.
  if (!GetDebugLoggingEnabled() || !_paused || _applicationPaused || !_focused ||
      _pendingReset || _lifecycle.RenderDepth() != 0) return;

  for (auto& [_, dome] : _domes) {
    if (dome.videoDiagnosticSampled || !dome.textureBound ||
        !Alive(dome.video) || dome.video->get_isPlaying() ||
        !dome.video->get_isPrepared() || dome.video->get_frame() < 0 ||
        !Alive(dome.videoTarget) || !dome.videoTarget->IsCreated()) continue;
    dome.videoDiagnosticSampled = true;

    try {
      auto* target = dome.videoTarget;
      auto descriptor = target->get_descriptor();
      auto* material = dome.material;
      auto* rendererMaterial = Alive(dome.renderer)
                                   ? dome.renderer->get_sharedMaterial().unsafePtr() : nullptr;
      auto* bound = Alive(material) ? material->GetTexture(u"_MainTex").unsafePtr() : nullptr;
      auto* playerTarget = dome.video->get_targetTexture().unsafePtr();
      auto* internalTexture = dome.video->get_texture().unsafePtr();
      auto* shader = Alive(material) ? material->get_shader().unsafePtr() : nullptr;
      PaperLogger.info(
          "Nexora video diagnostic binding dome='{}' frame={} videoTime={:.3f} api={} "
          "target={} playerTarget={} materialTexture={} internalTexture={} "
          "material={} rendererMaterial={} shader='{}' supported={} width={} height={} "
          "dimension={} format={} srgb={}",
          dome.id, dome.video->get_frame(), dome.video->get_time(),
          UnityEngine::SystemInfo::get_graphicsDeviceType().value__, ObjectId(target),
          ObjectId(playerTarget), ObjectId(bound), ObjectId(internalTexture),
          ObjectId(material), ObjectId(rendererMaterial),
          Alive(shader) ? std::string(shader->get_name()) : "<missing>",
          Alive(shader) && shader->get_isSupported(), target->get_width(),
          target->get_height(), descriptor.get_dimension().value__,
          target->get_graphicsFormat().value__, target->get_sRGB());
      if (Alive(material)) {
        auto const scale = material->get_mainTextureScale();
        auto const offset = material->get_mainTextureOffset();
        auto const tint = material->GetColor(u"_Tint");
        PaperLogger.info(
            "Nexora video diagnostic material dome='{}' ready={} opacity={} brightness={} "
            "exposure={} tint=({},{},{},{}) uvScale=({},{}) uvOffset=({},{}) queue={}",
            dome.id, material->GetFloat(u"_VideoReady"), material->GetFloat(u"_Opacity"),
            material->GetFloat(u"_Brightness"), material->GetFloat(u"_Exposure"),
            tint.r, tint.g, tint.b, tint.a, scale.x, scale.y, offset.x, offset.y,
            material->get_renderQueue());
        PaperLogger.info(
            "Nexora video diagnostic draw dome='{}' raw={} simple={} rgbd={} "
            "renderMode={} decoderRenderer={} domeRenderer={} depthWrite={}",
            dome.id, material->GetFloat(u"_RawSampling"),
            material->GetFloat(u"_SimpleSampling"), dome.rgbd.enabled,
            dome.video->get_renderMode().value__,
            ObjectId(dome.video->get_gameObject()->GetComponent<UnityEngine::Renderer*>()),
            ObjectId(dome.renderer), material->GetFloat(u"_DepthWrite"));
      }

      constexpr int patchSize = 8;
      int const width = target->get_width();
      int const height = target->get_height();
      if (descriptor.get_dimension() != UnityEngine::Rendering::TextureDimension::Tex2D ||
          target->get_antiAliasing() != 1 || width < patchSize || height < patchSize) {
        PaperLogger.warn("Nexora video diagnostic sample skipped: unsupported target layout");
        return;
      }

      PixelReadbackScope readback;
      readback.previous = UnityEngine::RenderTexture::get_active().unsafePtr();
      readback.pixels = UnityEngine::Texture2D::New_ctor(
          patchSize, patchSize, target->get_graphicsFormat(),
          UnityEngine::Experimental::Rendering::TextureCreationFlags::None);
      if (!Alive(readback.pixels)) return;
      // Set the restoration flag first in case the Unity call throws after
      // changing native state. No Apply is needed: results are only read on CPU.
      readback.targetChanged = true;
      UnityEngine::RenderTexture::set_active(target);
      VideoPixelStats stats;
      auto sentinel = ArrayW<UnityEngine::Color32>(patchSize * patchSize);
      for (int index = 0; index < patchSize * patchSize; ++index) {
        sentinel[index].r = 13 + index * 3;
        sentinel[index].g = 239 - index * 2;
        sentinel[index].b = 71 + index;
        sentinel[index].a = 255;
      }
      // Five tiny patches along the horizon of the packed image. These are
      // samples, not a claim about all pixels or a camera/screenshot capture.
      for (int patch = 0; patch < 5; ++patch) {
        int const x = std::clamp(width * (2 * patch + 1) / 10 - patchSize / 2,
                                 0, width - patchSize);
        int const y = std::clamp(height / 2 - patchSize / 2, 0, height - patchSize);
        // Some native ReadPixels failures only write to Unity's log. Poison the
        // CPU buffer first, so such failures cannot become false pixel evidence.
        readback.pixels->SetPixels32(sentinel);
        readback.pixels->ReadPixels(UnityEngine::Rect(x, y, patchSize, patchSize), 0, 0, false);
        auto pixels = readback.pixels->GetPixels32();
        bool unchanged = pixels.size() == sentinel.size();
        for (std::size_t index = 0; unchanged && index < pixels.size(); ++index) {
          unchanged = pixels[index].r == sentinel[index].r &&
                      pixels[index].g == sentinel[index].g &&
                      pixels[index].b == sentinel[index].b;
        }
        if (unchanged || pixels.size() != patchSize * patchSize) {
          PaperLogger.warn("Nexora video diagnostic readback inconclusive: CPU sample was not populated");
          return;
        }
        for (auto const& pixel : pixels) stats.Add(pixel.r, pixel.g, pixel.b);
      }
      PaperLogger.info(
          "Nexora video diagnostic pixels dome='{}' samples={} "
          "minRGB=({},{},{}) maxRGB=({},{},{}) meanRGB=({:.2f},{:.2f},{:.2f}) "
          "sampleOnly=true",
          dome.id, stats.count, stats.minimum[0], stats.minimum[1], stats.minimum[2],
          stats.maximum[0], stats.maximum[1], stats.maximum[2],
          stats.Mean(0), stats.Mean(1), stats.Mean(2));
    } catch (std::exception const& error) {
      PaperLogger.warn("Nexora video diagnostic failed without changing playback: {}", error.what());
    } catch (...) {
      PaperLogger.warn("Nexora video diagnostic failed without changing playback");
    }
    return;  // At most one dome per paused Update, even with layered media.
  }
}

}  // namespace Nexora
