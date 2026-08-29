#include "VivifyLifecycle.hpp"

#include <cassert>

using Vivify::RuntimeLifecycle;
using Vivify::RuntimePhase;

int main() {
  RuntimeLifecycle lifecycle;
  assert(lifecycle.Phase() == RuntimePhase::Dormant);

  auto const first = lifecycle.BeginPreparation();
  assert(lifecycle.Phase() == RuntimePhase::Preparing);
  assert(!lifecycle.CanRender(first));
  assert(lifecycle.Activate(first));
  assert(lifecycle.TryEnterRender(first));
  assert(lifecycle.RenderDepth() == 1);

  // Retirement invalidates the in-flight scene generation immediately, but
  // completion waits until its render scope has left.
  auto const retired = lifecycle.BeginRetirement();
  assert(retired != first);
  assert(!lifecycle.CanRender(first));
  assert(!lifecycle.CompleteRetirement());
  lifecycle.LeaveRender();
  assert(lifecycle.CompleteRetirement());

  auto const second = lifecycle.BeginPreparation();
  assert(second != first);
  assert(lifecycle.Activate(second));
  assert(!lifecycle.TryEnterRender(first));
  assert(lifecycle.TryEnterRender(second));
  lifecycle.LeaveRender();

  lifecycle.Suspend();
  assert(lifecycle.Phase() == RuntimePhase::Suspended);
  assert(!lifecycle.TryEnterRender(second));
  lifecycle.Resume();
  assert(lifecycle.TryEnterRender(second));
  lifecycle.LeaveRender();

  return 0;
}
