#include "QuestNativeVideo/Player.hpp"

#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#include <jni.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "System/IntPtr.hpp"
#include "UnityEngine/AndroidJNI.hpp"
#include "UnityEngine/FilterMode.hpp"
#include "UnityEngine/GL.hpp"
#include "UnityEngine/TextureFormat.hpp"
#include "UnityEngine/TextureWrapMode.hpp"

namespace QuestNativeVideo {
namespace {

using namespace std::chrono_literals;

struct GlState {
  GLint drawFramebuffer = 0;
  GLint readFramebuffer = 0;
  GLint program = 0;
  GLint viewport[4] = {};
  GLint activeTexture = 0;
  GLint texture2D = 0;
  GLint textureExternal = 0;
  GLint vertexArray = 0;
  GLboolean blend = GL_FALSE;
  GLboolean depth = GL_FALSE;
  GLboolean cull = GL_FALSE;
  GLboolean scissor = GL_FALSE;
  GLboolean colorMask[4] = {};
  GLfloat clearColor[4] = {};
};

void CaptureGlState(GlState& state) {
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &state.drawFramebuffer);
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &state.readFramebuffer);
  glGetIntegerv(GL_CURRENT_PROGRAM, &state.program);
  glGetIntegerv(GL_VIEWPORT, state.viewport);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &state.activeTexture);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &state.vertexArray);
  state.blend = glIsEnabled(GL_BLEND);
  state.depth = glIsEnabled(GL_DEPTH_TEST);
  state.cull = glIsEnabled(GL_CULL_FACE);
  state.scissor = glIsEnabled(GL_SCISSOR_TEST);
  glGetBooleanv(GL_COLOR_WRITEMASK, state.colorMask);
  glGetFloatv(GL_COLOR_CLEAR_VALUE, state.clearColor);

  glActiveTexture(GL_TEXTURE0);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &state.texture2D);
  glGetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES, &state.textureExternal);
}

void RestoreToggle(GLenum capability, GLboolean enabled) {
  if (enabled) {
    glEnable(capability);
  } else {
    glDisable(capability);
  }
}

void RestoreGlState(GlState const& state) {
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                    static_cast<GLuint>(state.drawFramebuffer));
  glBindFramebuffer(GL_READ_FRAMEBUFFER,
                    static_cast<GLuint>(state.readFramebuffer));
  glUseProgram(static_cast<GLuint>(state.program));
  glBindVertexArray(static_cast<GLuint>(state.vertexArray));
  glViewport(state.viewport[0], state.viewport[1], state.viewport[2],
             state.viewport[3]);
  glColorMask(state.colorMask[0], state.colorMask[1], state.colorMask[2],
              state.colorMask[3]);
  glClearColor(state.clearColor[0], state.clearColor[1], state.clearColor[2],
               state.clearColor[3]);
  RestoreToggle(GL_BLEND, state.blend);
  RestoreToggle(GL_DEPTH_TEST, state.depth);
  RestoreToggle(GL_CULL_FACE, state.cull);
  RestoreToggle(GL_SCISSOR_TEST, state.scissor);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(state.texture2D));
  glBindTexture(GL_TEXTURE_EXTERNAL_OES,
                static_cast<GLuint>(state.textureExternal));
  glActiveTexture(static_cast<GLenum>(state.activeTexture));
}

GLuint CompileShader(GLenum type, char const* source) {
  GLuint shader = glCreateShader(type);
  if (shader == 0) return 0;
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  GLint compiled = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (compiled == GL_TRUE) return shader;
  glDeleteShader(shader);
  return 0;
}

GLuint CreateProgram() {
  constexpr char kVertex[] = R"(
#version 300 es
precision highp float;
uniform mat4 uSurfaceTransform;
out vec2 vUv;
void main() {
  const vec2 positions[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
  const vec2 texcoords[3] = vec2[3](vec2(0.0, 0.0), vec2(2.0, 0.0), vec2(0.0, 2.0));
  gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
  vUv = (uSurfaceTransform * vec4(texcoords[gl_VertexID], 0.0, 1.0)).xy;
}
)";
  constexpr char kFragment[] = R"(
#version 300 es
#extension GL_OES_EGL_image_external_essl3 : require
precision mediump float;
uniform samplerExternalOES uVideo;
in vec2 vUv;
layout(location = 0) out vec4 outColor;
void main() {
  outColor = texture(uVideo, vUv);
}
)";

  GLuint vertex = CompileShader(GL_VERTEX_SHADER, kVertex);
  GLuint fragment = CompileShader(GL_FRAGMENT_SHADER, kFragment);
  if (vertex == 0 || fragment == 0) {
    if (vertex != 0) glDeleteShader(vertex);
    if (fragment != 0) glDeleteShader(fragment);
    return 0;
  }
  GLuint program = glCreateProgram();
  glAttachShader(program, vertex);
  glAttachShader(program, fragment);
  glLinkProgram(program);
  glDeleteShader(vertex);
  glDeleteShader(fragment);
  GLint linked = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  if (linked == GL_TRUE) return program;
  glDeleteProgram(program);
  return 0;
}

