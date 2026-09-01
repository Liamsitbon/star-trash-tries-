#include "Providers/MenuProvider.hpp"

#include "GlobalNamespace/PlayerData.hpp"
#include "GlobalNamespace/ColorSchemesSettings.hpp"
#include "ShaderProperties.hpp"
DEFINE_TYPE(AudioLink, MenuProvider);


namespace AudioLink {
    MenuProvider* MenuProvider::instance;
    MenuProvider* MenuProvider::get_instance() {
        return instance;
    }

    void MenuProvider::ctor(AudioLinkObj* audioLink, GlobalNamespace::PlayerDataModel* playerDataModel) {
        AudioLinkLogger.info("MenuProvider ctor!");
        static auto _audioLink_info = il2cpp_functions::class_get_field_from_name(klass, "_audioLink");
        static auto _playerDataModel_info = il2cpp_functions::class_get_field_from_name(klass, "_playerDataModel");

        il2cpp_functions::field_set_value_object(this, _audioLink_info, audioLink);
        il2cpp_functions::field_set_value_object(this, _playerDataModel_info, playerDataModel);
        instance = this;
    }

    void MenuProvider::dtor() {
        if (instance == this) instance = nullptr;
    }

    void MenuProvider::SongPreviewPlayerProvide(
        int activeChannel,
        ArrayW<GlobalNamespace::SongPreviewPlayer::AudioSourceVolumeController*> audioSourceControllers) {

        if (!_audioLink) {
            AudioLinkLogger.info("Skipping menu audio source: AudioLinkObj is not available.");
            return;
        }

        const auto controllerCount = static_cast<int>(audioSourceControllers.size());
        if (activeChannel < 0 || activeChannel >= controllerCount) {
            AudioLinkLogger.info(
                "Skipping menu audio source: active channel {} is outside controller count {}.",
                activeChannel,
                controllerCount);
            return;
        }

        auto controller = audioSourceControllers[activeChannel];
        if (!controller) {
            AudioLinkLogger.info("Skipping menu audio source: active controller is null.");
            return;
        }

        auto audioSource = controller->audioSource;
        if (!audioSource || !audioSource->m_CachedPtr.m_value) {
            AudioLinkLogger.info("Skipping menu audio source: active AudioSource is not alive yet.");
            return;
        }

        _audioLink->SetAudioSource(audioSource);
    }

    void MenuProvider::ColorManagerInstallerProvide(GlobalNamespace::ColorSchemeSO* menuColorScheme) {
        if (!_audioLink || !_playerDataModel || !menuColorScheme) {
            AudioLinkLogger.info("Skipping menu color scheme: menu dependencies are not ready.");
            return;
        }

        auto playerData = _playerDataModel->playerData;
        if (!playerData || !playerData->colorSchemesSettings) {
            AudioLinkLogger.info("Skipping menu color scheme: PlayerData color settings are not ready.");
            return;
        }

        auto overrideColorScheme = playerData->colorSchemesSettings->GetOverrideColorScheme();
        auto fallbackColorScheme = menuColorScheme->get_colorScheme();
        auto colorScheme = overrideColorScheme ? overrideColorScheme : fallbackColorScheme;

        if (!colorScheme) {
            AudioLinkLogger.info("Skipping menu color scheme: no valid ColorScheme was available.");
            return;
        }

        _audioLink->SetColorScheme(colorScheme);
    }
}
