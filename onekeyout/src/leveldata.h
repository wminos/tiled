#pragma once

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

namespace OneKeyOut {

struct LevelData {
    int width = 0;
    int height = 0;
    int tileSize = 32;
    QVector<bool> walls;
    QVector<QRectF> propColliders;
    // Explicit gameplay masks, independent of tile IDs and editor Wang metadata.
    QVector<int> waterCorners;    // TR, BR, BL, TL bits; empty means all dry.
    QVector<int> boardwalkEdges;  // N, E, S, W bits; -1 means no boardwalk.
    QPointF spawn;
    QRectF key;
    QRectF exit;
    QString title;

    bool blocked(int x, int y) const
    {
        return x < 0 || y < 0 || x >= width || y >= height || walls[y * width + x];
    }
    QRectF bounds() const { return {0, 0, qreal(width * tileSize), qreal(height * tileSize)}; }
};

} // namespace OneKeyOut
