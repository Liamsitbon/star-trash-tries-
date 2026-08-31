#include "NexoraLifecycle.hpp"

#include <cassert>

int main() {
  Nexora::RuntimeLifecycle lifecycle;
  assert(lifecycle.IsDormant());
  assert(!lifecycle.IsActive());

  auto const firstGeneration = lifecycle.BeginPreparation();
  assert(!lifecycle.Activate(firstGeneration + 1));
  assert(lifecycle.Activate(firstGeneration));
  assert(lifecycle.IsActive());

  assert(lifecycle.TryEnterRender(firstGeneration));
  assert(lifecycle.RenderDepth() == 1);
  auto const retirementGeneration = lifecycle.BeginRetirement();
  assert(retirementGeneration != firstGeneration);
  assert(!lifecycle.CompleteRetirement());
  lifecycle.LeaveRender();
  assert(lifecycle.CompleteRetirement());
  assert(lifecycle.IsDormant());

  auto const secondGeneration = lifecycle.BeginPreparation();
  assert(secondGeneration != retirementGeneration);
  assert(lifecycle.Activate(secondGeneration));
  lifecycle.Suspend();
  assert(lifecycle.IsSuspended());
  lifecycle.Resume();
  assert(lifecycle.IsActive());
  assert(!lifecycle.TryEnterRender(firstGeneration));

  [[maybe_unused]] auto const finalRetirementGeneration =
      lifecycle.BeginRetirement();
  assert(lifecycle.CompleteRetirement());
  return 0;
}