JNIEnv* GetEnv(JavaVM* vm) {
  if (vm == nullptr) return nullptr;
  JNIEnv* environment = nullptr;
  jint const result =
      vm->GetEnv(reinterpret_cast<void**>(&environment), JNI_VERSION_1_6);
  if (result == JNI_OK) return environment;
  if (result != JNI_EDETACHED ||
      vm->AttachCurrentThread(&environment, nullptr) != JNI_OK) {
    return nullptr;
  }
  return environment;
}

jstring NewJavaString(JNIEnv* env, std::string const& utf8) {
  if (env == nullptr) return nullptr;
  std::vector<jchar> utf16;
  utf16.reserve(utf8.size());
  for (std::size_t index = 0; index < utf8.size();) {
    auto const lead = static_cast<unsigned char>(utf8[index]);
    std::uint32_t codepoint = 0xfffd;
    std::size_t length = 1;
    if (lead < 0x80) {
      codepoint = lead;
    } else if ((lead & 0xe0) == 0xc0 && index + 1 < utf8.size()) {
      auto const b1 = static_cast<unsigned char>(utf8[index + 1]);
      if ((b1 & 0xc0) == 0x80) {
        std::uint32_t const candidate =
            (static_cast<std::uint32_t>(lead & 0x1f) << 6) | (b1 & 0x3f);
        if (candidate >= 0x80) {
          codepoint = candidate;
          length = 2;
        }
      }
    } else if ((lead & 0xf0) == 0xe0 && index + 2 < utf8.size()) {
      auto const b1 = static_cast<unsigned char>(utf8[index + 1]);
      auto const b2 = static_cast<unsigned char>(utf8[index + 2]);
      if ((b1 & 0xc0) == 0x80 && (b2 & 0xc0) == 0x80) {
        std::uint32_t const candidate =
            (static_cast<std::uint32_t>(lead & 0x0f) << 12) |
            (static_cast<std::uint32_t>(b1 & 0x3f) << 6) | (b2 & 0x3f);
        if (candidate >= 0x800 &&
            !(candidate >= 0xd800 && candidate <= 0xdfff)) {
          codepoint = candidate;
          length = 3;
        }
      }
    } else if ((lead & 0xf8) == 0xf0 && index + 3 < utf8.size()) {
      auto const b1 = static_cast<unsigned char>(utf8[index + 1]);
      auto const b2 = static_cast<unsigned char>(utf8[index + 2]);
      auto const b3 = static_cast<unsigned char>(utf8[index + 3]);
      if ((b1 & 0xc0) == 0x80 && (b2 & 0xc0) == 0x80 &&
          (b3 & 0xc0) == 0x80) {
        std::uint32_t const candidate =
            (static_cast<std::uint32_t>(lead & 0x07) << 18) |
            (static_cast<std::uint32_t>(b1 & 0x3f) << 12) |
            (static_cast<std::uint32_t>(b2 & 0x3f) << 6) | (b3 & 0x3f);
        if (candidate >= 0x10000 && candidate <= 0x10ffff) {
          codepoint = candidate;
          length = 4;
        }
      }
    }
    index += length;
    if (codepoint <= 0xffff) {
      utf16.emplace_back(static_cast<jchar>(codepoint));
    } else {
      codepoint -= 0x10000;
      utf16.emplace_back(static_cast<jchar>(0xd800 + (codepoint >> 10)));
      utf16.emplace_back(static_cast<jchar>(0xdc00 + (codepoint & 0x3ff)));
    }
  }
  return env->NewString(utf16.data(), static_cast<jsize>(utf16.size()));
}

