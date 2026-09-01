#include "AssetBundleManager.hpp"

#include "ShaderProperties.hpp"
#include "UnityEngine/AssetBundleRequest.hpp"
#include "UnityEngine/AssetBundleCreateRequest.hpp"

#include "assets.hpp"

DEFINE_TYPE(AudioLink, AssetBundleManager);

namespace AudioLink {
    UnityEngine::Material* AssetBundleManager::get_material() {
        return _material;
    }

    UnityEngine::RenderTexture* AssetBundleManager::get_renderTexture() {
        return _renderTexture;
    }

    void AssetBundleManager::Load() {
        if (_bundle && _bundle->m_CachedPtr.m_value) return;

        static auto assetBundle_LoadFromMemory = il2cpp_utils::resolve_icall<
            UnityEngine::AssetBundle*, ArrayW<uint8_t>, int>(
            "UnityEngine.AssetBundle::LoadFromMemory_Internal");

        static auto _bundle_info = il2cpp_functions::class_get_field_from_name(klass, "_bundle");
        static auto _material_info = il2cpp_functions::class_get_field_from_name(klass, "_material");
        static auto _renderTexture_info = il2cpp_functions::class_get_field_from_name(klass, "_renderTexture");

        if (!assetBundle_LoadFromMemory) {
            AudioLinkLogger.error("Could not resolve AssetBundle::LoadFromMemory_Internal; AudioLink assets will stay disabled.");
            return;
        }

        // Clear stale references before attempting a new load. This keeps a failed
        // retry from exposing half-initialized Unity objects to AudioLinkObj.
        il2cpp_functions::field_set_value_object(this, _bundle_info, nullptr);
        il2cpp_functions::field_set_value_object(this, _material_info, nullptr);
        il2cpp_functions::field_set_value_object(this, _renderTexture_info, nullptr);

        auto bundle = assetBundle_LoadFromMemory(IncludedAssets::Bundle, 0);
        if (!bundle || !bundle->m_CachedPtr.m_value) {
            // Unity documents LoadFromMemory as returning null on failure. Older
            // versions of this port dereferenced that null immediately, which can
            // crash Beat Saber while transitioning from Continue into the menu.
            AudioLinkLogger.error("AudioLink asset bundle failed to load; continuing without AudioLink rendering.");
            return;
        }
        il2cpp_functions::field_set_value_object(this, _bundle_info, bundle);

        auto material = bundle->LoadAsset<UnityEngine::Material*>(
            "assets/audiolink/materials/mat_audiolink.mat");
        if (!material || !material->m_CachedPtr.m_value) {
            AudioLinkLogger.error("AudioLink material was not found in the asset bundle; disabling AudioLink rendering.");
            bundle->Unload(true);
            il2cpp_functions::field_set_value_object(this, _bundle_info, nullptr);
            return;
        }
        il2cpp_functions::field_set_value_object(this, _material_info, material);

        auto renderTexture = bundle->LoadAsset<UnityEngine::RenderTexture*>(
            "assets/audiolink/rendertextures/rt_audiolink.asset");
        if (!renderTexture || !renderTexture->m_CachedPtr.m_value) {
            AudioLinkLogger.error("AudioLink render texture was not found in the asset bundle; disabling AudioLink rendering.");
            bundle->Unload(true);
            il2cpp_functions::field_set_value_object(this, _bundle_info, nullptr);
            il2cpp_functions::field_set_value_object(this, _material_info, nullptr);
            return;
        }
        il2cpp_functions::field_set_value_object(this, _renderTexture_info, renderTexture);

        AudioLinkLogger.info("AudioLink asset bundle loaded successfully.");
    }

    void AssetBundleManager::Dispose() {
        AudioLinkLogger.info("AssetBundleManager Dispose");
        static auto _bundle_info = il2cpp_functions::class_get_field_from_name(klass, "_bundle");
        static auto _material_info = il2cpp_functions::class_get_field_from_name(klass, "_material");
        static auto _renderTexture_info = il2cpp_functions::class_get_field_from_name(klass, "_renderTexture");

        if (_bundle && _bundle->m_CachedPtr.m_value) {
            // Unload(true) already destroys objects loaded from this bundle. Do not
            // DestroyImmediate the material/render texture a second time afterwards.
            _bundle->Unload(true);
        }

        il2cpp_functions::field_set_value_object(this, _bundle_info, nullptr);
        il2cpp_functions::field_set_value_object(this, _material_info, nullptr);
        il2cpp_functions::field_set_value_object(this, _renderTexture_info, nullptr);
    }
}
