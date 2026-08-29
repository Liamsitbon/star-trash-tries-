#include "CinemaComponents.hpp"

#include "CinemaRuntime.hpp"

DEFINE_TYPE(CinemaQuest, RuntimeBehaviour);

namespace CinemaQuest {

void RuntimeBehaviour::Update() { Runtime::Instance().Update(); }

void RuntimeBehaviour::OnApplicationPause(bool paused) {
  Runtime::Instance().SetApplicationPaused(paused);
}

void RuntimeBehaviour::OnApplicationFocus(bool focused) {
  Runtime::Instance().SetFocused(focused);
}

void RuntimeBehaviour::OnDestroy() { Runtime::Instance().OnBehaviourDestroyed(this); }

}  // namespace CinemaQuest
