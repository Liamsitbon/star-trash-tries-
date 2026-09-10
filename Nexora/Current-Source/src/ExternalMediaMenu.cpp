#include "ExternalMediaMenu.hpp"
#include "ExternalMedia.hpp"
#include "NexoraRuntime.hpp"
#include "main.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <thread>
#include <unordered_set>

#include "bsml/shared/BSML-Lite/Creation/Buttons.hpp"
#include "bsml/shared/BSML-Lite/Creation/Layout.hpp"
#include "bsml/shared/BSML-Lite/Creation/Settings.hpp"
#include "bsml/shared/BSML-Lite/Creation/Text.hpp"
#include "bsml/shared/BSML/MainThreadScheduler.hpp"
#include "bsml/shared/BSML/Settings/BSMLSettings.hpp"
#include "custom-types/shared/delegate.hpp"
#include "songcore/shared/SongCore.hpp"
#include "GlobalNamespace/BeatmapLevel.hpp"
#include "GlobalNamespace/BeatmapLevelsModel.hpp"
#include "GlobalNamespace/BeatmapLevelsRepository.hpp"
#include "GlobalNamespace/LevelSelectionFlowCoordinator.hpp"
#include "GlobalNamespace/MainFlowCoordinator.hpp"
#include "GlobalNamespace/SoloFreePlayFlowCoordinator.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/Time.hpp"
#include "HMUI/ViewController.hpp"

namespace Nexora {
namespace {
using Button = UnityEngine::UI::Button;
using Transform = UnityEngine::Transform;
enum class Page { Home, Files, Songs };
struct Song { std::string id, title, mapRoot; };
struct Menu {
  SafePtrUnity<HMUI::ViewController> view;
  SafePtrUnity<HMUI::CurvedTextMeshPro> status;
  SafePtrUnity<UnityEngine::GameObject> home, files, listing;
  std::array<SafePtrUnity<Button>, 8> rows;
  SafePtrUnity<Button> previous, next, copy;
  Page page = Page::Home;
  std::size_t first = 0;
  std::vector<std::size_t> filtered;
  MediaDirectory directory;
  std::vector<Song> songs;
  Song selected;
  std::filesystem::path video;
  std::string search, message;
  float offset = 0, yaw = 0;
  bool confirmCopy = false, navigating = false;
};
// One UI instance, weakly referenced by every callback. No worker touches a
// Unity object; replacing/recreating the view invalidates old completion work.
std::shared_ptr<Menu> gMenu;
struct FileJob {
  std::atomic_bool done = false, cancelled = false;
  std::thread worker;
  MediaDirectory directory;
  std::filesystem::path copied;
  std::string error;
  ~FileJob() { cancelled = true; if (worker.joinable()) worker.join(); }
};
std::shared_ptr<FileJob> gJob;

template<class T> bool Alive(T* object) { return object && UnityEngine::Object::op_Implicit_bool(object); }
std::string Plain(std::string value, std::size_t max = 200) {
  for (char& c : value) {
    if (c == '<') c = '(';
    if (c == '>') c = ')';
    if (static_cast<unsigned char>(c) < 32 || c == 127) c = ' ';
  }
  if (value.size() > max) {
    while (max > 0 && (static_cast<unsigned char>(value[max]) & 0xc0) == 0x80) --max;
    value.resize(max); value += "...";
  }
  return value;
}
std::string Lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::tolower(c); });
  return value;
}
void Render(std::shared_ptr<Menu> const& menu);
bool Busy() { return gJob != nullptr; }
void SetVisible(SafePtrUnity<UnityEngine::GameObject> const& object, bool visible) {
  if (object.isAlive()) object->SetActive(visible);
}
void Report(std::shared_ptr<Menu> const& menu, std::string message) {
  menu->message = std::move(message);
  Render(menu);
}
template<class F>
auto Guard(std::weak_ptr<Menu> weak, F action) {
  return [weak, action] {
    auto menu = weak.lock();
    if (!menu || !menu->view.isAlive()) return;
    try { action(menu); }
    catch (std::exception const& e) { PaperLogger.warn("Nexora menu: {}", e.what()); Report(menu, e.what()); }
    catch (...) { Report(menu, "Operation failed. See Nexora.log; no automatic import was attempted."); }
  };
}

