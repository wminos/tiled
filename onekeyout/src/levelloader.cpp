#include "levelloader.h"
#include "gameworld.h"
#include "collision.h"
#include "mapreader.h"
#include "mapobject.h"
#include "objectgroup.h"
#include "tilelayer.h"
#include "tileset.h"
#include "tile.h"

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
    QVector<PropSprite> props;
    data.width = map->width();
    data.height = map->height();
    data.walls.fill(false, data.width * data.height);
    data.waterCorners.fill(0, data.width * data.height);
    data.boardwalkEdges.fill(-1, data.width * data.height);
    data.title = map->property(QStringLiteral("title")).toString();
    if (data.title.isEmpty())
        data.title = QStringLiteral("THE LANTERN VAULT");
    QSet<QString> layerNames;
    QSet<QString> objects;
    for (auto *layer : map->layers()) {
        const QString name = layer->name();
        if (layerNames.contains(name) ||
            (name != QStringLiteral("Ground") && name != QStringLiteral("Walls") &&
             name != QStringLiteral("Objects") && name != QStringLiteral("Props") &&
             name != QStringLiteral("Water") && name != QStringLiteral("Boardwalk") &&
             name != QStringLiteral("Foreground")))
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
                    if ((name == QStringLiteral("Water") || name == QStringLiteral("Boardwalk")) && !cell.isEmpty()) {
                        const auto *tile = cell.tile();
                        const bool water = name == QStringLiteral("Water");
                        const auto property = water ? QStringLiteral("waterCorners") : QStringLiteral("boardwalkEdges");
                        const auto value = tile->property(property);
                        if (cell.flags() != 0 || tile->offset() != QPoint() || tile->isAnimated() ||
                            layer->effectiveTintColor() != QColor(Qt::white) ||
                            value.metaType().id() != QMetaType::Int || value.toInt() < 0 || value.toInt() > 15)
                            return fail(name + QStringLiteral(" needs static, untransformed tiles with integer ") +
                                        property + QStringLiteral(" from 0 to 15 and no tint or offset."));
                        (water ? data.waterCorners : data.boardwalkEdges)[y * data.width + x] = value.toInt();
                    }
                    if (name == QStringLiteral("Props") && !cell.isEmpty()) {
                        const auto *tile = cell.tile();
                        if (cell.flags() != 0 || tile->offset() != QPoint() || tile->isAnimated() ||
                            layer->effectiveTintColor() != QColor(Qt::white))
                            return fail(QStringLiteral("Props need unflipped, static tiles without offsets or tint."));
                        const QPointF origin(x * data.tileSize, y * data.tileSize);
                        bool validSortY = true;
                        const auto value = tile->property(QStringLiteral("sortY"));
                        const qreal sortY = value.isValid() ? value.toDouble(&validSortY) : data.tileSize;
                        if (!validSortY || !std::isfinite(sortY) || sortY < 0 || sortY > data.tileSize)
                            return fail(QStringLiteral("Prop sortY must be a tile-local number from 0 to 32."));
                        props.append({tile->image().copy(tile->imageRect()), origin, origin.y() + sortY});
                        if (const auto *shapes = tile->objectGroup()) {
                            if (shapes->offset() != QPointF() || shapes->x() != 0 || shapes->y() != 0)
                                return fail(QStringLiteral("Prop collision groups must have zero offset."));
                            for (const auto *shape : shapes->objects()) {
                                const QRectF bounds(shape->position(), shape->size());
                                if (shape->shape() != Tiled::MapObject::Rectangle || shape->rotation() != 0 ||
                                    !shape->cell().isEmpty() || !std::isfinite(bounds.x()) ||
                                    !std::isfinite(bounds.y()) || !std::isfinite(bounds.width()) ||
                                    !std::isfinite(bounds.height()) || bounds.width() <= 0 || bounds.height() <= 0 ||
                                    !QRectF(0, 0, data.tileSize, data.tileSize).contains(bounds))
                                    return fail(QStringLiteral("Prop collision shapes must be unrotated rectangles inside the tile."));
                                data.propColliders.append(bounds.translated(origin));
                            }
                        }
                    }
                }
            }
        }
    }
    if (!layerNames.contains(QStringLiteral("Ground")) || !layerNames.contains(QStringLiteral("Walls")) ||
        !layerNames.contains(QStringLiteral("Objects")) || objects.size() != 3)
        return fail(QStringLiteral("Ground, Walls, Objects and one PlayerSpawn, Key, Exit are required."));
    GameWorld world;
    world.reset(data);
    if (!hasTerrainClearance(data, GameWorld::footprintAt(data.spawn)) ||
        !hasTerrainClearance(data, GameWorld::footprintAt(data.key.center())) ||
        !hasTerrainClearance(data, GameWorld::footprintAt(data.exit.center())))
        return fail(QStringLiteral("Spawn, key and exit must have player-sized clearance."));
    if (data.key.intersects(data.exit) || world.playerBounds().intersects(data.key) ||
        world.playerBounds().intersects(data.exit))
        return fail(QStringLiteral("Spawn, key and exit must not overlap."));

    // Validate the same footprint and swept paths used by movement, first with
    // the gate closed, then open. Content still promises cell-center routes.
    const auto reachable = [&](QPointF from, QPointF target, const QRectF &gate) {
        const auto center = [&](QPoint cell) {
            return QPointF((cell.x() + 0.5) * data.tileSize, (cell.y() + 0.5) * data.tileSize);
        };
        const auto cellAt = [&](QPointF point) {
            return QPoint(int(point.x()) / data.tileSize, int(point.y()) / data.tileSize);
        };
        const auto clearPath = [&](QPointF a, QPointF b) {
            const auto body = GameWorld::footprintAt(a);
            return hasTerrainClearance(data, body) && !body.intersects(gate) &&
                   slideMove(data, body, b - a, gate) == b - a;
        };
        const QPoint start = cellAt(from), goal = cellAt(target);
        if (!clearPath(from, center(start)))
            return false;
        QSet<int> visited;
        QQueue<QPoint> queue;
        queue.enqueue(start);
        visited.insert(start.y() * data.width + start.x());
        const QPoint directions[]{{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        while (!queue.isEmpty()) {
            const QPoint cell = queue.dequeue();
            if (cell == goal && clearPath(center(cell), target))
                return true;
            for (const QPoint direction : directions) {
                const QPoint next = cell + direction;
                const int index = next.y() * data.width + next.x();
                if (!data.blocked(next.x(), next.y()) && !visited.contains(index) &&
                    clearPath(center(cell), center(next))) {
                    visited.insert(index);
                    queue.enqueue(next);
                }
            }
        }
        return false;
    };
    if (!reachable(data.spawn, data.key.center(), data.exit) ||
        !reachable(data.key.center(), data.exit.center(), {}))
        return fail(QStringLiteral("Need a clear route to the key with the gate closed, then to the exit."));
    auto result = std::make_unique<LoadedLevel>();
    result->map = std::move(map);
    result->data = std::move(data);
    result->props = std::move(props);
    return result;
}

} // namespace OneKeyOut