std::mutex gRegistryMutex;
std::unordered_map<int, std::weak_ptr<Player::Impl>> gRegistry;
std::atomic<int> gNextEventId{0x5100};

}  // namespace

struct Player::Impl : std::enable_shared_from_this<Player::Impl> {
  enum class CommandType { Open, Play, Pause, Stop, Seek, Speed, Loop };

  struct Command {
    CommandType type;
    std::string path;
    bool looping = false;
    double value = 0.0;
  };

  Impl(int width, int height, std::string playerLabel)
      : outputWidth(std::clamp(width, 16, 4096)),
        outputHeight(std::clamp(height, 16, 4096)),
        label(std::move(playerLabel)),
        eventId(gNextEventId.fetch_add(1)) {}

  void Start() {
    auto self = shared_from_this();
    std::thread([self = std::move(self)] { self->WorkerMain(); }).detach();
  }

  void SetError(std::string message, bool permanent = false) {
    if (permanent) backendFatal.store(true);
    {
      std::lock_guard lock(errorMutex);
      error = std::move(message);
    }
    state.store(State::Failed);
    commandCv.notify_all();
  }

  bool ClearJavaException(JNIEnv* env, std::string const& operation,
                          bool recordError, bool permanent = false) {
    if (env == nullptr || !env->ExceptionCheck()) return false;
    env->ExceptionDescribe();
    env->ExceptionClear();
    if (recordError) {
      SetError(label + ": Android " + operation + " failed", permanent);
    }
    return true;
  }

  void Enqueue(Command command) {
    {
      std::lock_guard lock(commandMutex);
      commands.emplace_back(std::move(command));
    }
    commandCv.notify_all();
  }

  void ResetFrame() {
    hasFrame.store(false);
    frameResetRequested.store(true);
  }

  void InitializeRenderSurface(JNIEnv* env) {
    if (renderInitialized.load() || state.load() == State::Failed) return;
    if (env == nullptr) {
      SetError(label + ": Java VM unavailable on render thread", true);
      return;
    }

    auto const* version = glGetString(GL_VERSION);
    if (version == nullptr) {
      SetError(label + ": OpenGL ES render context unavailable", true);
      return;
    }

    GlState saved{};
    CaptureGlState(saved);
    while (glGetError() != GL_NO_ERROR) {
    }

    glGenTextures(1, &externalTexture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, externalTexture);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &outputTexture);
    glBindTexture(GL_TEXTURE_2D, outputTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, outputWidth, outputHeight, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           outputTexture, 0);
    bool const framebufferComplete =
        glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    program = CreateProgram();
    glGenVertexArrays(1, &vertexArray);
    transformLocation =
        program != 0 ? glGetUniformLocation(program, "uSurfaceTransform") : -1;
    samplerLocation = program != 0 ? glGetUniformLocation(program, "uVideo") : -1;

    GLenum const initializationError = glGetError();
    RestoreGlState(saved);
    if (!framebufferComplete || program == 0 || vertexArray == 0 ||
        transformLocation < 0 || samplerLocation < 0 ||
        initializationError != GL_NO_ERROR) {
      SetError(label + ": native video GL surface initialization failed", true);
      return;
    }

    jclass surfaceTextureClass =
        env->FindClass("android/graphics/SurfaceTexture");
    if (ClearJavaException(env, "SurfaceTexture class lookup", true, true) ||
        surfaceTextureClass == nullptr) {
      return;
    }
    jmethodID constructor =
        env->GetMethodID(surfaceTextureClass, "<init>", "(I)V");
    updateTextureMethod =
        env->GetMethodID(surfaceTextureClass, "updateTexImage", "()V");
    transformMethod =
        env->GetMethodID(surfaceTextureClass, "getTransformMatrix", "([F)V");
    timestampMethod =
        env->GetMethodID(surfaceTextureClass, "getTimestamp", "()J");
    defaultBufferMethod = env->GetMethodID(surfaceTextureClass,
                                           "setDefaultBufferSize", "(II)V");
    jobject localSurfaceTexture =
        constructor != nullptr
            ? env->NewObject(surfaceTextureClass, constructor,
                             static_cast<jint>(externalTexture))
            : nullptr;
    if (ClearJavaException(env, "SurfaceTexture construction", true, true) ||
        localSurfaceTexture == nullptr || updateTextureMethod == nullptr ||
        transformMethod == nullptr || timestampMethod == nullptr ||
        defaultBufferMethod == nullptr) {
      env->DeleteLocalRef(surfaceTextureClass);
      return;
    }
    surfaceTexture = env->NewGlobalRef(localSurfaceTexture);
    env->DeleteLocalRef(localSurfaceTexture);
    env->DeleteLocalRef(surfaceTextureClass);

