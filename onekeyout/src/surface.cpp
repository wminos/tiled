#include "surface.h"

#include <cmath>

namespace OneKeyOut {

Surface surfaceAt(const LevelData &level, QPointF position)
{
    if (!std::isfinite(position.x()) || !std::isfinite(position.y()) || level.tileSize <= 0 ||
        position.x() < 0 || position.y() < 0 ||
        position.x() >= level.bounds().right() || position.y() >= level.bounds().bottom())
        return Surface::Stone;
    const int x = int(position.x()) / level.tileSize;
    const int y = int(position.y()) / level.tileSize;
    const int index = y * level.width + x;
    const qreal u = position.x() / level.tileSize - x;
    const qreal v = position.y() / level.tileSize - y;
    const int edges = level.boardwalkEdges.value(index, -1);
    // Four 14px-wide arms around the central platform, matching authored art.
    const bool across = u >= 9.0 / 32 && u < 23.0 / 32;
    const bool along = v >= 9.0 / 32 && v < 23.0 / 32;
    if (edges >= 0 && ((across && along) ||
        ((edges & 1) && across && v < .5) || ((edges & 2) && along && u >= .5) ||
        ((edges & 4) && across && v >= .5) || ((edges & 8) && along && u < .5)))
        return Surface::Boardwalk;
    const int corners = level.waterCorners.value(index, 0);
    const qreal coverage = ((corners >> 3) & 1) * (1-u) * (1-v) +
                           (corners & 1) * u * (1-v) +
                           ((corners >> 2) & 1) * (1-u) * v +
                           ((corners >> 1) & 1) * u * v;
    return coverage >= .5 ? Surface::Water : Surface::Stone;
}

qreal movementScale(Surface surface)
{
    return surface == Surface::Water ? .55 : 1;
}

} // namespace OneKeyOut
