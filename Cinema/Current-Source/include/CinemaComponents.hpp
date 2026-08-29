#pragma once

#include "UnityEngine/MonoBehaviour.hpp"
#include "custom-types/shared/macros.hpp"

DECLARE_CLASS_CODEGEN(CinemaQuest, RuntimeBehaviour, UnityEngine::MonoBehaviour) {
  DECLARE_DEFAULT_CTOR();
  DECLARE_SIMPLE_DTOR();
  DECLARE_INSTANCE_METHOD(void, Update);
  DECLARE_INSTANCE_METHOD(void, OnApplicationPause, bool paused);
  DECLARE_INSTANCE_METHOD(void, OnApplicationFocus, bool focused);
  DECLARE_INSTANCE_METHOD(void, OnDestroy);
};
