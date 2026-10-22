#pragma once

#include "leveldata.h"
#include "map.h"
#include <memory>

namespace OneKeyOut {

struct LoadedLevel {
    std::unique_ptr<Tiled::Map> map;
    LevelData data;
};

// libtiled owns format decoding; this adapter validates the game's supported subset.
std::unique_ptr<LoadedLevel> loadLevel(const QString &path, QString &error);

} // namespace OneKeyOut