    jclass surfaceClass = env->FindClass("android/view/Surface");
    jmethodID surfaceConstructor =
        surfaceClass != nullptr
            ? env->GetMethodID(surfaceClass, "<init>",
                               "(Landroid/graphics/SurfaceTexture;)V")
            : nullptr;
    jobject localSurface =
        surfaceConstructor != nullptr
            ? env->NewObject(surfaceClass, surfaceConstructor, surfaceTexture)
            : nullptr;
    if (ClearJavaException(env, "Surface construction", true, true) ||
        localSurface == nullptr) {
      if (surfaceClass != nullptr) env->DeleteLocalRef(surfaceClass);
      return;
    }
    surface = env->NewGlobalRef(localSurface);
    env->DeleteLocalRef(localSurface);
    env->DeleteLocalRef(surfaceClass);

    jfloatArray localTransform = env->NewFloatArray(16);
    if (localTransform == nullptr) {
      SetError(label + ": SurfaceTexture transform allocation failed", true);
      return;
    }
    transformArray =
        static_cast<jfloatArray>(env->NewGlobalRef(localTransform));
    env->DeleteLocalRef(localTransform);

    {
      std::lock_guard lock(commandMutex);
      surfaceReady = true;
    }
    outputTextureId.store(outputTexture);
    renderInitialized.store(true);
    if (state.load() == State::WaitingForSurface) state.store(State::Idle);
    commandCv.notify_all();
  }

  void ClearOutput() {
    GlState saved{};
    CaptureGlState(saved);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, outputWidth, outputHeight);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    RestoreGlState(saved);
  }

  void Render(JNIEnv* env) {
    if (!renderInitialized.load()) {
      InitializeRenderSurface(env);
      return;
    }
    if (env == nullptr || surfaceTexture == nullptr || state.load() == State::Failed) {
      return;
    }

    if (frameResetRequested.exchange(false)) {
      lastTimestamp = 0;
      ClearOutput();
    }

    env->CallVoidMethod(surfaceTexture, updateTextureMethod);
    if (ClearJavaException(env, "SurfaceTexture update", true, true)) return;
    jlong const timestamp =
        env->CallLongMethod(surfaceTexture, timestampMethod);
    if (ClearJavaException(env, "SurfaceTexture timestamp", true, true) ||
        timestamp <= 0 || timestamp == lastTimestamp) {
      return;
    }
    lastTimestamp = timestamp;

    env->CallVoidMethod(surfaceTexture, transformMethod, transformArray);
    if (ClearJavaException(env, "SurfaceTexture transform", true, true)) return;
    std::array<jfloat, 16> matrix{};
    env->GetFloatArrayRegion(transformArray, 0, 16, matrix.data());
    if (ClearJavaException(env, "SurfaceTexture transform read", true, true)) return;

    GlState saved{};
    CaptureGlState(saved);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, outputWidth, outputHeight);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(program);
    glBindVertexArray(vertexArray);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, externalTexture);
    glUniform1i(samplerLocation, 0);
    glUniformMatrix4fv(transformLocation, 1, GL_FALSE, matrix.data());
    glDrawArrays(GL_TRIANGLES, 0, 3);
    GLenum const result = glGetError();
    RestoreGlState(saved);

    if (result != GL_NO_ERROR) {
      SetError(label + ": native video frame copy failed", true);
      return;
    }
    hasFrame.store(true);
    frameSerial.fetch_add(1);
  }

  void ReleaseMediaPlayer(JNIEnv* env, jobject& mediaPlayer,
                          jmethodID releaseMethod) {
    if (mediaPlayer == nullptr) return;
    env->CallVoidMethod(mediaPlayer, releaseMethod);
    ClearJavaException(env, "MediaPlayer release", false);
    env->DeleteGlobalRef(mediaPlayer);
    mediaPlayer = nullptr;
  }

  void WorkerMain() {
    JavaVM* workerVm = nullptr;
    JNIEnv* env = nullptr;
    auto const vmDeadline = std::chrono::steady_clock::now() + 10s;
    while (env == nullptr) {
      workerVm = vm.load();
      env = GetEnv(workerVm);
      if (env == nullptr) {
        if (std::chrono::steady_clock::now() >= vmDeadline) {
          SetError(label + ": Android Java VM attachment timed out", true);
          return;
        }
        std::this_thread::sleep_for(10ms);
      }
    }

    jclass mediaClass = env->FindClass("android/media/MediaPlayer");
    if (ClearJavaException(env, "MediaPlayer class lookup", true, true) ||
        mediaClass == nullptr) {
      return;
    }
    jmethodID constructor = env->GetMethodID(mediaClass, "<init>", "()V");
    jmethodID setDataSource = env->GetMethodID(
        mediaClass, "setDataSource", "(Ljava/lang/String;)V");
    jmethodID setSurface = env->GetMethodID(
        mediaClass, "setSurface", "(Landroid/view/Surface;)V");
    jmethodID setVolume = env->GetMethodID(mediaClass, "setVolume", "(FF)V");
    jmethodID setLooping = env->GetMethodID(mediaClass, "setLooping", "(Z)V");
    jmethodID prepare = env->GetMethodID(mediaClass, "prepare", "()V");
    jmethodID start = env->GetMethodID(mediaClass, "start", "()V");
    jmethodID pause = env->GetMethodID(mediaClass, "pause", "()V");
    jmethodID stop = env->GetMethodID(mediaClass, "stop", "()V");
    jmethodID release = env->GetMethodID(mediaClass, "release", "()V");
    jmethodID seekTo = env->GetMethodID(mediaClass, "seekTo", "(I)V");
    jmethodID getDuration = env->GetMethodID(mediaClass, "getDuration", "()I");
    jmethodID getPosition =
        env->GetMethodID(mediaClass, "getCurrentPosition", "()I");
    jmethodID getWidth = env->GetMethodID(mediaClass, "getVideoWidth", "()I");
    jmethodID getHeight = env->GetMethodID(mediaClass, "getVideoHeight", "()I");
    jmethodID isPlaying = env->GetMethodID(mediaClass, "isPlaying", "()Z");
    jmethodID setPlaybackParams = env->GetMethodID(
        mediaClass, "setPlaybackParams",
        "(Landroid/media/PlaybackParams;)V");
    if (setPlaybackParams == nullptr) {
      ClearJavaException(env, "optional MediaPlayer speed API", false);
    }

    jclass paramsClass = env->FindClass("android/media/PlaybackParams");
    if (paramsClass == nullptr) {
      ClearJavaException(env, "optional PlaybackParams API", false);
    }
    jmethodID paramsConstructor =
        paramsClass != nullptr ? env->GetMethodID(paramsClass, "<init>", "()V")
                               : nullptr;
    jmethodID paramsSetSpeed =
        paramsClass != nullptr
            ? env->GetMethodID(paramsClass, "setSpeed",
                               "(F)Landroid/media/PlaybackParams;")
            : nullptr;
    if (paramsClass != nullptr &&
        (paramsConstructor == nullptr || paramsSetSpeed == nullptr)) {
      ClearJavaException(env, "optional PlaybackParams methods", false);
      paramsConstructor = nullptr;
      paramsSetSpeed = nullptr;
    }

    if (constructor == nullptr || setDataSource == nullptr || setSurface == nullptr ||
        setVolume == nullptr || setLooping == nullptr || prepare == nullptr ||
        start == nullptr || pause == nullptr || stop == nullptr ||
        release == nullptr || seekTo == nullptr || getDuration == nullptr ||
        getPosition == nullptr || getWidth == nullptr || getHeight == nullptr ||
        isPlaying == nullptr) {
      ClearJavaException(env, "required MediaPlayer API lookup", false);
      SetError(label + ": Android MediaPlayer API is incomplete", true);
      return;
    }

    jobject mediaPlayer = nullptr;
    bool prepared = false;

    auto applySpeed = [&](float value) {
      if (!prepared || mediaPlayer == nullptr || setPlaybackParams == nullptr ||
          paramsClass == nullptr || paramsConstructor == nullptr ||
          paramsSetSpeed == nullptr) {
        return;
      }
      jobject params = env->NewObject(paramsClass, paramsConstructor);
      if (params == nullptr || ClearJavaException(env, "PlaybackParams construction", false)) {
        return;
      }
      jobject configured = env->CallObjectMethod(params, paramsSetSpeed, value);
      if (!ClearJavaException(env, "PlaybackParams speed", false)) {
        env->CallVoidMethod(mediaPlayer, setPlaybackParams, params);
        ClearJavaException(env, "MediaPlayer speed", false);
      }
      if (configured != nullptr && configured != params) {
        env->DeleteLocalRef(configured);
      }
      env->DeleteLocalRef(params);
    };

    while (true) {
      std::deque<Command> pending;
      {
        std::unique_lock lock(commandMutex);
        commandCv.wait_for(lock, 25ms, [&] { return !commands.empty(); });
        pending.swap(commands);
      }

      for (auto const& command : pending) {
        if (command.type == CommandType::Open) {
          ReleaseMediaPlayer(env, mediaPlayer, release);
          prepared = false;
          durationMs.store(0);
          positionMs.store(0);
          videoWidth.store(0);
          videoHeight.store(0);
          currentSpeed.store(1.0f);
          ResetFrame();

          {
            std::unique_lock lock(commandMutex);
            if (!commandCv.wait_for(lock, 10s, [&] {
                  return surfaceReady || state.load() == State::Failed;
                })) {
              lock.unlock();
              SetError(label + ": decoder surface creation timed out", true);
              continue;
            }
          }
          if (state.load() == State::Failed) continue;

          state.store(State::Preparing);
          jobject local = env->NewObject(mediaClass, constructor);
          if (local == nullptr ||
              ClearJavaException(env, "MediaPlayer construction", true)) {
            continue;
          }
          mediaPlayer = env->NewGlobalRef(local);
          env->DeleteLocalRef(local);
          jstring source = NewJavaString(env, command.path);
          if (source == nullptr ||
              ClearJavaException(env, "media path conversion", true)) {
            continue;
          }
          env->CallVoidMethod(mediaPlayer, setDataSource, source);
          env->DeleteLocalRef(source);
          if (ClearJavaException(env, "MediaPlayer data source", true)) continue;
          env->CallVoidMethod(mediaPlayer, setSurface, surface);
          if (ClearJavaException(env, "MediaPlayer surface", true)) continue;
          env->CallVoidMethod(mediaPlayer, setVolume, 0.0f, 0.0f);
          env->CallVoidMethod(mediaPlayer, setLooping,
                              static_cast<jboolean>(command.looping));
          if (ClearJavaException(env, "MediaPlayer configuration", true)) continue;

          // Local map files prepare on a dedicated decoder thread. Blocking
          // prepare is intentional here: no listener proxy/custom Java class is
          // injected into Beat Saber's APK, and the Unity game thread never waits.
          env->CallVoidMethod(mediaPlayer, prepare);
          if (ClearJavaException(env, "MediaPlayer prepare", true)) continue;
          prepared = true;
          int const width = env->CallIntMethod(mediaPlayer, getWidth);
          int const height = env->CallIntMethod(mediaPlayer, getHeight);
          int const duration = env->CallIntMethod(mediaPlayer, getDuration);
          if (ClearJavaException(env, "MediaPlayer metadata", true)) continue;
          videoWidth.store(std::max(0, width));
          videoHeight.store(std::max(0, height));
          durationMs.store(std::max(0, duration));
          if (surfaceTexture != nullptr && defaultBufferMethod != nullptr &&
              width > 0 && height > 0) {
            env->CallVoidMethod(surfaceTexture, defaultBufferMethod, width, height);
            ClearJavaException(env, "SurfaceTexture buffer size", false);
          }
          state.store(State::Ready);
        } else if (command.type == CommandType::Play && prepared &&
                   mediaPlayer != nullptr) {
          env->CallVoidMethod(mediaPlayer, start);
          if (!ClearJavaException(env, "MediaPlayer start", true)) {
            state.store(State::Playing);
          }
        } else if (command.type == CommandType::Pause && prepared &&
                   mediaPlayer != nullptr) {
          env->CallVoidMethod(mediaPlayer, pause);
          if (!ClearJavaException(env, "MediaPlayer pause", true)) {
            state.store(State::Paused);
          }
        } else if (command.type == CommandType::Seek && prepared &&
                   mediaPlayer != nullptr) {
          double const duration = static_cast<double>(durationMs.load()) / 1000.0;
          double const bounded =
              std::clamp(command.value, 0.0, duration > 0.0 ? duration : 86400.0);
          int const milliseconds = static_cast<int>(std::llround(bounded * 1000.0));
          env->CallVoidMethod(mediaPlayer, seekTo, milliseconds);
          ClearJavaException(env, "MediaPlayer seek", false);
        } else if (command.type == CommandType::Speed) {
          float const speed =
              std::clamp(static_cast<float>(command.value), 0.1f, 4.0f);
          applySpeed(speed);
          currentSpeed.store(speed);
        } else if (command.type == CommandType::Loop && prepared &&
                   mediaPlayer != nullptr) {
          env->CallVoidMethod(mediaPlayer, setLooping,
                              static_cast<jboolean>(command.looping));
          ClearJavaException(env, "MediaPlayer looping", false);
        } else if (command.type == CommandType::Stop) {
          if (prepared && mediaPlayer != nullptr) {
            env->CallVoidMethod(mediaPlayer, stop);
            ClearJavaException(env, "MediaPlayer stop", false);
          }
          ReleaseMediaPlayer(env, mediaPlayer, release);
          prepared = false;
          durationMs.store(0);
          positionMs.store(0);
          videoWidth.store(0);
          videoHeight.store(0);
          currentSpeed.store(1.0f);
          ResetFrame();
          if (state.load() != State::Failed) state.store(State::Idle);
        }
      }

      if (prepared && mediaPlayer != nullptr && state.load() != State::Failed) {
        int const position = env->CallIntMethod(mediaPlayer, getPosition);
        jboolean const playing = env->CallBooleanMethod(mediaPlayer, isPlaying);
        if (!ClearJavaException(env, "MediaPlayer status", true)) {
          positionMs.store(std::max(0, position));
          if (playing == JNI_TRUE) {
            state.store(State::Playing);
          } else if (state.load() == State::Playing) {
            state.store(State::Ready);
          }
        }
      }
    }
  }

  int const outputWidth;
  int const outputHeight;
  std::string const label;
  int const eventId;

  std::atomic<JavaVM*> vm{nullptr};
  std::atomic<State> state{State::WaitingForSurface};
  std::atomic<bool> backendFatal{false};
  std::atomic<bool> renderInitialized{false};
  std::atomic<bool> frameResetRequested{true};
  std::atomic<bool> hasFrame{false};
  std::atomic<std::uint64_t> frameSerial{0};
  std::atomic<GLuint> outputTextureId{0};
  std::atomic<int> durationMs{0};
  std::atomic<int> positionMs{0};
  std::atomic<int> videoWidth{0};
  std::atomic<int> videoHeight{0};
  std::atomic<float> currentSpeed{1.0f};

  std::mutex errorMutex;
  std::string error;
  std::mutex commandMutex;
  std::condition_variable commandCv;
  std::deque<Command> commands;
  bool surfaceReady = false;

  GLuint externalTexture = 0;
  GLuint outputTexture = 0;
  GLuint framebuffer = 0;
  GLuint program = 0;
  GLuint vertexArray = 0;
  GLint transformLocation = -1;
  GLint samplerLocation = -1;
  jlong lastTimestamp = 0;

  jobject surfaceTexture = nullptr;
  jobject surface = nullptr;
  jfloatArray transformArray = nullptr;
  jmethodID updateTextureMethod = nullptr;
  jmethodID transformMethod = nullptr;
  jmethodID timestampMethod = nullptr;
  jmethodID defaultBufferMethod = nullptr;
};

