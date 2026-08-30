// AudioLink.cpp
// Implementation of AudioLinkObj: reads audio output from a Unity
// AudioSource and uploads per-band sample data to a shader Material and
// global render texture. The file focuses on performance (reducing
// duplication and repeated system calls) and documentation so the
// behavior is clearer for future changes.

#include "AudioLink.hpp"
#include "AssetBundleManager.hpp"

#include "config.hpp"
#include "ConfigProperties.hpp"
#include "ShaderProperties.hpp"

#include "UnityEngine/Time.hpp"
#include "UnityEngine/Vector4.hpp"
#include "UnityEngine/PrimitiveType.hpp"
#include "UnityEngine/Renderer.hpp"
#include "UnityEngine/Texture2D.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Rendering/RenderTextureSubElement.hpp"

#include <chrono>
#include <ctime>
#include <cmath>
#include <cstring>


#include "sombrero/shared/FastVector3.hpp"

DEFINE_TYPE(AudioLink, AudioLinkObj);

using namespace UnityEngine;

// File-local helpers and small constants are kept in an anonymous
// namespace to limit linkage to this translation unit.
namespace {
    // Constants describing how the audio frames are organized.
    // There are 4 bands of 1023 floats each in the original design.
    static constexpr int kBandSize = 1023;                 // samples per band
    static constexpr int kBandCount = 4;                   // number of bands
    static constexpr int kFrameSize = kBandSize * kBandCount; // samples per channel total

    // Seconds per day, used when computing day / seconds for shader uniforms.
    static constexpr double kSecondsPerDay = 86400.0;

    // Mask used when packing network time low bits into shader vector.
    static constexpr int kNetworkTimeLowMask = 0xFFFF;

    // Resolve and call Unity's internal GetOutputDataHelper icall.
    // The icall pointer is resolved once (function-local static) for efficiency.
    inline void GetOutputDataHelperSafe(UnityEngine::AudioSource* instance, ArrayW<float> out, int channel) {
        if (!instance) return;
        static auto getOutputDataHelper = il2cpp_utils::resolve_icall<void, UnityEngine::AudioSource*, ArrayW<float>, int>(
            "UnityEngine.AudioSource::GetOutputDataHelper");
        if (getOutputDataHelper) getOutputDataHelper(instance, out, channel);
    }

    // Resolve Unity's internal GetSpatialBlendMix icall once. If the icall
    // is unavailable, return 0 as a safe default.
    inline float GetSpatialBlendMixSafe(UnityEngine::AudioSource* self) {
        if (!self) return 0.0f;
        static auto get_SpatialBlendMix = il2cpp_utils::resolve_icall<float, UnityEngine::AudioSource*>(
            "UnityEngine.AudioSource::GetSpatialBlendMix");
        if (get_SpatialBlendMix) return get_SpatialBlendMix(self);
        return 0.0f;
    }

    // Small helper to copy one band of the interleaved frame buffer into the
    // small `_samples` buffer. Kept as an inline function to make intent clear
    // and avoid repeating memcpy calls.
    inline void CopyBand(ArrayW<float> samplesDest, const ArrayW<float> framesSrc, int bandIndex) {
        // Safety: we assume `framesSrc` contains at least (bandIndex+1) * kBandSize
        // elements. At initialization we allocate exactly kFrameSize.
        std::memcpy(samplesDest.begin(), framesSrc.begin() + (bandIndex * kBandSize), sizeof(float) * kBandSize);
    }
} // namespace

// Keep the AudioLink namespace for class method implementations.
namespace AudioLink {

