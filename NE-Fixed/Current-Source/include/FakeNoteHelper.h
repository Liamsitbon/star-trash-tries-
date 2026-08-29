#pragma once

#include <vector>
#include "custom-json-data/shared/VList.h"


namespace CustomJSONData::v3 {
class CustomBeatmapSaveData;
}

namespace GlobalNamespace {
class NoteController;
class NoteData;
class SliderData;
class ObstacleController;
} // namespace GlobalNamespace

namespace System::Collections::Generic {
template <typename T> class List_1;
}

namespace FakeNoteHelper {

// Ensures V3 fake arrays are appended to CJD's normal save-data lists exactly once.
// Returns true when fake objects were already injected or were injected by this call.
bool EnsureV3FakeObjectsInjected(CustomJSONData::v3::CustomBeatmapSaveData* beatmap);

bool GetFakeNote(GlobalNamespace::NoteData* noteData);
bool GetCuttable(GlobalNamespace::NoteData* noteData);
bool GetAttractableArc(GlobalNamespace::SliderData* arcData);

System::Collections::Generic::List_1<GlobalNamespace::ObstacleController*>*
ObstacleFakeCheck(VList<GlobalNamespace::ObstacleController*> intersectingObstacles);
} // namespace FakeNoteHelper