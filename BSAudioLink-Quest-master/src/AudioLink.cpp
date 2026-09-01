#include "AudioLink.hpp"
#include "AssetBundleManager.hpp"

#include "config.hpp"
#include "ConfigProperties.hpp"
#include "ShaderProperties.hpp"

#include "UnityEngine/AudioClip.hpp"
#include "UnityEngine/PrimitiveType.hpp"
#include "UnityEngine/Renderer.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Vector4.hpp"
#include "UnityEngine/Rendering/RenderTextureSubElement.hpp"

#include <chrono>
#include <cmath>
#include <cstring>
#include <ctime>

#include "sombrero/shared/FastVector3.hpp"

DEFINE_TYPE(AudioLink, AudioLinkObj);

using namespace UnityEngine;

namespace {
    // AudioLink exposes four shader arrays of 1023 floats. Unity's public
    // AudioSource.GetOutputData contract requires a power-of-two capture array,
    // so capture 4096 samples and upload the first 4092 as four 1023-value bands.
    static constexpr int kBandSize = 1023;
    static constexpr int kBandCount = 4;
    static constexpr int kUploadSampleCount = kBandSize * kBandCount;
    static constexpr int kCaptureSampleCount = 4096;
    static_assert((kCaptureSampleCount & (kCaptureSampleCount - 1)) == 0);
    static_assert(kCaptureSampleCount >= kUploadSampleCount);

    static constexpr double kSecondsPerDay = 86400.0;
    static constexpr int kNetworkTimeLowMask = 0xFFFF;

    template<typename T>
    inline bool IsUnityObjectAlive(T* object) {
        return object && object->m_CachedPtr.m_value;
    }

    inline bool GetOutputDataSafe(UnityEngine::AudioSource* source, ArrayW<float> output, int channel) {
        if (!IsUnityObjectAlive(source) || output.size() < kCaptureSampleCount) return false;

        // Use the generated Beat Saber 1.40.8 API instead of resolving a private
        // icall with a hand-written ABI. bs-cordl knows the exact IL2CPP signature.
        source->GetOutputData(output, channel);
        return true;
    }

    inline void CopyBand(ArrayW<float> destination, const ArrayW<float> source, int bandIndex) {
        if (destination.size() < kBandSize || source.size() < kUploadSampleCount) return;
        std::memcpy(
            destination.begin(),
            source.begin() + (bandIndex * kBandSize),
            sizeof(float) * kBandSize);
    }
}

namespace AudioLink {
    void AudioLinkObj::ctor(AssetBundleManager* assetBundleManager) {
        AudioLinkLogger.info("AudioLink ctor");

        _audioFramesL = ArrayW<float>(il2cpp_array_size_t(kCaptureSampleCount));
        _audioFramesR = ArrayW<float>(il2cpp_array_size_t(kCaptureSampleCount));
        _samples = ArrayW<float>(il2cpp_array_size_t(kBandSize));

        _customThemeColor0 = Sombrero::FastColor::red();
        _customThemeColor1 = Sombrero::FastColor::cyan();
        _customThemeColor2 = Sombrero::FastColor::pink();
        _customThemeColor3 = Sombrero::FastColor::lightblue();

        _assetBundleManager = assetBundleManager;

        _elapsedTime = 0.0;
        _elapsedTimeMSW = 0.0;
        _networkTimeMS = 0;
        _networkTimeMSAccumulatedError = 0.0;
        _fPSTime = 0.0;
        _fPSCount = 0;

        _rightChannelTestCounter = 0;
        _ignoreRightChannel = false;
        _initialized = false;

        _audioSource = nullptr;
        _audioMaterial = nullptr;
        _testPlane = nullptr;
    }

    void AudioLinkObj::SetAudioSource(UnityEngine::AudioSource* audioSource) {
        if (audioSource && !IsUnityObjectAlive(audioSource)) audioSource = nullptr;
        AudioLinkLogger.info("Set AudioSource: {}", fmt::ptr(audioSource));
        _audioSource = audioSource;
    }

    void AudioLinkObj::SetColorScheme(GlobalNamespace::ColorScheme* colorScheme) {
        if (!colorScheme) return;

        AudioLinkLogger.info("Set ColorScheme: {}", fmt::ptr(colorScheme));
        _customThemeColor0 = colorScheme->_environmentColor0;
        _customThemeColor1 = colorScheme->_environmentColor1;
        _customThemeColor2 = colorScheme->_environmentColor0Boost;
        _customThemeColor3 = colorScheme->_environmentColor1Boost;
        UpdateThemeColors();
    }