    // Constructor: allocate arrays and initialize state variables.
    // This mirrors the original runtime layout but makes initial state explicit.
    void AudioLinkObj::ctor(AssetBundleManager* assetBundleManager) {
        AudioLinkLogger.info("AudioLink ctor");

        // Pre-allocate backing buffers once so we don't cause managed allocations per-frame.
        _audioFramesL = ArrayW<float>(il2cpp_array_size_t(kFrameSize));
        _audioFramesR = ArrayW<float>(il2cpp_array_size_t(kFrameSize));
        _samples = ArrayW<float>(il2cpp_array_size_t(kBandSize));

        // Default theme colors (preserve legacy defaults)
        _customThemeColor0 = Sombrero::FastColor::red();
        _customThemeColor1 = Sombrero::FastColor::cyan();
        _customThemeColor2 = Sombrero::FastColor::pink();
        _customThemeColor3 = Sombrero::FastColor::lightblue();

        // Save dependency and zero runtime counters / flags explicitly.
        _assetBundleManager = assetBundleManager;

        _elapsedTime = 0.0;
        _elapsedTimeMSW = 0.0;
        _networkTimeMS = 0;
        _networkTimeMSAccumulatedError = 0.0;
        _fPSTime = 0.0; // triggers an immediate FPSUpdate on first Tick (matches original behavior)
        _fPSCount = 0;

        _rightChannelTestCounter = 0; // will cause a test on first SendAudioOutputData
        _ignoreRightChannel = false;
        _initialized = false;

        _audioSource = nullptr;
        _audioMaterial = nullptr;
        _testPlane = nullptr;
    }

    // Attach an AudioSource. We store the pointer for per-frame reads.
    void AudioLinkObj::SetAudioSource(UnityEngine::AudioSource* audioSource) {
        AudioLinkLogger.info("Set AudioSource: {}", fmt::ptr(audioSource));
        this->_audioSource = audioSource;
    }

    // Update the color scheme used by the shader; extracts colors from
    // the provided `ColorScheme` and pushes them into the material.
    void AudioLinkObj::SetColorScheme(GlobalNamespace::ColorScheme* colorScheme) {
        if (!colorScheme) return;
        AudioLinkLogger.info("Set ColorScheme: {}", fmt::ptr(colorScheme));
        _customThemeColor0 = colorScheme->_environmentColor0;
        _customThemeColor1 = colorScheme->_environmentColor1;
        _customThemeColor2 = colorScheme->_environmentColor0Boost;
        _customThemeColor3 = colorScheme->_environmentColor1Boost;

        UpdateThemeColors();
    }

    // Initialize assets and shader bindings. This method should be called when
    // the mod is ready to create its visuals (after the asset bundle manager is available).
    void AudioLinkObj::Initialize() {
        AudioLinkLogger.info("AudioLink Initialize");
        if (!_assetBundleManager) {
            AudioLinkLogger.error("AssetBundleManager was not provided; cannot initialize.");
            return;
        }

        _assetBundleManager->Load();

        // Get material / render texture from bundle manager and bind to global shader.
        this->_audioMaterial = _assetBundleManager->get_material();
        auto audioRenderTexture = _assetBundleManager->get_renderTexture();

        AudioLinkLogger.info("SetGlobalRenderTexture");
        Shader::SetGlobalTexture(ShaderProperties::_audioTexture, audioRenderTexture, Rendering::RenderTextureSubElement::Default);
        _initialized = true;

        // Create a small debug quad that displays the audio render texture. The
        // quad is optional and its visibility is controlled by `config.showTestPlane`.
        if (config.showTestPlane) {
            _testPlane = UnityEngine::GameObject::CreatePrimitive(UnityEngine::PrimitiveType::Quad);
            auto testPlaneTransform = _testPlane->get_transform();
            testPlaneTransform->set_localScale(Sombrero::FastVector3(2, 1, 1) * config.showTestPlane);
            testPlaneTransform->set_localPosition({0, 0.1f, 2});
            testPlaneTransform->set_localEulerAngles({80, 0, 0});
            UnityEngine::Object::DontDestroyOnLoad(_testPlane);

            auto mat = _testPlane->GetComponent<UnityEngine::Renderer*>()->get_material();
            mat->set_shader(Shader::Find("Unlit/Texture"));
            mat->set_mainTexture(audioRenderTexture);
        }
    }

    // Dispose visual artifacts created in Initialize. Safe to call multiple times.
    void AudioLinkObj::Dispose() {
        if (_testPlane && _testPlane->m_CachedPtr.m_value)
            UnityEngine::Object::DestroyImmediate(_testPlane);
        _testPlane = nullptr;
    }

