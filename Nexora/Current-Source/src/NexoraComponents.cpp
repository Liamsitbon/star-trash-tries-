#include "NexoraComponents.hpp"
#include "NexoraRuntime.hpp"
#include "main.hpp"

#include <exception>

DEFINE_TYPE(Nexora, RuntimeBehaviour);

namespace Nexora {

namespace {

template <typename Action>
void RunUnityMessageBoundary(char const* messageName, Action&& action) noexcept {
  try {
    action();
  } catch (std::exception const& exception) {
    try {
      PaperLogger.error("Nexora Unity message '{}' failed safely: {}", messageName,
                        exception.what());
    } catch (...) {
    }
  } catch (...) {
    try {
      PaperLogger.error("Nexora Unity message '{}' failed safely", messageName);
    } catch (...) {
    }
  }
}

}  // namespace

void RuntimeBehaviour::Update() {
  RunUnityMessageBoundary("Update", [] { Runtime::Instance().Update(); });
}

void RuntimeBehaviour::OnApplicationPause(bool paused) {
  RunUnityMessageBoundary("OnApplicationPause", [paused] {
    Runtime::Instance().SetApplicationPaused(paused);
  });
}

void RuntimeBehaviour::OnApplicationFocus(bool focused) {
  RunUnityMessageBoundary("OnApplicationFocus", [focused] {
    Runtime::Instance().SetFocused(focused);
  });
}

void RuntimeBehaviour::OnDestroy() {
  auto* behaviour = this;
  RunUnityMessageBoundary("OnDestroy", [behaviour] {
    Runtime::Instance().OnBehaviourDestroyed(behaviour);
  });
}

}  // namespace Nexora
