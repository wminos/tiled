#pragma once

#include "leveldata.h"
#include "map.h"
#include <QPixmap>
#include <memory>

namespace OneKeyOut {

struct PropSprite {
    QPixmap image;
    QPointF topLeft;
    qreal groundY = 0;
};

struct LoadedLevel {
    std::unique_ptr<Tiled::Map> map;
    LevelData data;
    QVector<PropSprite> props;
    QVector<PropSprite> caveSprites; // Projected art; no additional collision footprint.
};

// libtiled owns format decoding; this adapter validates the game's supported subset.
std::unique_ptr<LoadedLevel> loadLevel(const QString &path, QString &error);

} // namespace OneKeyOut