    // Per-frame update called by Zenject's ITickable. Responsible for time
    // accounting and triggering audio reads / shader uploads.
    void AudioLinkObj::Tick() {
        if (!_initialized) return;

        // Get delta for this frame.
        _elapsedTime = Time::get_deltaTime();

        // Convert elapsed seconds into integer milliseconds and track a
        // fractional remainder to avoid drift (mirrors original logic).
        {
            double deltaTimeMS = _elapsedTime * 1000.0;
            int advanceTimeMS = static_cast<int>(deltaTimeMS);
            _networkTimeMSAccumulatedError += deltaTimeMS - advanceTimeMS;
            if (_networkTimeMSAccumulatedError > 1.0) {
                _networkTimeMSAccumulatedError -= 1.0;
                advanceTimeMS++;
            }
            _networkTimeMS += advanceTimeMS;
        }

        _fPSCount++;

        // Periodic FPS update (usually once per second when _fPSTime starts at 0).
        if (_elapsedTime >= _fPSTime) {
            FPSUpdate();
        }

        // If we have a material bound, update shader timing uniforms and then
        // read audio samples (if we have an AudioSource attached).
        if (_audioMaterial) {
            // Compute local time-of-day (seconds since midnight) once.
            const auto now = std::chrono::system_clock::now();
            const std::time_t now_c = std::chrono::system_clock::to_time_t(now);
            const std::tm local_tm = *std::localtime(&now_c); // main thread only
            const unsigned long long timeOfDay = static_cast<unsigned long long>(local_tm.tm_hour) * 3600ULL
                                                + static_cast<unsigned long long>(local_tm.tm_min) * 60ULL
                                                + static_cast<unsigned long long>(local_tm.tm_sec);

            // Provide per-frame timing parameters to the shader.
            _audioMaterial->SetVector(ShaderProperties::_advancedTimeProps, Vector4(
                _elapsedTime,
                _elapsedTimeMSW,
                static_cast<float>(timeOfDay),
                ConfigProperties::READBACK_TIME
            ));

            // Reuse `now` to compute seconds since Unix epoch to reduce jitter
            // between the two time values we pass to the shader.
            const double utcSecondsUnix = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

            _audioMaterial->SetVector(ShaderProperties::_advancedTimeProps2, Vector4(
                static_cast<float>(_networkTimeMS & kNetworkTimeLowMask),
                static_cast<float>(_networkTimeMS >> 16),
                static_cast<float>(std::floor(utcSecondsUnix / kSecondsPerDay)),
                static_cast<float>(std::fmod(utcSecondsUnix, kSecondsPerDay))
            ));

            if (_audioSource) {
                SendAudioOutputData();

                // Push AudioSource-derived modifiers to the shader (volume/spatial blend).
                _audioMaterial->SetFloat(ShaderProperties::_sourceVolume, _audioSource->get_volume());
                _audioMaterial->SetFloat(ShaderProperties::_sourceSpatialBlend, GetSpatialBlendMixSafe(_audioSource));
            }
        }
    }

    // Synchronize settings constants into the material. Called when configuration
    // changes or during initialization.
    void AudioLinkObj::UpdateSettings() {
        AudioLinkLogger.info("Updating Settings");
        if (!_audioMaterial) return;

        _audioMaterial->SetFloat(ShaderProperties::_x0, ConfigProperties::X0);
        _audioMaterial->SetFloat(ShaderProperties::_x1, ConfigProperties::X1);
        _audioMaterial->SetFloat(ShaderProperties::_x2, ConfigProperties::X2);
        _audioMaterial->SetFloat(ShaderProperties::_x3, ConfigProperties::X3);
        _audioMaterial->SetFloat(ShaderProperties::_threshold0, ConfigProperties::THRESHOLD0);
        _audioMaterial->SetFloat(ShaderProperties::_threshold1, ConfigProperties::THRESHOLD1);
        _audioMaterial->SetFloat(ShaderProperties::_threshold2, ConfigProperties::THRESHOLD2);
        _audioMaterial->SetFloat(ShaderProperties::_threshold3, ConfigProperties::THRESHOLD3);
        _audioMaterial->SetFloat(ShaderProperties::_gain, ConfigProperties::GAIN);
        _audioMaterial->SetFloat(ShaderProperties::_fadeLength, ConfigProperties::FADE_LENGTH);
        _audioMaterial->SetFloat(ShaderProperties::_fadeExpFalloff, ConfigProperties::FADE_EXP_FALLOFF);
        _audioMaterial->SetFloat(ShaderProperties::_bass, ConfigProperties::BASS);
        _audioMaterial->SetFloat(ShaderProperties::_treble, ConfigProperties::TREBLE);
    }

