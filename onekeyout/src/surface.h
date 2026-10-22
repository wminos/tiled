#pragma once

#include "leveldata.h"

namespace OneKeyOut {

enum class Surface { Stone, Water, Boardwalk };

// Sample the ground anchor, not the sprite frame or its collision bounds.
Surface surfaceAt(const LevelData &level, QPointF position);
qreal movementScale(Surface surface);

} // namespace OneKeyOut