    void AudioLinkObj::Initialize() {
        AudioLinkLogger.info("AudioLink Initialize");
        _initialized = false;

        if (!_assetBundleManager) {
            AudioLinkLogger.error("AssetBundleManager was not provided; cannot initialize AudioLink.");
            return;
        }

        _assetBundleManager->Load();
        _audioMaterial = _assetBundleManager->get_material();
        auto audioRenderTexture = _assetBundleManager->get_renderTexture();

        if (!IsUnityObjectAlive(_audioMaterial) || !IsUnityObjectAlive(audioRenderTexture)) {
            AudioLinkLogger.error("AudioLink material/render texture is unavailable; runtime will remain disabled.");
            _audioMaterial = nullptr;
            return;
        }

        Shader::SetGlobalTexture(
            ShaderProperties::_audioTexture,
            audioRenderTexture,
            Rendering::RenderTextureSubElement::Default);

        _initialized = true;

        // The old Quest port never called UpdateSettings during startup, leaving
        // correctness up to whatever happened to be serialized into the bundle.
        UpdateSettings();
        UpdateThemeColors();

        if (!config.showTestPlane) return;

        _testPlane = UnityEngine::GameObject::CreatePrimitive(UnityEngine::PrimitiveType::Quad);
        if (!IsUnityObjectAlive(_testPlane)) {
            _testPlane = nullptr;
            return;
        }

        auto testPlaneTransform = _testPlane->get_transform();
        if (IsUnityObjectAlive(testPlaneTransform)) {
            testPlaneTransform->set_localScale(Sombrero::FastVector3(2, 1, 1));
            testPlaneTransform->set_localPosition({0, 0.1f, 2});
            testPlaneTransform->set_localEulerAngles({80, 0, 0});
        }
        UnityEngine::Object::DontDestroyOnLoad(_testPlane);

        auto renderer = _testPlane->GetComponent<UnityEngine::Renderer*>();
        if (!IsUnityObjectAlive(renderer)) return;

        auto material = renderer->get_material();
        if (!IsUnityObjectAlive(material)) return;

        auto shader = Shader::Find("Unlit/Texture");
        if (IsUnityObjectAlive(shader)) material->set_shader(shader);
        material->set_mainTexture(audioRenderTexture);
    }

    void AudioLinkObj::Dispose() {
        _initialized = false;
        _audioSource = nullptr;
        _audioMaterial = nullptr;

        if (IsUnityObjectAlive(_testPlane)) {
            UnityEngine::Object::DestroyImmediate(_testPlane);
        }
        _testPlane = nullptr;
    }

    void AudioLinkObj::Tick() {
        if (!_initialized) return;
        if (!IsUnityObjectAlive(_audioMaterial)) {
            _initialized = false;
            _audioMaterial = nullptr;
            return;
        }

        const double deltaTime = static_cast<double>(Time::get_deltaTime());
        if (!std::isfinite(deltaTime) || deltaTime < 0.0) return;

        // AudioLink's elapsed time is cumulative. The previous Quest port
        // accidentally replaced += with =, breaking FPS/time shader data.
        _elapsedTime += deltaTime;

        {
            const double deltaTimeMS = deltaTime * 1000.0;
            int advanceTimeMS = static_cast<int>(deltaTimeMS);
            _networkTimeMSAccumulatedError += deltaTimeMS - advanceTimeMS;
            if (_networkTimeMSAccumulatedError >= 1.0) {
                _networkTimeMSAccumulatedError -= 1.0;
                advanceTimeMS++;
            }
            _networkTimeMS += advanceTimeMS;
        }

        _fPSCount++;
        if (_elapsedTime >= _fPSTime) FPSUpdate();

        const auto now = std::chrono::system_clock::now();
        const std::time_t nowC = std::chrono::system_clock::to_time_t(now);
        std::tm localTime{};
        if (const auto* localTimePtr = std::localtime(&nowC)) localTime = *localTimePtr;

        const unsigned long long timeOfDay =
            static_cast<unsigned long long>(localTime.tm_hour) * 3600ULL +
            static_cast<unsigned long long>(localTime.tm_min) * 60ULL +
            static_cast<unsigned long long>(localTime.tm_sec);

        _audioMaterial->SetVector(
            ShaderProperties::_advancedTimeProps,
            Vector4(
                static_cast<float>(_elapsedTime),
                static_cast<float>(_elapsedTimeMSW),
                static_cast<float>(timeOfDay),
                ConfigProperties::READBACK_TIME));

        const double utcSecondsUnix =
            std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

        _audioMaterial->SetVector(
            ShaderProperties::_advancedTimeProps2,
            Vector4(
                static_cast<float>(_networkTimeMS & kNetworkTimeLowMask),
                static_cast<float>(_networkTimeMS >> 16),
                static_cast<float>(std::floor(utcSecondsUnix / kSecondsPerDay)),
                static_cast<float>(std::fmod(utcSecondsUnix, kSecondsPerDay))));

        if (!IsUnityObjectAlive(_audioSource)) {
            _audioSource = nullptr;
            return;
        }

        SendAudioOutputData();

        // Use the generated 1.40.8 methods instead of a private spatial-blend icall.
        _audioMaterial->SetFloat(ShaderProperties::_sourceVolume, _audioSource->get_volume());
        _audioMaterial->SetFloat(ShaderProperties::_sourceSpatialBlend, _audioSource->get_spatialBlend());
    }

