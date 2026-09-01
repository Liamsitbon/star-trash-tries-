#include "Providers/GameProvider.hpp"
#include "ShaderProperties.hpp"

DEFINE_TYPE(AudioLink, GameProvider);

namespace AudioLink {
    
    void GameProvider::ctor(AudioLinkObj* audioLink, GlobalNamespace::AudioTimeSyncController* audioTimeSyncController, GlobalNamespace::ColorScheme* colorScheme) {
        static auto _audioLink_info = il2cpp_functions::class_get_field_from_name(klass, "_audioLink");
        static auto _audioTimeSyncController_info = il2cpp_functions::class_get_field_from_name(klass, "_audioTimeSyncController");
        static auto _colorScheme_info = il2cpp_functions::class_get_field_from_name(klass, "_colorScheme");
        
        il2cpp_functions::field_set_value_object(this, _audioLink_info, audioLink);
        il2cpp_functions::field_set_value_object(this, _audioTimeSyncController_info, audioTimeSyncController);
        il2cpp_functions::field_set_value_object(this, _colorScheme_info, colorScheme);
    }

    void GameProvider::Initialize() {
        AudioLinkLogger.info("GameProvider Initialize");

        if (!_audioLink || !_audioTimeSyncController) {
            AudioLinkLogger.info("Skipping gameplay AudioLink binding: dependencies are not ready.");
            return;
        }

        auto audioSource = _audioTimeSyncController->_audioSource;
        if (audioSource && audioSource->m_CachedPtr.m_value) {
            _audioLink->SetAudioSource(audioSource);
        } else {
            AudioLinkLogger.info("Skipping gameplay audio source: AudioSource is not alive yet.");
        }

        if (_colorScheme) {
            _audioLink->SetColorScheme(_colorScheme);
        } else {
            AudioLinkLogger.info("Skipping gameplay color scheme: ColorScheme is null.");
        }
    }
}
