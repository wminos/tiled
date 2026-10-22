#include "levelloader.h"
#include "gameworld.h"
#include "mapreader.h"
#include "mapobject.h"
#include "objectgroup.h"
#include "tilelayer.h"
#include "tileset.h"

#include <QQueue>
#include <QSet>
#include <cmath>

namespace OneKeyOut {

std::unique_ptr<LoadedLevel> loadLevel(const QString &path, QString &error)
{
    error.clear();
    auto fail = [&](const QString &reason) -> std::unique_ptr<LoadedLevel> {
        error = path + QStringLiteral(": ") + reason;
        return {};
    };
    Tiled::MapReader reader;
    auto map = reader.readMap(path);
    if (!map)
        return fail(reader.errorString());
    if (map->orientation() != Tiled::Map::Orthogonal || map->infinite())
        return fail(QStringLiteral("Only finite orthogonal maps are supported."));
    if (map->tileWidth() != 32 || map->tileHeight() != 32 ||
        map->width() < 3 || map->height() < 3 || map->width() > 64 || map->height() > 64)
        return fail(QStringLiteral("Use 32 px tiles and a map between 3x3 and 64x64 cells."));
    for (const auto &tileset : map->tilesets()) {
        if (tileset->image().isNull() || tileset->tileWidth() != 32 || tileset->tileHeight() != 32)
            return fail(QStringLiteral("Each tileset needs a readable 32 px atlas image: ") + tileset->fileName());
    }

    LevelData data;
    data.width = map->width();
    data.height = map->height();
    data.walls.fill(false, data.width * data.height);
    data.title = map->property(QStringLiteral("title")).toString();
    if (data.title.isEmpty())
        data.title = QStringLiteral("THE LANTERN VAULT");
    QSet<QString> layerNames;
    QSet<QString> objects;
    for (auto *layer : map->layers()) {
        const QString name = layer->name();
        if (layerNames.contains(name) ||
            (name != QStringLiteral("Ground") && name != QStringLiteral("Walls") &&
             name != QStringLiteral("Objects") && name != QStringLiteral("Foreground")))
            return fail(QStringLiteral("Unknown or duplicate layer: ") + name);
        layerNames.insert(name);
        if (layer->offset() != QPointF() || layer->x() != 0 || layer->y() != 0 ||
            !layer->isVisible() || layer->opacity() != 1)
            return fail(QStringLiteral("Layers must be visible, opaque and have zero offset: ") + name);
        if (name == QStringLiteral("Objects")) {
            const auto *group = layer->asObjectGroup();
            if (!group)
                return fail(QStringLiteral("Objects must be an object layer."));
            for (const auto *object : group->objects()) {
                const QString kind = object->className();
                if ((kind != QStringLiteral("PlayerSpawn") && kind != QStringLiteral("Key") &&
                     kind != QStringLiteral("Exit")) || objects.contains(kind))
                    return fail(QStringLiteral("Expected one PlayerSpawn, Key and Exit; unexpected class: ") + kind);
                if (!object->isVisible() || object->rotation() != 0 || !object->cell().isEmpty() ||
                    !std::isfinite(object->x()) || !std::isfinite(object->y()))
                    return fail(QStringLiteral("Use visible, unrotated geometric objects: ") + kind);
                objects.insert(kind);
                if (kind == QStringLiteral("PlayerSpawn")) {
                    if (object->shape() != Tiled::MapObject::Point)
                        return fail(QStringLiteral("PlayerSpawn must be a point object."));
                    data.spawn = object->position();
                } else {
                    const QRectF bounds(object->position(), object->size());
                    if (object->shape() != Tiled::MapObject::Rectangle ||
                        !std::isfinite(bounds.width()) || !std::isfinite(bounds.height()) ||
                        bounds.width() <= 0 || bounds.height() <= 0 || !data.bounds().contains(bounds))
                        return fail(QStringLiteral("Key and Exit must be rectangles inside the map."));
                    if (kind == QStringLiteral("Key"))
                        data.key = bounds;
                    else
                        data.exit = bounds;
                }
            }
        } else {
            const auto *tiles = layer->asTileLayer();
            if (!tiles || tiles->width() != data.width || tiles->height() != data.height)
                return fail(QStringLiteral("Tile layer size must match the map: ") + name);
            for (int y = 0; y < data.height; ++y) {
                for (int x = 0; x < data.width; ++x) {
                    const auto &cell = tiles->cellAt(x, y);
                    if (!cell.isEmpty() && !cell.tile())
                        return fail(QStringLiteral("Tile ID outside the tileset in ") + name);
                    if (name == QStringLiteral("Walls"))
                        data.walls[y * data.width + x] = !cell.isEmpty();
                }
            }
        }
    }
    if (!layerNames.contains(QStringLiteral("Ground")) || !layerNames.contains(QStringLiteral("Walls")) ||
        !layerNames.contains(QStringLiteral("Objects")) || objects.size() != 3)
        return fail(QStringLiteral("Ground, Walls, Objects and one PlayerSpawn, Key, Exit are required."));
    GameWorld world;
    world.reset(data);
    if (!world.canOccupy(data.spawn) || !world.canOccupy(data.key.center()) ||
        !world.canOccupy(data.exit.center()))
        return fail(QStringLiteral("Spawn, key and exit must have player-sized clearance."));
    if (data.key.intersects(data.exit) || world.playerBounds().intersects(data.key) ||
        world.playerBounds().intersects(data.exit))
        return fail(QStringLiteral("Spawn, key and exit must not overlap."));

    // Cell-center reachability is the supported level contract (no sub-cell passages).
    QSet<int> visited;
    QQueue<QPoint> queue;
    const QPoint start(int(data.spawn.x()) / 32, int(data.spawn.y()) / 32);
    queue.enqueue(start);
    visited.insert(start.y() * data.width + start.x());
    const QPoint directions[]{{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    while (!queue.isEmpty()) {
        const QPoint cell = queue.dequeue();
        for (const QPoint direction : directions) {
            const QPoint next = cell + direction;
            const int index = next.y() * data.width + next.x();
            if (!data.blocked(next.x(), next.y()) && !visited.contains(index)) {
                visited.insert(index);
                queue.enqueue(next);
            }
        }
    }
    for (const QPointF target : {data.key.center(), data.exit.center()})
        if (!visited.contains(int(target.y()) / 32 * data.width + int(target.x()) / 32))
            return fail(QStringLiteral("Key and exit must be reachable from PlayerSpawn."));
    auto result = std::make_unique<LoadedLevel>();
    result->map = std::move(map);
    result->data = std::move(data);
    return result;
}

} // namespace OneKeyOut