    void AudioLinkObj::UpdateSettings() {
        AudioLinkLogger.info("Updating Settings");
        if (!IsUnityObjectAlive(_audioMaterial)) return;

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

    void AudioLinkObj::UpdateThemeColors() {
        AudioLinkLogger.info("Updating Color Scheme");
        if (!IsUnityObjectAlive(_audioMaterial)) return;

        _audioMaterial->SetInt(ShaderProperties::_themeColorMode, ConfigProperties::THEME_COLOR_MODE);
        _audioMaterial->SetColor(ShaderProperties::_customThemeColor0ID, _customThemeColor0);
        _audioMaterial->SetColor(ShaderProperties::_customThemeColor1ID, _customThemeColor1);
        _audioMaterial->SetColor(ShaderProperties::_customThemeColor2ID, _customThemeColor2);
        _audioMaterial->SetColor(ShaderProperties::_customThemeColor3ID, _customThemeColor3);
    }

    void AudioLinkObj::FPSUpdate() {
        if (IsUnityObjectAlive(_audioMaterial)) {
            _audioMaterial->SetVector(
                ShaderProperties::_versionNumberAndFPSProperty,
                Vector4(ConfigProperties::AUDIOLINK_VERSION_NUMBER, 0, _fPSCount, 1));
            _audioMaterial->SetVector(ShaderProperties::_playerCountAndData, {0, 0, 0, 0});
        }

        _fPSCount = 0;
        _fPSTime++;

        constexpr double elapsedTimeMSWBoundary = 1024.0;
        if (_elapsedTime >= elapsedTimeMSWBoundary) {
            _fPSTime = 0;
            _elapsedTime -= elapsedTimeMSWBoundary;
            _elapsedTimeMSW++;
        }

        const int networkTimeMSNow = static_cast<int>(Time::get_time() * 1000.0f);
        const int networkTimeDelta = networkTimeMSNow - _networkTimeMS;
        if (networkTimeDelta > 3000 || networkTimeDelta < -3000) {
            _networkTimeMS = networkTimeMSNow;
        } else {
            _networkTimeMS += networkTimeDelta / 20;
        }
    }

    void AudioLinkObj::SendAudioOutputData() {
        if (!IsUnityObjectAlive(_audioMaterial) || !IsUnityObjectAlive(_audioSource)) return;
        if (_audioFramesL.size() < kCaptureSampleCount ||
            _audioFramesR.size() < kCaptureSampleCount ||
            _samples.size() < kBandSize) {
            AudioLinkLogger.error("AudioLink sample buffers are not correctly initialized.");
            return;
        }

        if (!GetOutputDataSafe(_audioSource, _audioFramesL, 0)) return;

        // Beat Saber songs use AudioClips. Avoid querying channel 1 when the clip
        // is mono or missing; mirror left instead. This is safer than probing a
        // potentially invalid channel and guessing from a single zero sample.
        bool hasRightChannel = false;
        auto clip = _audioSource->get_clip();
        if (IsUnityObjectAlive(clip)) hasRightChannel = clip->get_channels() > 1;

        if (hasRightChannel) {
            if (!GetOutputDataSafe(_audioSource, _audioFramesR, 1)) {
                std::memcpy(
                    _audioFramesR.begin(),
                    _audioFramesL.begin(),
                    sizeof(float) * kCaptureSampleCount);
            }
        } else {
            std::memcpy(
                _audioFramesR.begin(),
                _audioFramesL.begin(),
                sizeof(float) * kCaptureSampleCount);
        }

        static const int samplesLProps[kBandCount] = {
            ShaderProperties::_samples0L,
            ShaderProperties::_samples1L,
            ShaderProperties::_samples2L,
            ShaderProperties::_samples3L
        };
        static const int samplesRProps[kBandCount] = {
            ShaderProperties::_samples0R,
            ShaderProperties::_samples1R,
            ShaderProperties::_samples2R,
            ShaderProperties::_samples3R
        };

        for (int band = 0; band < kBandCount; ++band) {
            CopyBand(_samples, _audioFramesL, band);
            _audioMaterial->SetFloatArray(samplesLProps[band], _samples);

            CopyBand(_samples, _audioFramesR, band);
            _audioMaterial->SetFloatArray(samplesRProps[band], _samples);
        }
    }

    Sombrero::FastColor AudioLinkObj::get_customThemeColor0() { return _customThemeColor0; }
    Sombrero::FastColor AudioLinkObj::get_customThemeColor1() { return _customThemeColor1; }
    Sombrero::FastColor AudioLinkObj::get_customThemeColor2() { return _customThemeColor2; }
    Sombrero::FastColor AudioLinkObj::get_customThemeColor3() { return _customThemeColor3; }
    AudioSource* AudioLinkObj::get_audioSource() { return _audioSource; }
    Material* AudioLinkObj::get_audioMaterial() { return _audioMaterial; }
    ArrayW<float> AudioLinkObj::get_audioFramesL() { return _audioFramesL; }
    ArrayW<float> AudioLinkObj::get_audioFramesR() { return _audioFramesR; }
    ArrayW<float> AudioLinkObj::get_samples() { return _samples; }
}
