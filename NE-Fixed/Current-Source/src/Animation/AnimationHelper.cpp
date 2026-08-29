#include "GlobalNamespace/BeatmapCallbacksController.hpp"
#include "GlobalNamespace/BeatmapObjectSpawnController.hpp"
#include "GlobalNamespace/StaticBeatmapObjectSpawnMovementData.hpp"
#include "GlobalNamespace/BeatmapObjectSpawnMovementData.hpp"
#include "custom-json-data/shared/CustomBeatmapData.h"
#include "tracks/shared/Animation/PointDefinition.h"
#include "Animation/AnimationHelper.h"
#include "AssociatedData.h"
#include "NELogger.h"
#include "NECaches.h"

using namespace AnimationHelper;
using namespace GlobalNamespace;
using namespace NEVector;
using namespace CustomJSONData;
using namespace Animation;

// BeatmapObjectCallbackController.cpp
extern BeatmapCallbacksController* callbackController;

// Events.cpp
extern BeatmapObjectSpawnController* spawnController;

constexpr std::optional<Vector3> operator+(std::optional<Vector3> const& a, std::optional<Vector3> const& b) {
  if (a && b) {
    return *a + *b;
  }
  if (a) {
    return a;
  }

  if (b) {
    return b;
  }

  return std::nullopt;
}

template <typename T> constexpr std::optional<T> operator*(std::optional<T> const& a, std::optional<T> const& b) {
  if (a && b) {
    return *a * *b;
  }
  if (a) {
    return a;
  }
  if (b) {
    return b;
  }
  return std::nullopt;
}

std::optional<NEVector::Vector3> AnimationHelper::GetDefinitePositionOffset(AnimationObjectData const& animationData,
                                                                            std::span<TrackW const> tracks,
                                                                            float time) {
  auto const& localDefinitePosition = animationData.definitePosition;

  std::optional<Vector3> pathDefinitePosition =
      localDefinitePosition ? std::optional(localDefinitePosition.InterpolateVec3(time)) : std::nullopt;

  // track animation only
  if (!pathDefinitePosition && !tracks.empty()) {
    if (tracks.size() == 1) {
      auto track = tracks.front();
      pathDefinitePosition = track.GetPathPropertyNamed(PropertyNames::DefinitePosition).InterpolateVec3(time);
    } else {
      auto positions = Animation::getPathPropertiesVec3(tracks, PropertyNames::DefinitePosition, time);
      pathDefinitePosition = Animation::addVector3s(positions);
    }
  }

  if (!pathDefinitePosition) return std::nullopt;

  auto const& position = animationData.position;
  std::optional<Vector3> pathPosition = position ? std::optional(position.InterpolateVec3(time)) : std::nullopt;
  std::optional<Vector3> trackPosition;

  std::optional<Vector3> positionOffset;

  if (!tracks.empty()) {
    if (tracks.size() == 1) {
      auto track = tracks.front();

      if (!pathPosition) pathPosition = track.GetPathPropertyNamed(PropertyNames::OffsetPosition).InterpolateVec3(time);

      trackPosition = track.GetPropertyNamed(PropertyNames::OffsetPosition).GetVec3();
    } else {
      trackPosition =
          Animation::addVector3s(Animation::getPropertiesVec3(tracks, PropertyNames::OffsetPosition, TimeUnit()));

      if (!pathPosition)
        pathPosition =
            Animation::addVector3s(Animation::getPathPropertiesVec3(tracks, PropertyNames::OffsetPosition, time));
    }

    positionOffset = pathPosition + trackPosition;
  } else {
    positionOffset = pathPosition;
  }

  std::optional<Vector3> definitePosition = positionOffset + pathDefinitePosition;

  if (definitePosition) {
    definitePosition =
        definitePosition.value() * GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance;
  }

  if (NECaches::LeftHandedMode) {
    definitePosition = Animation::MirrorVectorNullable(definitePosition);
  }

  return definitePosition;
}