// A single, owned background operation. A cancelled/error copy deletes only
// its own partial output. Directory browsing and multi-GB copies do not stall
// the game's render thread; Unity updates happen through BSML's scheduler.
void StartJob(std::shared_ptr<Menu> const& menu,
              std::function<void(FileJob&)> work,
              std::function<void(std::shared_ptr<Menu> const&, FileJob const&)> finish) {
  if (Busy()) { Report(menu, "A file operation is still running. Wait or cancel it first."); return; }
  auto job = std::make_shared<FileJob>();
  gJob = job;
  std::weak_ptr<Menu> weak = menu;
  try {
    BSML::MainThreadScheduler::ScheduleUntil(
      [job] { return job->done.load(std::memory_order_acquire); },
      [job, weak, finish = std::move(finish)] {
        if (job->worker.joinable()) job->worker.join();
        if (gJob == job) gJob.reset();
        auto current = weak.lock();
        if (!current || !current->view.isAlive()) return;
        try {
          if (!job->error.empty()) Report(current, job->error);
          else if (job->cancelled && job->copied.empty()) Report(current, "File operation cancelled.");
          else finish(current, *job);
        } catch (std::exception const& e) { Report(current, e.what()); }
        catch (...) { Report(current, "Could not update the menu after the operation."); }
      });
    // The owner/destructor joins this thread. Do not let the worker own the
    // last shared_ptr to itself (that could self-join during app shutdown).
    job->worker = std::thread([data = job.get(), work = std::move(work)] {
      try { work(*data); }
      catch (std::exception const& e) { data->error = e.what(); }
      catch (...) { data->error = "File operation failed"; }
      data->done.store(true, std::memory_order_release);
    });
  } catch (...) {
    job->cancelled = true;
    job->done.store(true, std::memory_order_release);
    gJob.reset();
    throw;
  }
  Render(menu);
}

GlobalNamespace::MainFlowCoordinator* MainFlow() {
  auto flows = UnityEngine::Resources::FindObjectsOfTypeAll<GlobalNamespace::MainFlowCoordinator*>();
  if (flows) for (auto* flow : flows) if (Alive(flow)) return flow;
  return nullptr;
}
GlobalNamespace::BeatmapLevelsModel* LevelsModel() {
  auto* flow = MainFlow();
  return flow ? flow->_beatmapLevelsModel : nullptr;
}
void Browse(std::shared_ptr<Menu> const& menu, std::filesystem::path path) {
  if (Busy()) { Report(menu, "Wait for the current file operation or cancel it first."); return; }
  menu->confirmCopy = false;
  menu->page = Page::Files;
  menu->directory.path = path;
  menu->directory.entries.clear();
  menu->message = "Reading the selected folder...";
  StartJob(menu, [path](FileJob& job) { job.directory = BrowseMedia(path, 4096, &job.cancelled); },
    [](auto const& current, FileJob const& job) {
      current->directory = job.directory;
      current->first = 0; current->page = Page::Files;
      Report(current, job.directory.truncated ? "First 4096 entries shown; choose a smaller folder." : "Choose a video; it stays in this folder.");
    });
}
void RefreshSongs(std::shared_ptr<Menu> const& menu) {
  if (Busy()) { Report(menu, "Wait for the current file operation or cancel it first."); return; }
  if (SongCore::API::Loading::AreSongsRefreshing()) {
    Report(menu, "SongCore is refreshing songs. Try again when it finishes."); return;
  }
  menu->songs.clear();
  std::unordered_set<std::string> ids;
  auto add = [&](GlobalNamespace::BeatmapLevel* level, std::string root = {}) {
    if (!level) return;
    auto id = std::string(level->levelID);
    if (!ids.insert(id).second) return;
    menu->songs.push_back({id, std::string(level->songName) + " - " + std::string(level->songAuthorName), std::move(root)});
  };
  // Capture plain metadata on the main thread; do not retain level pointers
  // across refreshes. Use the game's loaded catalogue, not an invented scan.
  for (auto* level : SongCore::API::Loading::GetAllLevels())
    if (level) add(level, std::string(level->customLevelPath));
  auto* model = LevelsModel();
  auto* repository = model ? model->_allLoadedBeatmapLevelsRepository : nullptr;
  if (repository && repository->_beatmapLevelPacks) {
    for (auto* pack : repository->_beatmapLevelPacks) {
      if (!pack) continue;
      if (pack->_allBeatmapLevels) {
        for (int i = 0; i < pack->_allBeatmapLevels->get_Count(); ++i) add(pack->_allBeatmapLevels->get_Item(i));
      } else if (pack->_beatmapLevels) for (auto* level : pack->_beatmapLevels) add(level);
    }
  }
  std::sort(menu->songs.begin(), menu->songs.end(), [](auto const& a, auto const& b) { return a.title < b.title; });
  menu->page = Page::Songs; menu->first = 0; menu->confirmCopy = false;
  Report(menu, "Choose a song. Difficulty and Play stay in Beat Saber's normal menu.");
}