namespace {

extern "C" void QuestNativeVideoRenderEvent(int eventId) {
  std::shared_ptr<Player::Impl> implementation;
  {
    std::lock_guard lock(gRegistryMutex);
    auto iterator = gRegistry.find(eventId);
    if (iterator == gRegistry.end()) return;
    implementation = iterator->second.lock();
  }
  if (!implementation) return;
  implementation->Render(GetEnv(implementation->vm.load()));
}

}  // namespace

std::shared_ptr<Player> Player::Create(int outputWidth, int outputHeight,
                                       std::string label) {
  auto implementation =
      std::make_shared<Impl>(outputWidth, outputHeight, std::move(label));
  {
    std::lock_guard lock(gRegistryMutex);
    gRegistry[implementation->eventId] = implementation;
  }
  implementation->Start();
  return std::shared_ptr<Player>(new Player(std::move(implementation)));
}

Player::Player(std::shared_ptr<Impl> implementation)
    : _impl(std::move(implementation)) {}

void Player::Tick() {
  if (!_impl || _impl->state.load() == State::Failed) return;
  if (_impl->vm.load() == nullptr) {
    std::int64_t const pointer = UnityEngine::AndroidJNI::GetJavaVM().ToInt64();
    auto* vm = reinterpret_cast<JavaVM*>(static_cast<std::uintptr_t>(pointer));
    if (vm == nullptr) {
      _impl->SetError(
          _impl->label + ": Unity did not expose the Android Java VM", true);
      return;
    }
    _impl->vm.store(vm);
    _impl->commandCv.notify_all();
  }
  UnityEngine::GL::IssuePluginEvent(
      System::IntPtr(reinterpret_cast<void*>(&QuestNativeVideoRenderEvent)),
      _impl->eventId);
  (void)Texture();
}

