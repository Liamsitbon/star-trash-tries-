#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "UnityEngine/Texture2D.hpp"
#include "beatsaber-hook/shared/utils/typedefs-wrappers.hpp"

namespace QuestNativeVideo {

// Quest-native local video playback. Android MediaPlayer decodes into a
// SurfaceTexture; a render-thread callback copies the external image into a
// normal RGBA texture that existing Beat Saber materials can sample safely.
// No UnityEngine.VideoPlayer, PC codec binary, or network downloader is used.
enum class State : std::uint8_t {
  WaitingForSurface,
  Idle,
  Preparing,
  Ready,
  Playing,
  Paused,
  Failed,
};

class Player final {
 public:
  struct Impl;

  static std::shared_ptr<Player> Create(int outputWidth, int outputHeight,
                                        std::string label);

  Player(Player const&) = delete;
  Player& operator=(Player const&) = delete;

  // Call once from the Unity main thread every frame while the player may be
  // preparing or visible. It only queues render-thread work and never decodes
  // video on the game thread.
  void Tick();

  void Open(std::string path, bool looping);
  void Play();
  void Pause();
  void Stop();
  void Seek(double seconds);
  void SetPlaybackSpeed(float speed);
  void SetLooping(bool looping);

  [[nodiscard]] State GetState() const;
  [[nodiscard]] bool IsPrepared() const;
  [[nodiscard]] bool IsPlaying() const;
  [[nodiscard]] bool HasFrame() const;
  [[nodiscard]] bool Failed() const;
  [[nodiscard]] double TimeSeconds() const;
  [[nodiscard]] double DurationSeconds() const;
  [[nodiscard]] float PlaybackSpeed() const;
  [[nodiscard]] int VideoWidth() const;
  [[nodiscard]] int VideoHeight() const;
  [[nodiscard]] std::uint64_t FrameSerial() const;
  [[nodiscard]] std::string Error() const;

  // The returned Texture2D is process-lifetime and intentionally independent
  // from gameplay scenes. MediaPlayer instances are reset between maps while
  // the bounded decoder surface is reused, avoiding render-thread teardown
  // races during Unity scene destruction.
  UnityEngine::Texture2D* Texture();

 private:
  explicit Player(std::shared_ptr<Impl> implementation);

  std::shared_ptr<Impl> _impl;
  SafePtrUnity<UnityEngine::Texture2D> _texture;
};

}  // namespace QuestNativeVideo