void OpenSelectedSong(std::shared_ptr<Menu> const& menu) {
  if (menu->navigating) throw std::runtime_error("The selected song is already opening");
  if (menu->selected.id.empty() || menu->video.empty()) throw std::runtime_error("Choose both a video and a song first");
  if (Busy()) throw std::runtime_error("Wait for the file operation to finish first");
  auto* model = LevelsModel();
  if (!model || !model->GetBeatmapLevel(StringW(menu->selected.id)))
    throw std::runtime_error("Song is no longer available. Refresh the song list");
  auto* settings = BSML::BSMLSettings::get_instance()->get_modSettingsFlowCoordinator();
  if (!Alive(settings) || settings->isPresenting || settings->isAnimating || settings->get_isInTransition())
    throw std::runtime_error("Menu is still transitioning; please try again");
  Runtime::Instance().BindExternalVideo({menu->selected.id, menu->video, menu->offset, menu->yaw});
  // Close BSML through its own cancel path (no applying/restarting other mods),
  // then ask the normal Solo flow to select the song. Never synthesize a Start
  // call or bypass native ownership, requirements, difficulty or play gates.
  SafePtrUnity<BSML::ModSettingsFlowCoordinator> settingsRef(settings);
  float const deadline = UnityEngine::Time::get_realtimeSinceStartup() + 5;
  auto levelId = menu->selected.id;
  settings->Cancel();
  BSML::MainThreadScheduler::ScheduleUntil(
    [settingsRef, deadline] {
      return !settingsRef.isAlive() || (!settingsRef->get_isActivated() && !settingsRef->get_isInTransition()) ||
             UnityEngine::Time::get_realtimeSinceStartup() > deadline;
    }, Guard(std::weak_ptr<Menu>(menu), [settingsRef, levelId](auto const& current) {
      current->navigating = false;
      auto const* bound = Runtime::Instance().GetExternalVideo();
      if (!bound || bound->levelId != levelId) return; // user cleared/replaced it during transition
      if (settingsRef.isAlive() && (settingsRef->get_isActivated() || settingsRef->get_isInTransition()))
        throw std::runtime_error("Menu transition timed out. Selection is armed; open this song normally in Solo.");
      auto* main = MainFlow();
      auto* model = main ? main->_beatmapLevelsModel : nullptr;
      auto* solo = main ? main->_soloFreePlayFlowCoordinator.unsafePtr() : nullptr;
      auto* level = model ? model->GetBeatmapLevel(StringW(levelId)) : nullptr;
      if (!Alive(main) || !Alive(solo) || !level || main->get_isInTransition() ||
          main->YoungestChildFlowCoordinatorOrSelf().unsafePtr() != main)
        throw std::runtime_error("Selection is armed. Open this song normally in Solo when the current menu closes.");
      auto* pack = model->GetLevelPackForLevelId(StringW(levelId));
      solo->Setup(GlobalNamespace::LevelSelectionFlowCoordinator_State::New_ctor(pack, level));
      main->PresentFlowCoordinatorOrAskForTutorial(solo);
      current->message = "Custom armed for this song only. Choose difficulty, then Play. Nothing was copied.";
    }));
  menu->navigating = true;
}