    // Push the configured theme colors to the shader material.
    void AudioLinkObj::UpdateThemeColors() {
        AudioLinkLogger.info("Updating Color Scheme");
        if (!_audioMaterial || !_audioMaterial->m_CachedPtr.m_value) return;

        _audioMaterial->SetInt(ShaderProperties::_themeColorMode, ConfigProperties::THEME_COLOR_MODE);
        _audioMaterial->SetColor(ShaderProperties::_customThemeColor0ID, _customThemeColor0);
        _audioMaterial->SetColor(ShaderProperties::_customThemeColor1ID, _customThemeColor1);
        _audioMaterial->SetColor(ShaderProperties::_customThemeColor2ID, _customThemeColor2);
        _audioMaterial->SetColor(ShaderProperties::_customThemeColor3ID, _customThemeColor3);
    }

    // Called approximately once per second (depending on _fPSTime behavior)
    // to publish FPS and wrap/adjust internal elapsed time counters.
    void AudioLinkObj::FPSUpdate() {
        if (_audioMaterial) {
            _audioMaterial->SetVector(ShaderProperties::_versionNumberAndFPSProperty, Vector4(ConfigProperties::AUDIOLINK_VERSION_NUMBER, 0, _fPSCount, 1));
            _audioMaterial->SetVector(ShaderProperties::_playerCountAndData, {0, 0, 0, 0});
        } else {
            AudioLinkLogger.error("_audioMaterial {} was not valid!\n", fmt::ptr(_audioMaterial.unsafePtr()));
            AudioLinkLogger.error("Some properties have not been set...");
        }

        // Reset per-second counters and increment the next-second marker.
        _fPSCount = 0;
        _fPSTime++;

        // Other things to handle every second.

        // This handles wrapping of the ElapsedTime so we don't lose precision
        // onthe floating point.
        const double ElapsedTimeMSWBoundary = 1024.0;
        if (_elapsedTime >= ElapsedTimeMSWBoundary) {
            _fPSTime = 0;
            _elapsedTime -= ElapsedTimeMSWBoundary;
            _elapsedTimeMSW++;
        }

        // Finely adjust our network time estimate if needed.
        int networkTimeMSNow = static_cast<int>(Time::get_time() * 1000.0f);
        int networkTimeDelta = networkTimeMSNow - _networkTimeMS;
        if (networkTimeDelta > 3000 || networkTimeDelta < -3000) {
            // Major upset, reset.
            _networkTimeMS = networkTimeMSNow;
        } else {
            // Slowly correct the timebase.
            _networkTimeMS += networkTimeDelta / 20;
        }
    }