ObjectOffset AnimationHelper::GetObjectOffset(AnimationObjectData const& animationData, std::span<TrackW const> tracks,
                                              float time) {
  ObjectOffset offset;

  auto const& position = animationData.position;
  auto const& rotation = animationData.rotation;
  auto const& scale = animationData.scale;
  auto const& localRotation = animationData.localRotation;
  auto const& dissolve = animationData.dissolve;
  auto const& dissolveArrow = animationData.dissolveArrow;
  auto const& cuttable = animationData.cuttable;

  // Get path properties from animation data
  std::optional<Vector3> pathPosition = position ? std::optional(position.InterpolateVec3(time)) : std::nullopt;
  std::optional<Quaternion> pathRotation =
      rotation ? std::optional(rotation.InterpolateQuaternion(time)) : std::nullopt;
  std::optional<Vector3> pathScale = scale ? std::optional(scale.InterpolateVec3(time)) : std::nullopt;
  std::optional<Quaternion> pathLocalRotation =
      localRotation ? std::optional(localRotation.InterpolateQuaternion(time)) : std::nullopt;
  std::optional<Vector3> pathLocalPosition;
  std::optional<float> pathDissolve = dissolve ? std::optional(dissolve.InterpolateLinear(time)) : std::nullopt;
  std::optional<float> pathDissolveArrow =
      dissolveArrow ? std::optional(dissolveArrow.InterpolateLinear(time)) : std::nullopt;
  std::optional<float> pathCuttable = cuttable ? std::optional(cuttable.InterpolateLinear(time)) : std::nullopt;

  if (!tracks.empty()) {
    if (tracks.size() == 1) {
      auto const track = tracks.front();

      auto pathProperties = track.GetPathPropertiesValuesW(time);
      auto properties = track.GetPropertiesValuesW();

      // Macros to simplify getter code
      if (!pathPosition) pathPosition = track.v2 ? pathProperties.position : pathProperties.offsetPosition;
      if (!pathRotation) pathRotation = track.v2 ? pathProperties.rotation : pathProperties.offsetRotation;
      if (!pathScale) pathScale = pathProperties.scale;
      if (!pathLocalRotation) pathLocalRotation = pathProperties.localRotation;
      if (!pathLocalPosition) pathLocalPosition = pathProperties.localPosition;

      if (!pathDissolve) pathDissolve = pathProperties.dissolve;
      if (!pathDissolveArrow) pathDissolveArrow = pathProperties.dissolveArrow;
      if (!pathCuttable) pathCuttable = pathProperties.cuttable;

      // Combine with track properties
      offset.positionOffset = pathPosition + (track.v2 ? properties.position : properties.offsetPosition);
      offset.localPositionOffset = pathLocalPosition + properties.localPosition;
      offset.rotationOffset = pathRotation * (track.v2 ? properties.rotation : properties.offsetRotation);
      offset.scaleOffset = pathScale * properties.scale;
      offset.localRotationOffset = pathLocalRotation * properties.localRotation;
      offset.dissolve = pathDissolve * properties.dissolve;
      offset.dissolveArrow = pathDissolveArrow * properties.dissolveArrow;
      offset.cuttable = pathCuttable * properties.cuttable;
    } else {
      bool v2 = tracks.front().v2;

      // Multiple tracks - combine their properties
      if (!pathPosition) {
        auto positionPaths = Animation::getPathPropertiesVec3(tracks, v2 ? PropertyNames::Position : PropertyNames::OffsetPosition, time);
        pathPosition = Animation::addVector3s(positionPaths);
      }
      if (!pathRotation) {
        auto rotationPaths = Animation::getPathPropertiesQuat(tracks, v2 ? PropertyNames::Rotation : PropertyNames::OffsetRotation, time);
        pathRotation = Animation::multiplyQuaternions(rotationPaths);
      }
      if (!pathScale) {
        auto scalePaths = Animation::getPathPropertiesVec3(tracks, PropertyNames::Scale, time);
        pathScale = Animation::multiplyVector3s(scalePaths);
      }
      if (!pathLocalRotation) {
        auto localRotationPaths = Animation::getPathPropertiesQuat(tracks, PropertyNames::LocalRotation, time);
        pathLocalRotation = Animation::multiplyQuaternions(localRotationPaths);
      }
      if (!pathLocalPosition) {
        auto localPositionPaths = Animation::getPathPropertiesVec3(tracks, PropertyNames::LocalPosition, time);
        pathLocalPosition = Animation::addVector3s(localPositionPaths);
      }
      if (!pathDissolve) {
        auto dissolvePaths = Animation::getPathPropertiesFloat(tracks, PropertyNames::Dissolve, time);
        pathDissolve = Animation::multiplyFloats(dissolvePaths);
      }
      if (!pathDissolveArrow) {
        auto dissolveArrowPaths = Animation::getPathPropertiesFloat(tracks, PropertyNames::DissolveArrow, time);
        pathDissolveArrow = Animation::multiplyFloats(dissolveArrowPaths);
      }
      if (!pathCuttable) {
        auto cuttablePaths = Animation::getPathPropertiesFloat(tracks, PropertyNames::Cuttable, time);
        pathCuttable = Animation::multiplyFloats(cuttablePaths);
      }


      // Combine track properties with path properties
      auto trackPositions =
          Animation::getPropertiesVec3(tracks, v2 ? PropertyNames::Position : PropertyNames::OffsetPosition, {});
      auto trackRotations = Animation::getPropertiesQuat(tracks, v2 ? PropertyNames::Rotation : PropertyNames::OffsetRotation, {});
      auto trackScales = Animation::getPropertiesVec3(tracks, PropertyNames::Scale, {});
      auto trackLocalRotations = Animation::getPropertiesQuat(tracks, PropertyNames::LocalRotation, {});
      auto trackLocalPositions = Animation::getPropertiesVec3(tracks, PropertyNames::LocalPosition, {});
      auto trackDissolves = Animation::getPropertiesFloat(tracks, PropertyNames::Dissolve, {});
      auto trackDissolveArrows = Animation::getPropertiesFloat(tracks, PropertyNames::DissolveArrow, {});
      auto trackCuttables = Animation::getPropertiesFloat(tracks, PropertyNames::Cuttable, {});

      // Calculate combined track values
      auto combinedTrackPositions = Animation::addVector3s(trackPositions);
      auto combinedTrackRotations = Animation::multiplyQuaternions(trackRotations);
      auto combinedTrackScales = Animation::multiplyVector3s(trackScales);
      auto combinedTrackLocalRotations = Animation::multiplyQuaternions(trackLocalRotations);
      auto combinedTrackLocalPositions = Animation::addVector3s(trackLocalPositions);
      auto combinedTrackDissolves = Animation::multiplyFloats(trackDissolves);
      auto combinedTrackDissolveArrows = Animation::multiplyFloats(trackDissolveArrows);
      auto combinedTrackCuttables = Animation::multiplyFloats(trackCuttables);

      // all paths are non-null by now
      // Set final property values
      offset.positionOffset = pathPosition + combinedTrackPositions;
      offset.localPositionOffset = pathLocalPosition + combinedTrackLocalPositions;
      offset.rotationOffset = pathRotation * combinedTrackRotations;
      offset.scaleOffset = pathScale * combinedTrackScales;
      offset.localRotationOffset = pathLocalRotation * combinedTrackLocalRotations;
      offset.dissolve = pathDissolve * combinedTrackDissolves;
      offset.dissolveArrow = pathDissolveArrow * combinedTrackDissolveArrows;
      offset.cuttable = pathCuttable * combinedTrackCuttables;
    }
  } else {
    // No tracks - use animation data only
    offset.positionOffset = pathPosition;
    offset.localPositionOffset = pathLocalPosition;
    offset.rotationOffset = pathRotation;
    offset.scaleOffset = pathScale;
    offset.localRotationOffset = pathLocalRotation;
    offset.dissolve = pathDissolve;
    offset.dissolveArrow = pathDissolveArrow;
    offset.cuttable = pathCuttable;
  }

  // Apply scale and mirroring
  if (offset.positionOffset)
    offset.positionOffset = offset.positionOffset.value() * StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance;

  if (NECaches::LeftHandedMode) {
    offset.rotationOffset = Animation::MirrorQuaternionNullable(offset.rotationOffset);
    offset.localRotationOffset = Animation::MirrorQuaternionNullable(offset.localRotationOffset);
    offset.positionOffset = Animation::MirrorVectorNullable(offset.positionOffset);
    offset.localPositionOffset = Animation::MirrorVectorNullable(offset.localPositionOffset);
    offset.scaleOffset = Animation::MirrorVectorNullable(offset.scaleOffset);
  }

  return offset;
}