void Player::Open(std::string path, bool looping) {
  if (!_impl || path.empty()) return;
  if (_impl->backendFatal.load()) return;
  {
    std::lock_guard lock(_impl->errorMutex);
    _impl->error.clear();
  }
  _impl->state.store(State::Preparing);
  _impl->ResetFrame();
  _impl->Enqueue({Impl::CommandType::Open, std::move(path), looping, 0.0});
}

void Player::Play() {
  if (_impl) _impl->Enqueue({Impl::CommandType::Play, {}, false, 0.0});
}

void Player::Pause() {
  if (_impl) _impl->Enqueue({Impl::CommandType::Pause, {}, false, 0.0});
}

void Player::Stop() {
  if (_impl) _impl->Enqueue({Impl::CommandType::Stop, {}, false, 0.0});
}

void Player::Seek(double seconds) {
  if (_impl && std::isfinite(seconds)) {
    _impl->Enqueue({Impl::CommandType::Seek, {}, false, seconds});
  }
}

void Player::SetPlaybackSpeed(float speed) {
  if (_impl && std::isfinite(speed)) {
    _impl->Enqueue({Impl::CommandType::Speed, {}, false, speed});
  }
}

void Player::SetLooping(bool looping) {
  if (_impl) {
    _impl->Enqueue({Impl::CommandType::Loop, {}, looping, 0.0});
  }
}