    // Read audio output data from the attached AudioSource and upload the four
    // bands for each channel to the shader material. The overall strategy:
    //  - Read full channel buffers into _audioFramesL / _audioFramesR.
    //  - Break the full buffer into four equal bands and upload each band.
    void AudioLinkObj::SendAudioOutputData() {
        if (!_audioMaterial) return;
        if (!_audioSource) return;

        // Read left channel into its buffer.
        GetOutputDataHelperSafe(_audioSource, _audioFramesL, 0); // left channel

        // Right-channel logic: the plugin periodically tests whether the right
        // channel is actually providing data. If it isn't, we mirror the left
        // channel into the right buffer to keep the shader input consistent.
        if (_rightChannelTestCounter > 0) {
            if (_ignoreRightChannel) {
                // Mirror left to right when there is no real right channel.
                std::memcpy(_audioFramesR.begin(), _audioFramesL.begin(), sizeof(float) * kFrameSize);
            } else {
                GetOutputDataHelperSafe(_audioSource, _audioFramesR, 1); // right channel
            }
            _rightChannelTestCounter--;
        } else {
            // Trigger a right-channel test: zero the first sample, read, and
            // check whether the driver actually wrote anything.
            _rightChannelTestCounter = ConfigProperties::RIGHT_CHANNEL_TEST_DELAY;
            _audioFramesR[0] = 0.0f;
            GetOutputDataHelperSafe(_audioSource, _audioFramesR, 1);
            _ignoreRightChannel = (_audioFramesR[0] == 0.0f);
        }

        // Property ID arrays: calling the ShaderProperties operator int() will
        // lazily resolve the property IDs the first time this function runs.
        static const int samplesLProps[kBandCount] = {
            ShaderProperties::_samples0L,
            ShaderProperties::_samples1L,
            ShaderProperties::_samples2L,
            ShaderProperties::_samples3L
        };

        static const int samplesRProps[kBandCount] = {
            ShaderProperties::_samples0R, ShaderProperties::_samples1R,
            ShaderProperties::_samples2R, ShaderProperties::_samples3R};
        /*

            memcpy(_samples.begin(), _audioFramesL.begin() + (1023 * 0), sizeof(float) * 1023);
            _audioMaterial->SetFloatArray(ShaderProperties::_samples0L, _samples);
            memcpy(_samples.begin(), _audioFramesL.begin() + (1023 * 1), sizeof(float) * 1023);
            _audioMaterial->SetFloatArray(ShaderProperties::_samples1L, _samples);
            memcpy(_samples.begin(), _audioFramesL.begin() + (1023 * 2), sizeof(float) * 1023);
            _audioMaterial->SetFloatArray(ShaderProperties::_samples2L, _samples);
            memcpy(_samples.begin(), _audioFramesL.begin() + (1023 * 3), sizeof(float) * 1023);
            _audioMaterial->SetFloatArray(ShaderProperties::_samples3L, _samples);
    
            memcpy(_samples.begin(), _audioFramesR.begin() + (1023 * 0), sizeof(float) * 1023);
            _audioMaterial->SetFloatArray(ShaderProperties::_samples0R, _samples);
            memcpy(_samples.begin(), _audioFramesR.begin() + (1023 * 1), sizeof(float) * 1023);
            _audioMaterial->SetFloatArray(ShaderProperties::_samples1R, _samples);
            memcpy(_samples.begin(), _audioFramesR.begin() + (1023 * 2), sizeof(float) * 1023);
            _audioMaterial->SetFloatArray(ShaderProperties::_samples2R, _samples);
            memcpy(_samples.begin(), _audioFramesR.begin() + (1023 * 3), sizeof(float) * 1023);
            _audioMaterial->SetFloatArray(ShaderProperties::_samples3R, _samples);

         */

        // Copy each band into the small `_samples` buffer and upload.
        for (int band = 0; band < kBandCount; ++band) {
            CopyBand(_samples, _audioFramesL, band);
            _audioMaterial->SetFloatArray(samplesLProps[band], _samples);

            CopyBand(_samples, _audioFramesR, band);
            _audioMaterial->SetFloatArray(samplesRProps[band], _samples);
        }
    }

    /* these methods exist to make the things they return "readonly" */
    Sombrero::FastColor AudioLinkObj::get_customThemeColor0() { return _customThemeColor0; }
    Sombrero::FastColor AudioLinkObj::get_customThemeColor1() { return _customThemeColor1; }
    Sombrero::FastColor AudioLinkObj::get_customThemeColor2() { return _customThemeColor2; }
    Sombrero::FastColor AudioLinkObj::get_customThemeColor3() { return _customThemeColor3; }
    AudioSource* AudioLinkObj::get_audioSource() { return _audioSource; }
    Material* AudioLinkObj::get_audioMaterial() { return _audioMaterial; }
    ArrayW<float> AudioLinkObj::get_audioFramesL() { return _audioFramesL; }
    ArrayW<float> AudioLinkObj::get_audioFramesR() { return _audioFramesR; }
    ArrayW<float> AudioLinkObj::get_samples() { return _samples; }
} // namespace AudioLink
