#include "collision.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace OneKeyOut {
namespace {
template<typename Visit>
void visitTerrain(const LevelData &level, const QRectF &area, Visit visit)
{
    const QRectF clipped = area.intersected(level.bounds());
    if (clipped.isEmpty())
        return;
    // Treat right/bottom as exclusive without allowing an epsilon of penetration.
    const auto lastCell = [&](qreal value) {
        return int(std::floor(std::nextafter(value, -std::numeric_limits<qreal>::infinity()) / level.tileSize));
    };
    const int left = int(std::floor(clipped.left() / level.tileSize));
    const int top = int(std::floor(clipped.top() / level.tileSize));
    for (int y = top; y <= lastCell(clipped.bottom()); ++y)
        for (int x = left; x <= lastCell(clipped.right()); ++x)
            if (level.blocked(x, y))
                visit(QRectF(x * level.tileSize, y * level.tileSize, level.tileSize, level.tileSize));
    for (const QRectF &obstacle : level.propColliders)
        if (obstacle.intersects(area))
            visit(obstacle);
}

bool validBody(const QRectF &body)
{
    return std::isfinite(body.x()) && std::isfinite(body.y()) &&
           std::isfinite(body.width()) && std::isfinite(body.height()) &&
           body.width() > 0 && body.height() > 0;
}
}

bool hasTerrainClearance(const LevelData &level, const QRectF &body)
{
    if (!validBody(body) || !level.bounds().contains(body))
        return false;
    bool clear = true;
    visitTerrain(level, body, [&](const QRectF &obstacle) {
        if (body.intersects(obstacle))
            clear = false;
    });
    return clear;
}

QPointF slideMove(const LevelData &level, QRectF body, QPointF displacement,
                  const QRectF &extraSolid)
{
    if (!hasTerrainClearance(level, body) || body.intersects(extraSolid) ||
        !std::isfinite(displacement.x()) || !std::isfinite(displacement.y()))
        return {};
    const auto sweep = [&](qreal distance, bool horizontal) {
        const qreal near = horizontal ? body.left() : body.top();
        const qreal far = horizontal ? body.right() : body.bottom();
        const qreal limit = horizontal ? level.bounds().right() : level.bounds().bottom();
        qreal allowed = std::clamp(distance, -near, limit - far);
        const QRectF swept = body.united(body.translated(horizontal ? allowed : 0,
                                                        horizontal ? 0 : allowed));
        const auto constrain = [&](const QRectF &obstacle) {
            const bool overlap = horizontal
                ? body.top() < obstacle.bottom() && body.bottom() > obstacle.top()
                : body.left() < obstacle.right() && body.right() > obstacle.left();
            if (!overlap)
                return;
            const qreal obstacleNear = horizontal ? obstacle.left() : obstacle.top();
            const qreal obstacleFar = horizontal ? obstacle.right() : obstacle.bottom();
            if (distance > 0 && far <= obstacleNear)
                allowed = std::min(allowed, obstacleNear - far);
            else if (distance < 0 && near >= obstacleFar)
                allowed = std::max(allowed, obstacleFar - near);
        };
        visitTerrain(level, swept, constrain);
        if (!extraSolid.isEmpty())
            constrain(extraSolid);
        body.translate(horizontal ? allowed : 0, horizontal ? 0 : allowed);
        return allowed;
    };
    const qreal x = sweep(displacement.x(), true);
    const qreal y = sweep(displacement.y(), false);
    return {x, y};
}

} // namespace OneKeyOut