State Player::GetState() const {
  return _impl ? _impl->state.load() : State::Failed;
}

bool Player::IsPrepared() const {
  State const value = GetState();
  return value == State::Ready || value == State::Playing ||
         value == State::Paused;
}

bool Player::IsPlaying() const { return GetState() == State::Playing; }
bool Player::HasFrame() const { return _impl && _impl->hasFrame.load(); }
bool Player::Failed() const { return GetState() == State::Failed; }

double Player::TimeSeconds() const {
  return _impl ? static_cast<double>(_impl->positionMs.load()) / 1000.0 : 0.0;
}

double Player::DurationSeconds() const {
  return _impl ? static_cast<double>(_impl->durationMs.load()) / 1000.0 : 0.0;
}

float Player::PlaybackSpeed() const {
  return _impl ? _impl->currentSpeed.load() : 1.0f;
}

int Player::VideoWidth() const { return _impl ? _impl->videoWidth.load() : 0; }
int Player::VideoHeight() const { return _impl ? _impl->videoHeight.load() : 0; }

std::uint64_t Player::FrameSerial() const {
  return _impl ? _impl->frameSerial.load() : 0;
}

std::string Player::Error() const {
  if (!_impl) return "native player unavailable";
  std::lock_guard lock(_impl->errorMutex);
  return _impl->error;
}

UnityEngine::Texture2D* Player::Texture() {
  if (_texture) return _texture.ptr();
  if (!_impl) return nullptr;
  GLuint const identifier = _impl->outputTextureId.load();
  if (identifier == 0) return nullptr;
  auto texture = UnityEngine::Texture2D::CreateExternalTexture(
      _impl->outputWidth, _impl->outputHeight,
      UnityEngine::TextureFormat::RGBA32, false, false,
      System::IntPtr(reinterpret_cast<void*>(static_cast<std::uintptr_t>(identifier))));
  auto* rawTexture = texture.unsafePtr();
  if (rawTexture != nullptr) {
    _texture = rawTexture;
    rawTexture->set_filterMode(UnityEngine::FilterMode::Bilinear);
    rawTexture->set_wrapMode(UnityEngine::TextureWrapMode::Clamp);
  }
  return rawTexture;
}

}  // namespace QuestNativeVideo