void Render(std::shared_ptr<Menu> const& menu) {
  if (!menu->view.isAlive() || !menu->status.isAlive()) return;
  bool const home = menu->page == Page::Home;
  SetVisible(menu->home, home);
  SetVisible(menu->files, menu->page == Page::Files);
  SetVisible(menu->listing, !home);
  std::string text = menu->message;
  if (home) {
    auto const* active = Runtime::Instance().GetExternalVideo();
    text += "\nVideo: " + (menu->video.empty() ? std::string("none selected") : Plain(menu->video.string(), 180));
    text += "\nSong: " + (menu->selected.id.empty() ? std::string("none selected") : Plain(menu->selected.title, 110));
    text += active ? "\nCustom binding active for one song; all other songs use their normal map." : "\nNormal map mode; no external override active.";
    if (menu->copy.isAlive()) BSML::Lite::SetButtonText(menu->copy.ptr(), menu->confirmCopy
      ? u"Confirm COPY into selected map (source stays)" : u"Copy to map... (optional)");
  } else {
    menu->filtered.clear();
    auto search = Lower(menu->search);
    std::size_t count = menu->page == Page::Files ? menu->directory.entries.size() : menu->songs.size();
    for (std::size_t i = 0; i < count; ++i) {
      auto name = menu->page == Page::Files ? menu->directory.entries[i].path.filename().string() : menu->songs[i].title;
      if (search.empty() || Lower(name).find(search) != std::string::npos) menu->filtered.push_back(i);
    }
    if (menu->first >= menu->filtered.size()) menu->first = 0;
    text += "\n" + (menu->page == Page::Files ? Plain(menu->directory.path.string(), 150) : std::string("Installed/loaded song catalogue"));
    text += "\n" + std::to_string(menu->filtered.size()) + " results; page " + std::to_string(menu->first / menu->rows.size() + 1);
    for (std::size_t row = 0; row < menu->rows.size(); ++row) {
      if (!menu->rows[row].isAlive()) continue;
      auto* button = menu->rows[row].ptr();
      bool exists = menu->first + row < menu->filtered.size();
      button->get_gameObject()->SetActive(exists);
      if (!exists) continue;
      auto index = menu->filtered[menu->first + row];
      auto label = menu->page == Page::Files
        ? (menu->directory.entries[index].directory ? "[Folder] " : "[Video] ") + menu->directory.entries[index].path.filename().string()
        : menu->songs[index].title;
      BSML::Lite::SetButtonText(button, StringW(Plain(label, 85)));
      button->set_interactable(!Busy());
    }
    if (menu->previous.isAlive()) menu->previous->set_interactable(menu->first > 0 && !Busy());
    if (menu->next.isAlive()) menu->next->set_interactable(menu->first + menu->rows.size() < menu->filtered.size() && !Busy());
  }
  if (Busy()) text += "\nFile operation running in background...";
  menu->status->set_text(StringW(text));
}

