#include "NELogger.h"
#include "SpawnDataHelper.h"

#include "GlobalNamespace/BeatmapObjectSpawnMovementData.hpp"
#include "GlobalNamespace/BeatmapObjectData.hpp"
#include "GlobalNamespace/NoteData.hpp"
#include "GlobalNamespace/BeatmapObjectSpawnController.hpp"

#include "custom-json-data/shared/CustomBeatmapData.h"

#include "tracks/shared/Vector.h"
#include "NECaches.h"

using namespace GlobalNamespace;

float SpawnDataHelper::HighestJumpPosYForLineLayer(float lineLayer)
{
    // Magic numbers found with linear regression against the game's existing values.
    return (0.525f * lineLayer) + 0.858333f + NECaches::JumpOffsetYProvider->jumpOffsetY;
}

float SpawnDataHelper::GetGravityBase(float noteLineLayer, float beforeJumpLineLayer)
{
  return HighestJumpPosYForLineLayer(noteLineLayer) - LineYPosForLineLayer(beforeJumpLineLayer);
}

float SpawnDataHelper::LineYPosForLineLayer(float height) {
  return GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kBaseLinesYPos +
         (height * GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance);
}
