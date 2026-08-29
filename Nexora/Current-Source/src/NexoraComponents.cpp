#include "NexoraComponents.hpp"
#include "NexoraRuntime.hpp"

DEFINE_TYPE(Nexora, RuntimeBehaviour);

namespace Nexora {

void RuntimeBehaviour::Update() { Runtime::Instance().Update(); }

void RuntimeBehaviour::OnApplicationPause(bool paused) {
  Runtime::Instance().SetApplicationPaused(paused);
}

void RuntimeBehaviour::OnApplicationFocus(bool focused) {
  Runtime::Instance().SetFocused(focused);
}

void RuntimeBehaviour::OnDestroy() { Runtime::Instance().OnBehaviourDestroyed(this); }

}  // namespace Nexora