void Create(HMUI::ViewController* view, bool first, bool, bool) {
  if (!Alive(view)) return;
  if (!first && gMenu && gMenu->view.isAlive() && gMenu->view.ptr() == view) { Render(gMenu); return; }
  auto menu = std::make_shared<Menu>();
  menu->view = view;
  menu->message = "Normal map videos stay supported. Custom plays in place; Copy is a separate explicit action.";
  gMenu = menu;
  std::weak_ptr<Menu> weak = menu;
  auto* container = BSML::Lite::CreateScrollableSettingsContainer(view->get_transform());
  auto parent = container->get_transform();
  menu->status = BSML::Lite::CreateText(parent, u"Nexora", TMPro::FontStyles::Normal, 3.1f);
  menu->status->set_richText(false);
  auto* home = BSML::Lite::CreateVerticalLayoutGroup(parent);
  menu->home = home->get_gameObject().unsafePtr();
  auto hp = home->get_transform();
  BSML::Lite::CreateUIButton(hp, u"Choose external video (no import)", Guard(weak, [](auto const& m) { Browse(m, "/sdcard/Movies"); }));
  BSML::Lite::CreateUIButton(hp, u"Choose installed song", Guard(weak, RefreshSongs));
  BSML::Lite::CreateIncrementSetting(hp, u"Video time offset (seconds)", 1, .1f, 0, true, true, -3600, 3600,
    [weak](float value) { if (auto m = weak.lock()) { m->offset = value; m->confirmCopy = false; } });
  BSML::Lite::CreateIncrementSetting(hp, u"Extra rotation (degrees)", 0, 15, 0, true, true, -360, 360,
    [weak](float value) { if (auto m = weak.lock()) { m->yaw = value; m->confirmCopy = false; } });
  BSML::Lite::CreateUIButton(hp, u"Custom: use in place + open song", Guard(weak, OpenSelectedSong));
  BSML::Lite::CreateUIButton(hp, u"Normal map mode (clear Custom)", Guard(weak, [](auto const& m) {
    Runtime::Instance().ClearExternalVideo(); m->confirmCopy = false;
    Report(m, "Custom cleared. Original map DAT/media behavior restored; no files changed.");
  }));
  menu->copy = BSML::Lite::CreateUIButton(hp, u"Copy to map... (optional)", Guard(weak, [](auto const& m) {
    if (m->selected.mapRoot.empty() || m->video.empty()) throw std::runtime_error("Choose a video and a custom map first. OST/DLC folders cannot receive a copy.");
    if (Busy()) throw std::runtime_error("A file operation is still running");
    auto* installed = SongCore::API::Loading::GetLevelByLevelID(m->selected.id);
    if (!installed || std::string(installed->customLevelPath) != m->selected.mapRoot)
      throw std::runtime_error("Selected song changed or was removed. Refresh the song list before copying.");
    if (!m->confirmCopy) {
      m->confirmCopy = true;
      Report(m, "COPY confirmation: create Nexora-Custom-" + Plain(m->video.filename().string()) +
                " inside " + Plain(m->selected.mapRoot) + ". Original stays. Existing files will NOT be replaced. Press Confirm COPY to continue.");
      return;
    }
    m->confirmCopy = false;
    auto source = m->video; auto destination = m->selected.mapRoot;
    StartJob(m, [source, destination](FileJob& job) { job.copied = CopyVideoToMap(source, destination, job.cancelled); },
      [](auto const& current, FileJob const& job) {
        current->video = job.copied;
        Report(current, "Copy complete; source retained. Copied video selected. Press Custom to use it. No DAT or Info.dat was changed.");
      });
  }));
  auto* files = BSML::Lite::CreateHorizontalLayoutGroup(parent);
  menu->files = files->get_gameObject().unsafePtr();
  BSML::Lite::CreateUIButton(files->get_transform(), u"Movies", Guard(weak, [](auto const& m) { Browse(m, "/sdcard/Movies"); }));
  BSML::Lite::CreateUIButton(files->get_transform(), u"Download", Guard(weak, [](auto const& m) { Browse(m, "/sdcard/Download"); }));
  BSML::Lite::CreateUIButton(files->get_transform(), u"Storage", Guard(weak, [](auto const& m) { Browse(m, "/sdcard"); }));
  BSML::Lite::CreateUIButton(files->get_transform(), u"Up", Guard(weak, [](auto const& m) { Browse(m, m->directory.path.parent_path()); }));
  auto* listing = BSML::Lite::CreateVerticalLayoutGroup(parent);
  menu->listing = listing->get_gameObject().unsafePtr();
  auto lp = listing->get_transform();
  BSML::Lite::CreateStringSetting(lp, u"Filter names", u"", [weak](StringW text) {
    if (auto m = weak.lock()) { m->search = std::string(text).substr(0, 256); m->first = 0; Render(m); }
  });
  for (std::size_t row = 0; row < menu->rows.size(); ++row) {
    menu->rows[row] = BSML::Lite::CreateUIButton(lp, u"...", Guard(weak, [row](auto const& m) {
      if (Busy() || m->first + row >= m->filtered.size()) return;
      auto index = m->filtered[m->first + row];
      if (m->page == Page::Files) {
        auto entry = m->directory.entries.at(index);
        if (entry.directory) { Browse(m, entry.path); return; }
        m->video = ReadableExternalVideo(entry.path);
      } else if (m->page == Page::Songs) m->selected = m->songs.at(index);
      m->page = Page::Home; m->confirmCopy = false;
      Report(m, "Selection updated. Nothing copied, moved, or imported. Press Custom when ready.");
    }));
  }
  auto* pagination = BSML::Lite::CreateHorizontalLayoutGroup(lp);
  menu->previous = BSML::Lite::CreateUIButton(pagination->get_transform(), u"Previous", Guard(weak, [](auto const& m) {
    m->first = m->first >= m->rows.size() ? m->first - m->rows.size() : 0; Render(m);
  }));
  menu->next = BSML::Lite::CreateUIButton(pagination->get_transform(), u"Next", Guard(weak, [](auto const& m) {
    if (m->first + m->rows.size() < m->filtered.size()) m->first += m->rows.size(); Render(m);
  }));
  BSML::Lite::CreateUIButton(parent, u"Back to selection / cancel COPY confirmation", Guard(weak, [](auto const& m) {
    m->page = Page::Home; m->confirmCopy = false; Render(m);
  }));
  BSML::Lite::CreateUIButton(parent, u"Cancel file operation", Guard(weak, [](auto const& m) {
    if (gJob) gJob->cancelled = true;
    m->confirmCopy = false; Report(m, "Cancellation requested. Only a partial copy, if any, is removed; the source is untouched.");
  }));
  Render(menu);
}
}  // namespace

void RegisterExternalMediaMenu() {
  bool const registered = BSML::BSMLSettings::get_instance()->TryAddSettingsMenu(
    [](HMUI::ViewController* view, bool first, bool added, bool enabling) {
      try { Create(view, first, added, enabling); }
      catch (std::exception const& e) { PaperLogger.error("Nexora menu creation failed: {}", e.what()); }
      catch (...) { PaperLogger.error("Nexora menu creation failed"); }
    }, "Nexora", false);
  PaperLogger.info("Nexora external-media menu registration={}", registered);
}
}  // namespace Nexora
