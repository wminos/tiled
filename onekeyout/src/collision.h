#pragma once

#include "leveldata.h"

namespace OneKeyOut {

// Touching edges are allowed; only positive-area overlap blocks movement.
bool hasTerrainClearance(const LevelData &level, const QRectF &body);

// Sweep each axis to exact contact, then slide along the other axis.
// Returns the permitted displacement; extraSolid is the closed gate, if any.
QPointF slideMove(const LevelData &level, QRectF body, QPointF displacement,
                  const QRectF &extraSolid = {});

} // namespace OneKeyOut
