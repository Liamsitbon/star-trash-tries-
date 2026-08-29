#pragma once

#include "Animation/NoodleMovementDataProvider.hpp"

#include "tracks/shared/VariableMovementHelper.hpp"

// default to NoodleMovementDataProvider
using VariableMovementW = VariableMovementWrapper<NoodleExtensions::NoodleMovementDataProvider>;