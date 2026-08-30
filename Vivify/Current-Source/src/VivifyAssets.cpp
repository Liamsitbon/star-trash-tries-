#include "VivifyRuntimeInternal.hpp"
#include "VivifyComponents.hpp"
#include "QuestInterop.hpp"
#include <array>
#include <charconv>
#include <cstring>
#include <limits>
#include <vector>

namespace Vivify {

namespace {

bool IsDesktopBundleName(std::filesystem::path const& path) {
  std::string filename = path.filename().string();
  std::transform(filename.begin(), filename.end(), filename.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return filename.find("windows") != std::string::npos ||
         filename.find("win64") != std::string::npos ||
         filename.find("macos") != std::string::npos ||
         filename.find("osx") != std::string::npos;
}

std::vector<std::string> ResolveBundlePaths(std::string const& levelPath) {
  std::vector<std::string> paths;
  std::string bundlePath = JoinPath(levelPath, kBundleFile);
  if (std::filesystem::exists(bundlePath)) {
    paths.push_back(bundlePath);
  }
  std::error_code ec;
  for (auto const& entry : std::filesystem::directory_iterator(levelPath, ec)) {
    if (ec) break;
    auto const& p = entry.path();
    std::string extension = p.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    // Quest must never probe a desktop player bundle just because it shares
    // the .vivify extension. AssetBundle.LoadFromFile can return a wrapper for
    // an incompatible bundle before an asset lookup fails much later, which
    // made Windows-only maps look partially functional and left stale state.
    if (extension != ".vivify" || p.string() == bundlePath || IsDesktopBundleName(p)) continue;
    paths.push_back(p.string());
  }
  if (paths.size() > 1) {
    std::sort(paths.begin() + (paths.front() == bundlePath ? 1 : 0), paths.end());
  }
  return paths;
}

std::string ResolveBundlePath(std::string const& levelPath) {
  auto paths = ResolveBundlePaths(levelPath);
  return paths.empty() ? std::string{} : std::move(paths.front());
}

uint32_t ReadAndroidChecksumFromInfoDat(std::string const& levelPath) {
  std::string infoPath = JoinPath(levelPath, "Info.dat");
  if (!std::filesystem::exists(infoPath)) infoPath = JoinPath(levelPath, "info.dat");
  if (!std::filesystem::exists(infoPath)) return 0;
  std::ifstream ifs(infoPath);
  if (!ifs.is_open()) return 0;
  std::string str((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
  rapidjson::Document doc;
  doc.Parse(str.c_str());
  if (doc.HasParseError() || !doc.IsObject()) return 0;
  rapidjson::Value const* customData = nullptr;
  if (doc.HasMember("_customData")) customData = &doc["_customData"];
  else if (doc.HasMember("customData")) customData = &doc["customData"];
  if (customData == nullptr || !customData->IsObject()) return 0;
  rapidjson::Value const* assetBundle = nullptr;
  if (customData->HasMember("_assetBundle")) assetBundle = &(*customData)["_assetBundle"];
  else if (customData->HasMember("assetBundle")) assetBundle = &(*customData)["assetBundle"];
  if (assetBundle == nullptr || !assetBundle->IsObject()) return 0;
  for (char const* key : {"_android2021", "android2021"}) {
    if (!assetBundle->HasMember(key)) continue;
    auto const& value = (*assetBundle)[key];
    if (value.IsUint()) return value.GetUint();
    if (value.IsUint64() && value.GetUint64() <= std::numeric_limits<uint32_t>::max()) {
      return static_cast<uint32_t>(value.GetUint64());
    }
    if (value.IsString()) {
      uint64_t parsed = 0;
      char const* begin = value.GetString();
      char const* end = begin + value.GetStringLength();
      auto [position, error] = std::from_chars(begin, end, parsed);
      if (error == std::errc{} && position == end &&
          parsed <= std::numeric_limits<uint32_t>::max()) {
        return static_cast<uint32_t>(parsed);
      }
    }
  }
  return 0;
}

uint32_t ReadWindowsChecksumFromInfoDat(std::string const& levelPath) {
  std::string infoPath = JoinPath(levelPath, "Info.dat");
  if (!std::filesystem::exists(infoPath)) infoPath = JoinPath(levelPath, "info.dat");
  if (!std::filesystem::exists(infoPath)) return 0;

  std::ifstream ifs(infoPath);
  if (!ifs.is_open()) return 0;
  std::string str((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
  rapidjson::Document doc;
  doc.Parse(str.c_str());
  if (doc.HasParseError() || !doc.IsObject()) return 0;

  rapidjson::Value const* customData = nullptr;
  if (doc.HasMember("_customData")) customData = &doc["_customData"];
  else if (doc.HasMember("customData")) customData = &doc["customData"];
  if (customData == nullptr || !customData->IsObject()) return 0;

  rapidjson::Value const* assetBundle = nullptr;
  if (customData->HasMember("_assetBundle")) assetBundle = &(*customData)["_assetBundle"];
  else if (customData->HasMember("assetBundle")) assetBundle = &(*customData)["assetBundle"];
  if (assetBundle == nullptr || !assetBundle->IsObject()) return 0;

  for (char const* key : {"_windows2021", "windows2021", "_windows2019", "windows2019", "_windows", "windows"}) {
    if (!assetBundle->HasMember(key)) continue;
    auto const& value = (*assetBundle)[key];
    if (value.IsUint()) return value.GetUint();
    if (value.IsUint64() && value.GetUint64() <= std::numeric_limits<uint32_t>::max()) {
      return static_cast<uint32_t>(value.GetUint64());
    }
    if (value.IsString()) {
      uint64_t parsed = 0;
      char const* begin = value.GetString();
      char const* end = begin + value.GetStringLength();
      auto [position, error] = std::from_chars(begin, end, parsed);
      if (error == std::errc{} && position == end &&
          parsed <= std::numeric_limits<uint32_t>::max()) {
        return static_cast<uint32_t>(parsed);
      }
    }
  }
  return 0;
}

bool HasLocalWindowsBundle(std::string const& levelPath) {
  std::error_code ec;
  for (auto const& entry : std::filesystem::directory_iterator(levelPath, ec)) {
    if (ec) break;
    auto const& path = entry.path();
    if (!entry.is_regular_file(ec) || ec) continue;

    std::string extension = path.extension().string();
    std::string filename = path.filename().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(filename.begin(), filename.end(), filename.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (extension == ".vivify" && filename.find("windows") != std::string::npos) return true;
  }
  return false;
}

void AddWindowsOnlyBundleWarning(SongCore::API::LevelSelect::LevelWasSelectedEventArgs const& event) {
  if (!event.customLevelDetails.has_value()) return;

  static std::string const warning =
      "Quest warning: this map only provides a Windows Vivify bundle. No Android bundle was found. "
      "Visuals may be missing, incorrect, white/grey, or uncomfortable, but playback is allowed.";

  auto const& warningsConst = event.customLevelDetails->difficultyDetails.warnings;
  auto& warnings = const_cast<std::vector<std::string>&>(warningsConst);
  if (std::find(warnings.begin(), warnings.end(), warning) == warnings.end()) {
    warnings.push_back(warning);
  }
}

bool IsUnityBundleData(std::vector<uint8_t> const& data) {
  static constexpr char kUnityFsMagic[] = "UnityFS";
  return data.size() >= sizeof(kUnityFsMagic) &&
         std::memcmp(data.data(), kUnityFsMagic, sizeof(kUnityFsMagic) - 1) == 0;
}

struct MaterialFallbackState {
  std::optional<UnityEngine::Color> color;
  UnityEngine::Texture* mainTexture = nullptr;
  int renderQueue = -1;
};

std::optional<UnityEngine::Color> ReadMaterialFallbackColor(UnityEngine::Material* material) {
  if (!IsManagedAlive(material)) return std::nullopt;
  static int const colorIds[] = {
      UnityEngine::Shader::PropertyToID(u"_Color"),
      UnityEngine::Shader::PropertyToID(u"_BaseColor"),
      UnityEngine::Shader::PropertyToID(u"_TintColor"),
      UnityEngine::Shader::PropertyToID(u"_MainColor"),
      UnityEngine::Shader::PropertyToID(u"_HorizonCol"),
      UnityEngine::Shader::PropertyToID(u"_SkyCol"),
      UnityEngine::Shader::PropertyToID(u"_EmissionColor"),
  };
  for (int id : colorIds) {
    if (material->HasProperty(id)) {
      return material->GetColor(id);
    }
  }
  auto names = material->GetPropertyNames(UnityEngine::MaterialPropertyType::Vector);
  if (!names) return std::nullopt;
  for (auto name : names) {
    if (!name) continue;
    std::string key = NormalizeAssetKey(ToStdString(name));
    if (key.find("color") == std::string::npos &&
        key.find("colour") == std::string::npos &&
        key.find("col") == std::string::npos) {
      continue;
    }
    return material->GetColor(name);
  }
  return std::nullopt;
}

MaterialFallbackState CaptureMaterialFallbackState(UnityEngine::Material* material) {
  MaterialFallbackState state;
  if (!IsManagedAlive(material)) return state;
  state.color = ReadMaterialFallbackColor(material);
  state.mainTexture = material->get_mainTexture().unsafePtr();
  state.renderQueue = material->get_renderQueue();
  return state;
}

void RestoreMaterialFallbackState(UnityEngine::Material* material, MaterialFallbackState const& state) {
  if (!IsManagedAlive(material)) return;
  if (state.color.has_value()) {
    static int const fallbackColorIds[] = {
        UnityEngine::Shader::PropertyToID(u"_Color"),
        UnityEngine::Shader::PropertyToID(u"_BaseColor"),
        UnityEngine::Shader::PropertyToID(u"_TintColor"),
   };
    for (int id : fallbackColorIds) {
      if (material->HasProperty(id)) {
        material->SetColor(id, state.color.value());
      }
    }
  }
  if (IsManagedAlive(state.mainTexture)) {
    material->set_mainTexture(state.mainTexture);
  }
  if (state.renderQueue >= 0) {
    material->set_renderQueue(state.renderQueue);
  }
}
}

void Runtime::HandleLevelSelected(SongCore::API::LevelSelect::LevelWasSelectedEventArgs const& event) {

  // Vivify changes presentation, not scoring. Do not self-disable BeatLeader
  // or ScoreSaber uploads through MetaCore. Other mods and both leaderboard
  // clients keep their own submission guards and server-side validation.
  MetaCore::Game::SetScoreSubmission("Vivify", true);

  std::string incomingLevelPath;
  if (event.isCustom && event.customBeatmapLevel != nullptr) {
    incomingLevelPath = std::string(event.customBeatmapLevel->customLevelPath);
  }
  // An empty incoming path means we selected an official/non-custom level.
  // That is still a level change and must retire the previous Vivify bundle.
  bool const changingLevel = incomingLevelPath != _selectedLevelPath;

  // LevelWasSelected runs after the previous gameplay scene has already torn
  // down many Unity objects. Never probe, restore, Release(), or Destroy() raw
  // pointers from that old scene here: the native object can already be gone
  // even when the managed wrapper is still non-null. Drop those references
  // first, then retire the old bundle and its loaded asset objects.  Keeping
  // those objects alive with Unload(false) made every browsed Vivify map add
  // another complete bundle to Quest's process RSS.  Android then killed Beat
  // Saber with LOW_MEMORY after only a few map changes.  ResetRuntime above is
  // pointer-free for this late-transition path and clears every Vivify-owned
  // asset/scene reference before the immediate bundle retirement.
  ResetRuntime(ResetMode::LateSceneTransition);
  if (changingLevel) {
    if (_mainBundle != nullptr && UnityEngine::Object::op_Implicit_bool(_mainBundle)) {
      _mainBundle->Unload(true);
      _mainBundle = nullptr;
      VIVIFY_DEBUG("Vivify retired the previous map bundle and released its loaded assets");
    }
    _preloadedBundlePath.clear();
  }

  _activeSabers.clear();
  _selectedLevelPath.clear();
  _selectedMapHasVivifyRequirement = false;
  _warnedMissingBundleEvents = false;
  if (!event.isCustom || event.customBeatmapLevel == nullptr) {
    SongCore::API::PlayButton::EnablePlayButton("Vivify");
    return;
  }
  _selectedLevelPath = std::string(event.customBeatmapLevel->customLevelPath);
  QuestModInterop::PeerSet requiredPeers;
  if (event.customLevelDetails) {
    requiredPeers = QuestModInterop::RequiredPeers(
        event.customLevelDetails->difficultyDetails.requirements);
  }
  auto const installedPeers = QuestModInterop::InstalledPeers();
  PaperLogger.info(
      "Vivify interop: installed[C={} N={} NE={} V={}] required[C={} N={} NE={} V={}]",
      installedPeers.cinema, installedPeers.nexora,
      installedPeers.noodleExtensions, installedPeers.vivify,
      requiredPeers.cinema, requiredPeers.nexora,
      requiredPeers.noodleExtensions, requiredPeers.vivify);
  if (GetVivifyDebugLogging()) {
    PaperLogger.info("Vivify level selected: path='{}' isCustom={} hasDetails={}",
                     _selectedLevelPath, BoolText(event.isCustom), BoolText(event.customLevelDetails.has_value()));
  }

  // SongCore normally supplies the selected difficulty's requirements here.
  // Some valid v3 maps (including older converted maps) expose their Android
  // Vivify bundle but omit that requirement from this selection payload.  Do
  // not silently turn those maps into vanilla gameplay: a local Android
  // bundle or an android2021 checksum is an unambiguous, map-scoped signal
  // that Vivify must be enabled.  We intentionally do not use a Windows-only
  // bundle as a fallback, because it is unsafe to execute on Quest.
  std::string const androidBundlePath = JoinPath(_selectedLevelPath, std::string(kBundleFile));
  bool const hasAndroidBundle = std::filesystem::exists(androidBundlePath);
  uint32_t const androidChecksum = ReadAndroidChecksumFromInfoDat(_selectedLevelPath);
  uint32_t const windowsChecksum = ReadWindowsChecksumFromInfoDat(_selectedLevelPath);
  bool const hasWindowsBundle = HasLocalWindowsBundle(_selectedLevelPath) || windowsChecksum != 0;
  bool const windowsOnlyBundle = !hasAndroidBundle && androidChecksum == 0 && hasWindowsBundle;

  if (event.customLevelDetails) {
    auto const& requirements = event.customLevelDetails->difficultyDetails.requirements;
    _selectedMapHasVivifyRequirement = std::any_of(requirements.begin(), requirements.end(), [](std::string const& requirement) {
      return NormalizeAssetKey(requirement) == NormalizeAssetKey(kCapability);
    });
  }
  if (!_selectedMapHasVivifyRequirement && (hasAndroidBundle || androidChecksum != 0)) {
    _selectedMapHasVivifyRequirement = true;
    PaperLogger.warn(
        "Vivify capability fallback activated for '{}': SongCore did not report the Vivify requirement, but an Android Vivify bundle is present (local={}, android2021={})",
        _selectedLevelPath, BoolText(hasAndroidBundle), androidChecksum);
  }
  if (!_selectedMapHasVivifyRequirement) {
    SongCore::API::PlayButton::EnablePlayButton("Vivify");
    return;
  }

  if (GetQuestCompatibilityMode()) {
    // Do not load or download desktop-authored Vivify bundles in recovery
    // mode.  The event stream is also ignored in HandleCustomEvent, letting
    // SongCore start the chart as normal Beat Saber gameplay.
    PaperLogger.warn(
        "Vivify Quest compatibility mode is active; skipping map effects for '{}'.",
        _selectedLevelPath);
    SongCore::API::PlayButton::EnablePlayButton("Vivify");
    return;
  }

  if (windowsOnlyBundle) {
    AddWindowsOnlyBundleWarning(event);
    PaperLogger.warn(
        "Vivify map is Windows-bundle-only: path='{}' windowsChecksum={} — allowing play with a non-blocking Quest warning",
        _selectedLevelPath, windowsChecksum);
  }
  auto bundlePaths = ResolveBundlePaths(_selectedLevelPath);

  if (hasAndroidBundle) {
    if (GetVivifyDebugLogging()) {
      PaperLogger.info("Vivify bundle selection: using local Android bundle '{}'", androidBundlePath);
    }
    if (PreloadBundle(androidBundlePath)) {
      SongCore::API::PlayButton::EnablePlayButton("Vivify");
      return;
    }
    PaperLogger.warn("Vivify could not load the local Android bundle '{}'; trying the map checksum",
                     androidBundlePath);
  }

  for (auto const& candidate : bundlePaths) {
    if (candidate == androidBundlePath) continue;
    if (GetVivifyDebugLogging()) {
      PaperLogger.info("Vivify bundle selection: probing non-standard bundle path '{}'", candidate);
    }
    if (PreloadBundle(candidate)) {
      PaperLogger.info("Vivify accepted a compatible Android bundle with a non-standard filename: '{}'",
                       candidate);
      SongCore::API::PlayButton::EnablePlayButton("Vivify");
      return;
    }
  }

  if (GetVivifyDebugLogging()) {
    PaperLogger.info("Vivify bundle selection: no local bundle in '{}' android2021={}",
                     _selectedLevelPath, androidChecksum);
  }
  if (androidChecksum != 0) {
    SongCore::API::PlayButton::DisablePlayButton("Vivify", "Downloading assets...");
    std::string levelPath = _selectedLevelPath;
    DownloadBundle(androidChecksum, levelPath, [this, levelPath](bool success) {
      if (levelPath != _selectedLevelPath) {
        if (GetVivifyDebugLogging()) {
          PaperLogger.info("Vivify ignored a completed download for a level that is no longer selected: '{}'",
                           levelPath);
        }
        return;
      }
      if (!success) {
        SongCore::API::PlayButton::DisablePlayButton("Vivify", "Failed to download assets.");
        return;
      }
      std::string downloaded = ResolveBundlePath(levelPath);
      if (!downloaded.empty() && PreloadBundle(downloaded)) {
        SongCore::API::PlayButton::EnablePlayButton("Vivify");
      } else {
        SongCore::API::PlayButton::DisablePlayButton(
            "Vivify", "Downloaded Android assets are invalid or incompatible.");
      }
    });
  } else if (windowsOnlyBundle) {
    // Windows-only Vivify maps are intentionally non-blocking on Quest. The map may still be playable,
    // while unsupported or missing visuals are explained in SongCore's Requirements/Warnings modal.
    SongCore::API::PlayButton::EnablePlayButton("Vivify");
  } else {
    SongCore::API::PlayButton::DisablePlayButton(
        "Vivify", "No compatible Android asset bundle is available for this map.");
  }
}

void Runtime::DownloadBundle(uint32_t checksum, std::string const& levelPath, std::function<void(bool)> callback) {
  std::string url = "https://repo.totalbs.dev/api/v1/bundles/" + std::to_string(checksum);
  std::string bundlePath = JoinPath(levelPath, kBundleFile);
  if (GetVivifyDebugLogging()) {
    PaperLogger.info("Vivify bundle download: android2021={} metadataUrl='{}' cachePath='{}'",
                     checksum, url, bundlePath);
  }
  WebUtils::GetAsync<WebUtils::StringResponse>(WebUtils::URLOptions(url), [bundlePath, callback, url](WebUtils::StringResponse res) {
    if (!res.IsSuccessful() || !res.responseData.has_value()) {
      if (GetVivifyDebugLogging()) {
        PaperLogger.warn("Vivify bundle download failed: metadata request unsuccessful url='{}'", url);
      }
      BSML::MainThreadScheduler::Schedule([callback] { callback(false); });
      return;
    }
    rapidjson::Document doc;
    doc.Parse(res.responseData->c_str());
    if (doc.HasParseError() || !doc.IsObject() ||
        !doc.HasMember("downloadUrl") || !doc["downloadUrl"].IsString()) {
      if (GetVivifyDebugLogging()) {
        PaperLogger.warn("Vivify bundle download failed: metadata response did not contain downloadUrl");
      }
      BSML::MainThreadScheduler::Schedule([callback] { callback(false); });
      return;
    }
    std::string downloadUrl = doc["downloadUrl"].GetString();
    if (GetVivifyDebugLogging()) {
      PaperLogger.info("Vivify bundle download URL resolved: '{}'", downloadUrl);
    }
    WebUtils::GetAsync<WebUtils::DataResponse>(WebUtils::URLOptions(downloadUrl), [bundlePath, callback](WebUtils::DataResponse dataRes) {
      if (!dataRes.IsSuccessful() || !dataRes.responseData.has_value()) {
        if (GetVivifyDebugLogging()) {
          PaperLogger.warn("Vivify bundle download failed: data request unsuccessful path='{}'", bundlePath);
        }
        BSML::MainThreadScheduler::Schedule([callback] { callback(false); });
        return;
      }
      auto const& data = *dataRes.responseData;
      if (!IsUnityBundleData(data)) {
        PaperLogger.warn("Vivify bundle download rejected invalid data: path='{}' bytes={}",
                         bundlePath, data.size());
        BSML::MainThreadScheduler::Schedule([callback] { callback(false); });
        return;
      }

      std::string const temporaryPath = bundlePath + ".part";
      std::error_code ec;
      std::filesystem::remove(temporaryPath, ec);
      ec.clear();
      std::ofstream os(temporaryPath, std::ios::binary | std::ios::trunc);
      bool written = os.is_open();
      if (written) {
        os.write(reinterpret_cast<char const*>(data.data()),
                 static_cast<std::streamsize>(data.size()));
        os.flush();
        written = os.good();
        os.close();
      }
      if (written) {
        std::filesystem::rename(temporaryPath, bundlePath, ec);
        if (ec) {
          std::filesystem::remove(bundlePath, ec);
          ec.clear();
          std::filesystem::rename(temporaryPath, bundlePath, ec);
        }
        written = !ec;
      }
      if (!written) {
        ec.clear();
        std::filesystem::remove(temporaryPath, ec);
      }
      if (GetVivifyDebugLogging()) {
        PaperLogger.info("Vivify bundle download complete: path='{}' bytes={} written={}",
                         bundlePath, data.size(), BoolText(written));
      }
      BSML::MainThreadScheduler::Schedule([callback, written] { callback(written); });
    });
  });
}

void Runtime::CacheBundleAssets() {
  if (_mainBundle == nullptr || !UnityEngine::Object::op_Implicit_bool(_mainBundle)) return;
  auto assetNames = _mainBundle->GetAllAssetNames();
  if (!assetNames) return;
  _assets.clear();
  _assetsByName.clear();
  _ambiguousAssetAliases.clear();
  _missingAssetKeys.clear();
  _missingInstantiatePrefabAssets.clear();
  _supportedShadersByName.clear();
  auto addUniqueAlias = [this](std::string_view alias, UnityEngine::Object* asset) {
    std::string const key = NormalizeAssetKey(alias);
    if (key.empty() || asset == nullptr || _ambiguousAssetAliases.contains(key)) return;
    auto [it, inserted] = _assetsByName.emplace(key, asset);
    if (!inserted && it->second != asset) {
      _assetsByName.erase(it);
      _ambiguousAssetAliases.emplace(key);
    }
  };
  for (auto assetName : assetNames) {
    if (!assetName) continue;
    std::string originalAssetPath = il2cpp_utils::detail::to_string(assetName);
    std::string key = NormalizeAssetKey(originalAssetPath);
    auto asset = _mainBundle->LoadAsset(assetName);
    if (asset == nullptr) {
      if (GetVivifyDebugLogging()) {
        PaperLogger.warn("Vivify asset load failed: path='{}'", originalAssetPath);
      }
      continue;
    }
    if (!key.empty()) _assets[key] = asset;
    std::string const filename = std::filesystem::path(originalAssetPath).filename().string();
    std::string const stem = std::filesystem::path(filename).stem().string();
    addUniqueAlias(filename, asset.unsafePtr());
    addUniqueAlias(stem, asset.unsafePtr());
    auto name = asset->get_name();
    if (name) {
      auto nameKey = NormalizeAssetKey(il2cpp_utils::detail::to_string(name));
      addUniqueAlias(nameKey, asset.unsafePtr());
      if (auto* shader = il2cpp_utils::try_cast<UnityEngine::Shader>(asset.unsafePtr()).value_or(nullptr);
          IsAlive(shader) && shader->get_isSupported() && !nameKey.empty()) {
        _supportedShadersByName[nameKey] = shader;
      }
    }
    if (GetVivifyDebugLogging()) {
      if (auto* material = il2cpp_utils::try_cast<UnityEngine::Material>(asset.unsafePtr()).value_or(nullptr);
          IsAlive(material)) {
        LogMaterialShader("bundle-load", originalAssetPath, material);
      } else if (auto* shader = il2cpp_utils::try_cast<UnityEngine::Shader>(asset.unsafePtr()).value_or(nullptr);
                 IsAlive(shader)) {
        PaperLogger.info("Vivify shader asset: path='{}' shader='{}' supported={}",
                         originalAssetPath, ShaderNameForLog(shader), BoolText(shader->get_isSupported()));
      }
    }
  }
}

bool Runtime::PreloadBundle(std::string const& bundlePath) {
  if (_preloadedBundlePath == bundlePath && _mainBundle != nullptr &&
      UnityEngine::Object::op_Implicit_bool(_mainBundle)) {
    if (GetVivifyDebugLogging()) {
      PaperLogger.info("Vivify bundle already preloaded: '{}'", bundlePath);
    }
    return true;
  }
  if (_mainBundle != nullptr && UnityEngine::Object::op_Implicit_bool(_mainBundle)) {
    // PreloadBundle may replace a failed/non-standard candidate while the map
    // is still selected. No gameplay instance owns these preload-only assets,
    // so retaining them would recreate the same cross-map memory leak.
    _assets.clear();
    _assetsByName.clear();
    _ambiguousAssetAliases.clear();
    _supportedShadersByName.clear();
    _mainBundle->Unload(true);
    _mainBundle = nullptr;
  }
  _preloadedBundlePath = bundlePath;
  _mainBundle = UnityEngine::AssetBundle::LoadFromFile(StringW(bundlePath));
  if (_mainBundle == nullptr) {
    if (GetVivifyDebugLogging()) {
      PaperLogger.warn("Vivify bundle preload failed: '{}'", bundlePath);
    }
    _preloadedBundlePath.clear();
    return false;
  }
  if (GetVivifyDebugLogging()) {
    PaperLogger.info("Vivify bundle preloaded: '{}'", bundlePath);
  }
  CacheBundleAssets();
  return true;
}

void Runtime::LoadMainBundle() {
  LogUnityPlatformInfoOnce();
  if (_selectedLevelPath.empty()) {
    if (_selectedMapHasVivifyRequirement && GetVivifyDebugLogging()) {
      PaperLogger.warn("Vivify bundle load skipped: selected level path is empty");
    }
    return;
  }
  auto bundlePaths = ResolveBundlePaths(_selectedLevelPath);
  if (bundlePaths.empty()) {
    if (GetVivifyDebugLogging()) {
      PaperLogger.warn("Vivify bundle not found in '{}' (no *.vivify file)", _selectedLevelPath);
    }
    return;
  }
  if (!_preloadedBundlePath.empty() &&
      std::find(bundlePaths.begin(), bundlePaths.end(), _preloadedBundlePath) != bundlePaths.end() &&
      _mainBundle != nullptr && UnityEngine::Object::op_Implicit_bool(_mainBundle)) {
    if (GetVivifyDebugLogging()) {
      PaperLogger.info("Vivify bundle preloaded, rebuilding asset caches: '{}'", _preloadedBundlePath);
    }
    CacheBundleAssets();
    RepairLoadedMaterialShaders();
    return;
  }
  for (auto const& bundlePath : bundlePaths) {
    if (GetVivifyDebugLogging()) {
      PaperLogger.info("Vivify loading asset bundle: path='{}'", bundlePath);
    }
    _mainBundle = UnityEngine::AssetBundle::LoadFromFile(StringW(bundlePath));
    if (_mainBundle == nullptr) {
      if (GetVivifyDebugLogging()) {
        PaperLogger.warn("Vivify asset bundle load failed: path='{}'", bundlePath);
      }
      continue;
    }
    _preloadedBundlePath = bundlePath;
    CacheBundleAssets();
    RepairLoadedMaterialShaders();
    return;
  }
  PaperLogger.warn("Vivify could not load any bundle candidate for '{}'", _selectedLevelPath);
}

UnityEngine::Object* Runtime::GetAssetObject(std::string_view assetName) const {
  std::string const key = NormalizeAssetKey(assetName);
  auto it = _assets.find(key);
  if (it != _assets.end()) {
    return it->second;
  }
  auto findAlias = [this](std::string const& alias) -> UnityEngine::Object* {
    if (alias.empty() || _ambiguousAssetAliases.contains(alias)) return nullptr;
    auto found = _assetsByName.find(alias);
    return found != _assetsByName.end() ? found->second : nullptr;
  };
  if (auto* asset = findAlias(key); asset != nullptr) return asset;
  std::string const filename = std::filesystem::path(key).filename().string();
  if (auto* asset = findAlias(filename); asset != nullptr) return asset;
  std::string const stem = std::filesystem::path(filename).stem().string();
  if (auto* asset = findAlias(stem); asset != nullptr) return asset;
  if (GetVivifyDebugLogging() && _missingAssetKeys.emplace(key).second) {
    PaperLogger.warn("Vivify asset lookup miss (first occurrence): '{}'", std::string(assetName));
  }
  return nullptr;
}

void Runtime::LogUnityPlatformInfoOnce() {
  if (!GetVivifyDebugLogging() || _loggedUnityPlatformInfo) return;
  _loggedUnityPlatformInfo = true;
  auto stereoMode = UnityEngine::XR::XRSettings::get_stereoRenderingMode();
  auto graphicsType = UnityEngine::SystemInfo::get_graphicsDeviceType();
  PaperLogger.info(
      "Vivify platform: os='{}' device='{}' gpu='{}' vendor='{}' api={} stereoMode={} xrOcclusionMesh={} supportsInstancing={} supportsR8={} supportsDepthRT={}",
      ToStdString(UnityEngine::SystemInfo::get_operatingSystem()),
      ToStdString(UnityEngine::SystemInfo::get_deviceModel()),
      ToStdString(UnityEngine::SystemInfo::get_graphicsDeviceName()),
      ToStdString(UnityEngine::SystemInfo::get_graphicsDeviceVendor()),
      graphicsType.value__,
      stereoMode.value__,
      BoolText(UnityEngine::XR::XRSettings::get_useOcclusionMesh()),
      BoolText(UnityEngine::SystemInfo::get_supportsInstancing()),
      BoolText(UnityEngine::SystemInfo::SupportsRenderTextureFormat(UnityEngine::RenderTextureFormat::R8)),
      BoolText(UnityEngine::SystemInfo::SupportsRenderTextureFormat(UnityEngine::RenderTextureFormat::Depth)));
}

UnityEngine::RenderTextureFormat Runtime::SupportedRenderTextureFormat(UnityEngine::RenderTextureFormat requested,
                                                                       std::string_view context) const {
  // Keep an authored render-texture format whenever the device supports it.
  if (UnityEngine::SystemInfo::SupportsRenderTextureFormat(requested)) {
    return requested;
  }
  auto fallback = UnityEngine::RenderTextureFormat::ARGB32;
  if (GetVivifyDebugLogging()) {
    PaperLogger.warn("Vivify RT format unsupported: context={} requested={} fallback={}",
                     context, requested.value__, fallback.value__);
  }
  return fallback;
}

void Runtime::LogMaterialShader(std::string_view context, std::string_view assetPath, UnityEngine::Material* material) const {
  if (!GetVivifyDebugLogging()) return;
  if (!IsAlive(material)) {
    PaperLogger.warn("Vivify material missing: context={} asset={}", context, assetPath);
    return;
  }
  auto* shader = material->get_shader().unsafePtr();
  auto shaderName = ShaderNameForLog(shader);
  PaperLogger.info("Vivify material: context={} asset={} material='{}' shader='{}' supported={} internalError={}",
                   context,
                   assetPath,
                   ToStdString(material->get_name()),
                   shaderName,
                   BoolText(IsAlive(shader) && shader->get_isSupported()),
                   BoolText(IsInternalErrorShaderName(shaderName)));
}

UnityEngine::Shader* Runtime::FindUsableShader(std::string const& shaderName) const {
  if (shaderName.empty()) return nullptr;
  if (auto it = _supportedShadersByName.find(NormalizeAssetKey(shaderName));
      it != _supportedShadersByName.end() && IsAlive(it->second) && it->second->get_isSupported()) {
    return it->second;
  }
  auto* bundled = il2cpp_utils::try_cast<UnityEngine::Shader>(GetAssetObject(shaderName)).value_or(nullptr);
  if (IsAlive(bundled) && bundled->get_isSupported()) {
    return bundled;
  }
  auto found = UnityEngine::Shader::Find(StringW(shaderName));
  auto* foundShader = found.unsafePtr();
  if (IsAlive(foundShader) && foundShader->get_isSupported()) {
    return foundShader;
  }
  return nullptr;
}

UnityEngine::Shader* Runtime::FindFallbackShader(bool preferTexture) const {
  std::array<std::string_view, 4> const fallbackNames = preferTexture
      ? std::array<std::string_view, 4>{"Unlit/Texture"sv, "Sprites/Default"sv,
                                        "Unlit/Color"sv, "Standard"sv}
      : std::array<std::string_view, 4>{"Unlit/Color"sv, "Standard"sv,
                                        "Unlit/Texture"sv, "Sprites/Default"sv};
  for (auto name : fallbackNames) {
    auto shader = UnityEngine::Shader::Find(StringW(std::string(name)));
    auto* rawShader = shader.unsafePtr();
    if (IsAlive(rawShader) && rawShader->get_isSupported()) {
      return rawShader;
    }
  }
  return nullptr;
}

void Runtime::RepairMaterialShader(UnityEngine::Material* material, std::string_view context) {
  if (!IsAlive(material)) return;
  if (_repairedMaterials.contains(material)) return;
  auto shader = material->get_shader();
  auto* rawShader = shader.unsafePtr();
  auto originalShaderName = ShaderNameForLog(rawShader);
  int const originalPassCount = IsAlive(rawShader) ? material->get_passCount() : 0;
  if (GetVivifyDebugLogging() &&
      (!IsAlive(rawShader) || IsInternalErrorShaderName(originalShaderName) ||
       (IsAlive(rawShader) && !rawShader->get_isSupported()))) {
    PaperLogger.warn("Vivify shader diagnostic: context={} material='{}' shader='{}' supported={} internalError={} passes={}",
                     context,
                     ToStdString(material->get_name()),
                     originalShaderName,
                     BoolText(IsAlive(rawShader) && rawShader->get_isSupported()),
                     BoolText(IsInternalErrorShaderName(originalShaderName)),
                     originalPassCount);
  }
  if (IsAlive(rawShader) && !IsInternalErrorShaderName(originalShaderName) &&
      rawShader->get_isSupported()) {
    ApplyStereoKeywords(material);
    _repairedMaterials.emplace(material);
    return;
  }
  auto fallbackState = CaptureMaterialFallbackState(material);
  UnityEngine::Shader* replacement = nullptr;
  if (IsAlive(rawShader)) {
    auto shaderName = rawShader->get_name();
    if (shaderName) {
      replacement = FindUsableShader(ToStdString(shaderName));
    }
  }
  bool usedGenericFallback = false;
  if (!IsAlive(replacement)) {
    replacement = FindFallbackShader(IsManagedAlive(fallbackState.mainTexture));
    usedGenericFallback = IsAlive(replacement);
  }
  if (IsAlive(replacement)) {
    if (usedGenericFallback) {
      _genericFallbackMaterials.emplace(material);
    } else {
      _genericFallbackMaterials.erase(material);
    }
    material->set_shader(replacement);
    _blitMaterialValidCache.erase(material);
    _warnedInvalidBlitPasses.clear();
    RestoreMaterialFallbackState(material, fallbackState);
    ApplyStereoKeywords(material);
    if (GetVivifyDebugLogging()) {
      PaperLogger.info("Vivify shader repaired: context={} material='{}' from='{}' to='{}' preservedColor={} preservedTexture={}",
                       context, ToStdString(material->get_name()), originalShaderName, ShaderNameForLog(replacement),
                       BoolText(fallbackState.color.has_value()), BoolText(IsManagedAlive(fallbackState.mainTexture)));
    }
  } else if (GetVivifyDebugLogging()) {
    PaperLogger.warn("Vivify shader repair failed: context={} material='{}' original='{}'",
                     context, ToStdString(material->get_name()), originalShaderName);
  }
  _repairedMaterials.emplace(material);
}

void Runtime::RepairGameObjectMaterials(UnityEngine::GameObject* gameObject, std::string_view context) {
  if (!IsAlive(gameObject)) return;
  auto renderers = gameObject->GetComponentsInChildren<UnityEngine::Renderer*>(true);
  for (int i = 0; i < renderers.size(); i++) {
    auto* renderer = renderers[i];
    if (!IsAlive(renderer)) continue;
    auto materials = renderer->get_sharedMaterials();
    if (!materials) continue;
    for (int j = 0; j < materials.size(); j++) {
      RepairMaterialShader(materials[j].unsafePtr(), context);
    }
  }
}

void Runtime::SetMaterialKeyword(UnityEngine::Material* material, ::StringW keyword, bool enabled) const {
  if (!IsAlive(material)) return;
  if (enabled) {
    material->EnableKeyword(keyword);
  } else {
    material->DisableKeyword(keyword);
  }
}

void Runtime::ApplyStereoKeywords(UnityEngine::Material* material) const {
  if (!IsAlive(material)) return;

  // MULTIPASS_ENABLED is valid only for Unity's actual MultiPass mode.
  bool const enableMultipassKeyword = GetMultipassRenderingEnabled();
  SetMaterialKeyword(material, u"MULTIPASS_ENABLED", enableMultipassKeyword);

  // A Multiview shader already executes once for each gl_ViewID/array slice.
  // Enabling the legacy explicit-per-eye variant and issuing another pair of
  // draws creates a second eye axis (the Dynasty crack regression). Keep the
  // marker property as bundle metadata, but never turn it into a Quest keyword.
  SetMaterialKeyword(material, u"VIVIFY_PER_EYE_MULTIPASS", false);
}

void Runtime::ApplyGameObjectStereoKeywords(UnityEngine::GameObject* gameObject) {
  if (!IsAlive(gameObject)) return;
  auto renderers = gameObject->GetComponentsInChildren<UnityEngine::Renderer*>(true);
  for (int i = 0; i < renderers.size(); i++) {
    auto* renderer = renderers[i];
    if (!IsAlive(renderer)) continue;
    auto materials = renderer->get_sharedMaterials();
    if (!materials) continue;
    for (int j = 0; j < materials.size(); j++) {
      ApplyStereoKeywords(materials[j].unsafePtr());
    }
  }
}

void Runtime::RefreshLoadedMaterialStereoKeywords() {
  for (auto const& [_, asset] : _assets) {
    if (!IsAlive(asset)) continue;
    if (auto* material = il2cpp_utils::try_cast<UnityEngine::Material>(asset).value_or(nullptr); IsAlive(material)) {
      ApplyStereoKeywords(material);
    } else if (auto* gameObject = il2cpp_utils::try_cast<UnityEngine::GameObject>(asset).value_or(nullptr); IsAlive(gameObject)) {
      ApplyGameObjectStereoKeywords(gameObject);
    }
  }
}

void Runtime::RepairLoadedMaterialShaders() {
  for (auto const& [path, asset] : _assets) {
    if (!IsAlive(asset)) continue;
    if (auto* material = il2cpp_utils::try_cast<UnityEngine::Material>(asset).value_or(nullptr); IsAlive(material)) {
      RepairMaterialShader(material, path);
    } else if (auto* gameObject = il2cpp_utils::try_cast<UnityEngine::GameObject>(asset).value_or(nullptr); IsAlive(gameObject)) {
      RepairGameObjectMaterials(gameObject, path);
    }
  }
}

}
