#include "gamewindow.h"
#include "gameview.h"
#include "gameworld.h"
#include "collision.h"
#include "levelloader.h"
#include "mapwriter.h"
#include "tilelayer.h"
#include "wangset.h"
#include <QtTest>
#include <QDir>
#include <QQueue>
#include <QTemporaryDir>
#include <QPainter>
#include <cmath>
#include <limits>

using namespace OneKeyOut;

class RenderProbe : public GameView {
public:
    using GameView::GameView;
    QImage renderWorld()
    {
        QImage image(sceneRect().size().toSize(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        drawBackground(&painter, sceneRect());
        drawForeground(&painter, sceneRect());
        return image;
    }
};

class GameTest : public QObject {
    Q_OBJECT
private:
    QString fixture() const { return QFINDTESTDATA("fixtures/valid-room.tmx"); }
    LevelData simpleLevel() const
    {
        LevelData data;
        data.width = 8;
        data.height = 6;
        data.walls.fill(false, 48);
        for (int y = 0; y < data.height; ++y)
            for (int x = 0; x < data.width; ++x)
                if (x == 0 || y == 0 || x == 7 || y == 5)
                    data.walls[y * data.width + x] = true;
        data.spawn = {80, 80};
        data.key = {134, 70, 20, 20};
        data.exit = {194, 66, 28, 28};
        return data;
    }

    QString rewrittenMap(QTemporaryDir &dir, const QByteArray &from, const QByteArray &to,
                         QString tileset = {})
    {
        QFile source(fixture());
        if (!source.open(QIODevice::ReadOnly))
            return {};
        QByteArray text = source.readAll();
        if (tileset.isEmpty())
            tileset = QDir(QFileInfo(fixture()).absolutePath()).absoluteFilePath(
                QStringLiteral("../../assets/tilesets/dungeon.tsx"));
        text.replace("../../assets/tilesets/dungeon.tsx", tileset.toUtf8());
        text.replace(from, to);
        const QString target = dir.filePath(QStringLiteral("map.tmx"));
        QFile output(target);
        if (!output.open(QIODevice::WriteOnly) || output.write(text) != text.size())
            return {};
        return target;
    }

    void capture(GameWindow &window, const QString &name)
    {
        const QString directory = qEnvironmentVariable("ONEKEYOUT_CAPTURE_DIR");
        if (directory.isEmpty())
            return;
        QVERIFY(QDir().mkpath(directory));
        QVERIFY(window.grab().save(QDir(directory).filePath(name + QStringLiteral(".png")), "PNG"));
    }

    void walkTo(GameWindow &window, QPointF target)
    {
        const auto &data = window.world().level();
        const QPoint start(int(window.world().position().x()) / 32, int(window.world().position().y()) / 32);
        const QPoint goal(int(target.x()) / 32, int(target.y()) / 32);
        QVector<int> parents(data.width * data.height, -1);
        QQueue<QPoint> queue;
        queue.enqueue(start);
        const int startIndex = start.y() * data.width + start.x();
        parents[startIndex] = startIndex;
        const QPoint offsets[]{{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        while (!queue.isEmpty()) {
            const auto current = queue.dequeue();
            for (const auto offset : offsets) {
                const auto next = current + offset;
                if (data.blocked(next.x(), next.y()))
                    continue;
                const QPointF from((current.x() + 0.5) * 32, (current.y() + 0.5) * 32);
                const QPointF to((next.x() + 0.5) * 32, (next.y() + 0.5) * 32);
                if (slideMove(data, GameWorld::footprintAt(from), to - from,
                              window.world().hasKey() ? QRectF() : data.exit) != to - from)
                    continue;
                const int index = next.y() * data.width + next.x();
                if (parents[index] < 0) {
                    parents[index] = current.y() * data.width + current.x();
                    queue.enqueue(next);
                }
            }
        }
        int index = goal.y() * data.width + goal.x();
        QVERIFY(parents[index] >= 0);
        QList<QPointF> path;
        while (index != startIndex) {
            const QPoint cell(index % data.width, index / data.width);
            path.prepend(QPointF(cell.x() * 32 + 16, cell.y() * 32 + 16));
            index = parents[index];
        }
        path.prepend(QPointF(start.x() * 32 + 16, start.y() * 32 + 16));
        for (const auto point : path) {
            for (int tick = 0; tick < 100; ++tick) {
                const QPointF delta = point - window.world().position();
                if (std::hypot(delta.x(), delta.y()) < 2.5 || window.world().state() == GameWorld::State::Won)
                    break;
                const Qt::Key key = std::abs(delta.x()) > std::abs(delta.y())
                    ? (delta.x() > 0 ? Qt::Key_Right : Qt::Key_Left)
                    : (delta.y() > 0 ? Qt::Key_Down : Qt::Key_Up);
                QTest::keyPress(&window, key);
                window.advance(1.0 / 60.0);
                QTest::keyRelease(&window, key);
                QVERIFY2(tick < 99, "Route stalled before its next cell.");
            }
        }
    }

private slots:
    void readsRealTmxAndEmbeddedCopy()
    {
        QString error;
        auto disk = loadLevel(QFINDTESTDATA("../assets/maps/first-room.tmx"), error);
        QVERIFY2(disk, qPrintable(error));
        auto embedded = loadLevel(QStringLiteral(":/onekeyout/assets/maps/first-room.tmx"), error);
        QVERIFY2(embedded, qPrintable(error));
        QCOMPARE(disk->data.walls, embedded->data.walls);
        QCOMPARE(disk->data.spawn, embedded->data.spawn);
        QCOMPARE(disk->data.key, embedded->data.key);
        QCOMPARE(disk->data.exit, embedded->data.exit);
        QCOMPARE(disk->data.propColliders, embedded->data.propColliders);
        QCOMPARE(disk->data.waterCorners, embedded->data.waterCorners);
        QCOMPARE(disk->data.boardwalkEdges, embedded->data.boardwalkEdges);
        QCOMPARE(disk->props.size(), 8);
        QCOMPARE(disk->props.size(), embedded->props.size());
        QCOMPARE(disk->data.propColliders.size(), 8);
        QCOMPARE(disk->props.first().groundY, 60.0);
        QCOMPARE(disk->data.propColliders.first(), QRectF(75, 53, 11, 7));
        for (int i = 0; i < disk->props.size(); ++i) {
            QCOMPARE(disk->props[i].groundY, embedded->props[i].groundY);
            QCOMPARE(disk->props[i].image.toImage(), embedded->props[i].image.toImage());
        }
    }

    void rejectsUnsupportedOrBrokenContent_data()
    {
        QTest::addColumn<QByteArray>("from");
        QTest::addColumn<QByteArray>("to");
        QTest::newRow("infinite") << QByteArray("infinite=\"0\"") << QByteArray("infinite=\"1\"");
        QTest::newRow("missing-spawn") << QByteArray("type=\"PlayerSpawn\"") << QByteArray("type=\"Unknown\"");
        QTest::newRow("offset") << QByteArray("name=\"Walls\"") << QByteArray("name=\"Walls\" offsetx=\"2\"");
        QTest::newRow("hidden-wall") << QByteArray("name=\"Walls\"") << QByteArray("name=\"Walls\" visible=\"0\"");
        QTest::newRow("broken-atlas") << QByteArray("dungeon.tsx") << QByteArray("missing.tsx");
        QTest::newRow("duplicate-key") << QByteArray("type=\"Exit\"") << QByteArray("type=\"Key\"");
        QTest::newRow("spawn-in-wall") << QByteArray("type=\"PlayerSpawn\" x=\"112\" y=\"80\"")
                                      << QByteArray("type=\"PlayerSpawn\" x=\"16\" y=\"16\"");
        QTest::newRow("flipped-prop") << QByteArray("0,0,7,0,0,8") << QByteArray("0,0,2147483655,0,0,8");
    }

    void rejectsUnsupportedOrBrokenContent()
    {
        QFETCH(QByteArray, from);
        QFETCH(QByteArray, to);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = rewrittenMap(dir, from, to);
        QVERIFY(!path.isEmpty());
        QString error;
        QVERIFY(!loadLevel(path, error));
        QVERIFY(!error.isEmpty());
    }

    void collisionDiagonalAndLongFrame()
    {
        GameWorld world;
        auto data = simpleLevel();
        data.walls[2 * data.width + 3] = true;
        world.reset(data);
        for (int i = 0; i < 120; ++i)
            world.step({1, 0}, 1.0 / 60);
        QCOMPARE(world.position().x(), 96 - GameWorld::PlayerHalfWidth);
        QVERIFY(world.canOccupy(world.position()));
        world.restart();
        world.step({1, 0}, 60);
        QCOMPARE(world.position().x(), 96 - GameWorld::PlayerHalfWidth);
        data.walls[2 * data.width + 3] = false;
        world.reset(data);
        world.step({1, 1}, 1.0 / 60);
        const auto displacement = world.position() - data.spawn;
        QVERIFY(std::abs(std::hypot(displacement.x(), displacement.y()) - GameWorld::Speed / 60) < 0.00001);
    }

    void rejectsInvalidPropMetadata_data()
    {
        QTest::addColumn<QByteArray>("from");
        QTest::addColumn<QByteArray>("to");
        QTest::newRow("sort-outside-tile") << QByteArray("value=\"28\"") << QByteArray("value=\"33\"");
        QTest::newRow("sort-not-finite") << QByteArray("value=\"28\"") << QByteArray("value=\"nan\"");
        QTest::newRow("collision-outside-tile") << QByteArray("width=\"11\"") << QByteArray("width=\"33\"");
        QTest::newRow("rotated-collision") << QByteArray("<object id=\"1\"")
                                           << QByteArray("<object id=\"1\" rotation=\"10\"");
        QTest::newRow("ellipse-collision") << QByteArray("height=\"7\"/>")
                                           << QByteArray("height=\"7\"><ellipse/></object>");
        QTest::newRow("collision-group-offset") << QByteArray("<objectgroup draworder=\"index\"")
                                                << QByteArray("<objectgroup offsetx=\"1\" draworder=\"index\"");
    }

    void rejectsInvalidPropMetadata()
    {
        QFETCH(QByteArray, from);
        QFETCH(QByteArray, to);
        QFile source(QFINDTESTDATA("../assets/tilesets/dungeon.tsx"));
        QVERIFY(source.open(QIODevice::ReadOnly));
        auto text = source.readAll();
        QVERIFY(text.contains(from));
        text.replace(from, to);
        text.replace("../images/dungeon.png", QFINDTESTDATA("../assets/images/dungeon.png").toUtf8());
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile tileset(directory.filePath("invalid.tsx"));
        QVERIFY(tileset.open(QIODevice::WriteOnly));
        QCOMPARE(tileset.write(text), qint64(text.size()));
        tileset.close();
        const QString map = rewrittenMap(directory, {}, {}, tileset.fileName());
        QString error;
        QVERIFY(!loadLevel(map, error));
        QVERIFY2(error.contains("Prop"), qPrintable(error));
    }

    void lockedGatePickupWinAndRestart()
    {
        GameWorld world;
        auto data = simpleLevel();
        data.spawn = {170, 80};
        world.reset(data);
        for (int i = 0; i < 80; ++i)
            world.step({1, 0}, 1.0 / 60);
        QCOMPARE(world.position().x(), data.exit.left() - GameWorld::PlayerHalfWidth);
        QVERIFY(!world.canOccupy(data.exit.center()));
        QCOMPARE(world.state(), GameWorld::State::Playing);
        data = simpleLevel();
        world.reset(data);
        for (int i = 0; i < 80; ++i)
            world.step({1, 0}, 1.0 / 60);
        QVERIFY(world.hasKey());
        QVERIFY(world.canOccupy(data.exit.center()));
        QCOMPARE(world.state(), GameWorld::State::Won);
        const auto position = world.position();
        world.step({-1, 0}, 0.1);
        QCOMPARE(world.position(), position);
        world.restart();
        QCOMPARE(world.position(), data.spawn);
        QVERIFY(!world.hasKey());
        QCOMPARE(world.state(), GameWorld::State::Playing);
        QCOMPARE(world.elapsed(), 0.0);
        QVERIFY(!world.canOccupy(data.exit.center()));
    }

    void rejectsKeyBehindClosedGate()
    {
        const QString sourcePath = QFINDTESTDATA("fixtures/gate-corridor.tmx");
        QString error;
        QVERIFY(!loadLevel(sourcePath, error));
        QVERIFY2(error.contains("gate closed"), qPrintable(error));
        QFile source(sourcePath);
        QVERIFY(source.open(QIODevice::ReadOnly));
        auto text = source.readAll();
        text.replace("../../assets/tilesets/dungeon.tsx",
                     QFINDTESTDATA("../assets/tilesets/dungeon.tsx").toUtf8());
        // The same corridor is playable when the key comes before the gate.
        text.replace("type=\"Key\" x=\"134\"", "type=\"Key\" x=\"70\"");
        text.replace("type=\"Exit\" x=\"66\"", "type=\"Exit\" x=\"130\"");
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile target(directory.filePath("valid-corridor.tmx"));
        QVERIFY(target.open(QIODevice::WriteOnly));
        QCOMPARE(target.write(text), qint64(text.size()));
        target.close();
        QVERIFY2(loadLevel(target.fileName(), error), qPrintable(error));
    }

    void exactContactOnAllWallFaces_data()
    {
        QTest::addColumn<QPointF>("start");
        QTest::addColumn<QPointF>("direction");
        QTest::addColumn<QPointF>("contact");
        QTest::newRow("left") << QPointF(80, 80) << QPointF(1, 0) << QPointF(87, 80);
        QTest::newRow("right") << QPointF(144, 80) << QPointF(-1, 0) << QPointF(137, 80);
        QTest::newRow("above") << QPointF(112, 48) << QPointF(0, 1) << QPointF(112, 59);
        QTest::newRow("below") << QPointF(112, 112) << QPointF(0, -1) << QPointF(112, 101);
    }

    void exactContactOnAllWallFaces()
    {
        QFETCH(QPointF, start);
        QFETCH(QPointF, direction);
        QFETCH(QPointF, contact);
        auto data = simpleLevel();
        data.spawn = start;
        data.key = {40, 128, 8, 8};
        data.exit = {194, 128, 20, 20};
        data.walls[2 * data.width + 3] = true;
        GameWorld world;
        world.reset(data);
        for (int i = 0; i < 60; ++i) {
            world.step(direction, 1.0 / 60);
            QVERIFY(world.canOccupy(world.position()));
        }
        QCOMPARE(world.position(), contact);
        QVERIFY(!world.canOccupy(contact + direction * 0.00001));
        world.step(-direction, 1.0 / 60);
        QVERIFY(world.position() != contact); // Leaving a touching edge must not stick.
    }

    void slidesAlongWallsAndStopsAtCorners()
    {
        auto data = simpleLevel();
        data.walls[2 * data.width + 3] = true;
        data.walls[3 * data.width + 2] = true;
        data.spawn = {87, 80};
        GameWorld world;
        world.reset(data);
        world.step({1, 1}, 1.0 / 60);
        QCOMPARE(world.position().x(), 87.0);
        QVERIFY(world.position().y() > 80);
        for (int i = 0; i < 120; ++i) {
            world.step({1, 1}, 1.0 / 60);
            QVERIFY(world.canOccupy(world.position()));
        }
        QCOMPARE(world.position(), QPointF(87, 91));
        // The opposite orientation must slide on X when Y is blocked.
        data.spawn = {80, 91};
        world.reset(data);
        world.step({1, 1}, 1.0 / 60);
        QVERIFY(world.position().x() > 80);
        QCOMPARE(world.position().y(), 91.0);
    }

    void thinPropsBoundariesAndInvalidInputs()
    {
        auto data = simpleLevel();
        data.walls.fill(false);
        const QRectF body = GameWorld::footprintAt(data.spawn);
        QCOMPARE(slideMove(data, body, {-1000, 0}), QPointF(9 - 80, 0));
        QCOMPARE(slideMove(data, body, {1000, 0}), QPointF(256 - 9 - 80, 0));
        QCOMPARE(slideMove(data, body, {0, -1000}), QPointF(0, 5 - 80));
        QCOMPARE(slideMove(data, body, {0, 1000}), QPointF(0, 192 - 5 - 80));
        data.propColliders.append(QRectF(100.25, 64, 0.5, 32));
        QCOMPARE(slideMove(data, body, {1000, 0}), QPointF(11.25, 0));
        QCOMPARE(slideMove(data, GameWorld::footprintAt({140, 80}), {-1000, 0}), QPointF(-30.25, 0));
        GameWorld world;
        world.reset(data);
        for (int i = 0; i < 60; ++i)
            world.step({1, 0}, 1.0 / 60);
        QCOMPARE(world.position(), QPointF(91.25, 80));
        QVERIFY(world.canOccupy(world.position()));
        QVERIFY(!world.canOccupy({91.25001, 80}));
        const auto position = world.position();
        const auto nan = std::numeric_limits<qreal>::quiet_NaN();
        const auto inf = std::numeric_limits<qreal>::infinity();
        QVERIFY(!world.canOccupy({nan, 80}));
        QVERIFY(!world.canOccupy({80, inf}));
        world.step({inf, 1}, 0.1);
        world.step({-1, 0}, nan);
        QCOMPARE(world.position(), position);
    }

    void ySortedPixelsAndForeground()
    {
        QString error;
        auto level = loadLevel(fixture(), error);
        QVERIFY2(level, qPrintable(error));
        const QColor marker(230, 30, 190);
        QPixmap prop(32, 32);
        prop.fill(marker);
        level->props = {{prop, {64, 32}, 64}};
        GameWorld world;
        RenderProbe view(world);
        view.setLevel(level.get());
        const auto renderAt = [&](qreal y) {
            auto data = level->data;
            data.spawn = {80, y};
            world.reset(data);
            return view.renderWorld();
        };
        const auto behind = renderAt(62);
        const auto inFront = renderAt(66);
        QCOMPARE(behind.pixelColor(80, 45), marker);
        QVERIFY(inFront.pixelColor(80, 45) != marker);
        QVERIFY(renderAt(64).pixelColor(80, 45) != marker); // Stable equal-Y tie.
        const QString directory = qEnvironmentVariable("ONEKEYOUT_CAPTURE_DIR");
        if (!directory.isEmpty()) {
            QVERIFY(behind.save(directory + "/06-sort-behind.png"));
            QVERIFY(inFront.save(directory + "/07-sort-front.png"));
        }
        auto foreground = std::make_unique<Tiled::TileLayer>(QStringLiteral("Foreground"), 0, 0,
                                                           level->data.width, level->data.height);
        foreground->setCell(2, 1, Tiled::Cell(level->map->tilesets().first().data(), 0));
        level->map->addLayer(std::move(foreground));
        QCOMPARE(renderAt(62).pixelColor(80, 45), renderAt(66).pixelColor(80, 45));
    }

    void gateAnchorAndKeyBobbingDepth()
    {
        QString error;
        auto level = loadLevel(fixture(), error);
        QVERIFY2(level, qPrintable(error));
        const QColor marker(230, 30, 190);
        QPixmap prop(32, 32);
        prop.fill(marker);
        GameWorld world;
        world.reset(level->data);
        RenderProbe view(world);
        view.setLevel(level.get());
        // Gate depth uses its base at y=94, not its trigger center at y=80.
        level->props = {{prop, {608, 64}, 90}};
        QVERIFY(view.renderWorld().pixelColor(624, 80) != marker);
        level->props[0].groundY = 95;
        QCOMPARE(view.renderWorld().pixelColor(624, 80), marker);
        // Key moves visually down by two pixels but stays behind ground Y=64.5.
        auto data = level->data;
        data.key = {70, 54, 20, 20};
        world.reset(data);
        level->data = data;
        level->props = {{prop, {64, 32}, 64.5}};
        QCOMPARE(view.renderWorld().pixelColor(80, 56), marker);
        for (int i = 0; i < 5; ++i)
            world.step({}, 0.1);
        QCOMPARE(view.renderWorld().pixelColor(80, 56), marker);
    }

    void keyboardRoutePauseFocusAndReload()
    {
        GameWindow window;
        window.setAutomaticUpdates(false);
        QString error;
        QVERIFY2(window.load(fixture(), error), qPrintable(error));
        window.show();
        QCoreApplication::processEvents();
        QTest::keyClick(&window, Qt::Key_R);
        capture(window, QStringLiteral("01-start"));
        const auto start = window.world().position();
        QTest::keyPress(&window, Qt::Key_D);
        window.advance(0.1);
        QVERIFY(window.world().position().x() > start.x());
        QTest::keyClick(&window, Qt::Key_Escape);
        QCOMPARE(window.world().state(), GameWorld::State::Paused);
        const auto paused = window.world().position();
        window.advance(0.1);
        QCOMPARE(window.world().position(), paused);
        capture(window, QStringLiteral("02-paused"));
        QTest::keyClick(&window, Qt::Key_Escape);
        window.advance(0.1);
        QCOMPARE(window.world().position(), paused); // pause clears held inputs
        QTest::keyPress(&window, Qt::Key_D);
        QVERIFY(QMetaObject::invokeMethod(qApp, "applicationStateChanged", Qt::DirectConnection,
                                         Q_ARG(Qt::ApplicationState, Qt::ApplicationInactive)));
        QCOMPARE(window.world().state(), GameWorld::State::Paused);
        QTest::keyClick(&window, Qt::Key_Escape);
        const auto unfocusedPosition = window.world().position();
        window.advance(0.1);
        QCOMPARE(window.world().position(), unfocusedPosition);
        QTest::keyClick(&window, Qt::Key_R);

        QVERIFY(!window.load(QStringLiteral("/nonexistent/onekeyout.tmx"), error));
        QCOMPARE(window.world().position(), start);
        walkTo(window, window.world().level().key.center());
        QVERIFY(window.world().hasKey());
        capture(window, QStringLiteral("03-key"));
        walkTo(window, window.world().level().exit.center());
        QCOMPARE(window.world().state(), GameWorld::State::Won);
        capture(window, QStringLiteral("04-complete"));
        window.resize(832, 700);
        QCoreApplication::processEvents();
        capture(window, QStringLiteral("05-small-window"));
        QTest::keyClick(&window, Qt::Key_R);
        QVERIFY(!window.world().hasKey());
        QCOMPARE(window.world().position(), start);
    }

    void terrainArtMatchesGameplayFootprint()
    {
        QString error;
        auto level = loadLevel(QStringLiteral(":/onekeyout/assets/maps/first-room.tmx"), error);
        QVERIFY2(level, qPrintable(error));
        LevelData probe;
        probe.width = probe.height = 1;
        probe.walls = {false};
        int checked = 0;
        for (const auto &tileset : level->map->tilesets()) {
            const bool water = tileset->name() == "water";
            if (!water && tileset->name() != "boardwalk")
                continue;
            QCOMPARE(tileset->tileCount(), 16);
            QCOMPARE(tileset->wangSets().size(), 1);
            const auto *wang = tileset->wangSets().first();
            QSet<int> masks;
            for (const auto *tile : tileset->tiles()) {
                const int mask = tile->property(water ? "waterCorners" : "boardwalkEdges").toInt();
                masks.insert(mask);
                // No dependence on atlas order: explicit properties must match Wang labels.
                const auto id = wang->wangIdOfTile(tile);
                for (int bit = 0; bit < 4; ++bit) {
                    QCOMPARE(id.indexColor(bit * 2 + (water ? 1 : 0)), (mask >> bit) & 1);
                    QCOMPARE(id.indexColor(bit * 2 + (water ? 0 : 1)), 0);
                }
                probe.waterCorners = {water ? mask : 15};
                probe.boardwalkEdges = {water ? -1 : mask};
                const QImage pixels = tile->image().copy(tile->imageRect()).toImage();
                for (int y = 0; y < 32; ++y)
                    for (int x = 0; x < 32; ++x) {
                        const auto surface = surfaceAt(probe, {x + .5, y + .5});
                        const bool covered = surface == (water ? Surface::Water : Surface::Boardwalk);
                        QCOMPARE(covered, pixels.pixelColor(x, y).alpha() != 0);
                    }
                ++checked;
            }
            QCOMPARE(masks.size(), 16);
        }
        QCOMPARE(checked, 32);
    }

    void authoredWangConnectionsHaveNoSeams()
    {
        QString error;
        auto level = loadLevel(QStringLiteral(":/onekeyout/assets/maps/first-room.tmx"), error);
        QVERIFY2(level, qPrintable(error));
        int checkedLayers = 0;
        for (auto *layer : level->map->layers()) {
            const bool water = layer->name() == "Water";
            if (!water && layer->name() != "Boardwalk")
                continue;
            const auto *tiles = layer->asTileLayer();
            QVERIFY(tiles);
            const auto at = [&](int x, int y) {
                if (x < 0 || y < 0 || x >= level->data.width || y >= level->data.height)
                    return Tiled::WangId();
                const auto cell = tiles->cellAt(x, y);
                return cell.isEmpty() ? Tiled::WangId() : cell.tileset()->wangSets().first()->wangIdOfCell(cell);
            };
            int occupied = 0;
            QSet<quint64> patterns;
            for (int y = -1; y < level->data.height; ++y)
                for (int x = -1; x < level->data.width; ++x) {
                    const auto here = at(x, y), right = at(x+1, y), below = at(x, y+1);
                    if (!here.isEmpty()) { ++occupied; patterns.insert(here); }
                    if (water) {
                        QCOMPARE(here.indexColor(1), right.indexColor(7));
                        QCOMPARE(here.indexColor(3), right.indexColor(5));
                        QCOMPARE(here.indexColor(3), below.indexColor(1));
                        QCOMPARE(here.indexColor(5), below.indexColor(7));
                    } else {
                        QCOMPARE(here.indexColor(2), right.indexColor(6));
                        QCOMPARE(here.indexColor(4), below.indexColor(0));
                    }
                }
            QVERIFY(occupied >= 30);
            QVERIFY(patterns.size() >= 6); // Edges, corners and interior, not one repeated tile.
            ++checkedLayers;
        }
        QCOMPARE(checkedLayers, 2);
    }

    void waterMovementBoardwalkAndPause()
    {
        auto data = simpleLevel();
        data.waterCorners.fill(15, data.width * data.height);
        data.boardwalkEdges.fill(-1, data.width * data.height);
        GameWorld world;
        world.reset(data);
        QCOMPARE(world.surface(), Surface::Water);
        world.step({1, 0}, .1);
        QVERIFY(qAbs(world.position().x() - (data.spawn.x() + 14.4 * .55)) < .00001);
        world.pause();
        const auto paused = world.position();
        const auto elapsed = world.elapsed();
        world.step({1, 0}, .1);
        QCOMPARE(world.position(), paused);
        QCOMPARE(world.elapsed(), elapsed);
        data.boardwalkEdges.fill(10); // Continuous east-west boardwalk.
        world.reset(data);
        QCOMPARE(world.surface(), Surface::Boardwalk);
        world.step({1, 0}, .1);
        QVERIFY(qAbs(world.position().x() - (data.spawn.x() + 14.4)) < .00001);
        QCOMPARE(surfaceAt(data, {80, 72}), Surface::Water); // Outside its visible width.
        QCOMPARE(surfaceAt(data, {80, 73}), Surface::Boardwalk);
        QCOMPARE(surfaceAt(data, {80, 87}), Surface::Water);
        world.restart();
        QCOMPARE(world.elapsed(), 0.0);
        QCOMPARE(world.surface(), Surface::Boardwalk);
    }

    void rejectsInvalidSurfaceMetadata_data()
    {
        QTest::addColumn<QByteArray>("from");
        QTest::addColumn<QByteArray>("to");
        QTest::newRow("missing-mask") << QByteArray("name=\"waterCorners\"") << QByteArray("name=\"unknown\"");
        QTest::newRow("negative-mask") << QByteArray("value=\"15\"") << QByteArray("value=\"-1\"");
        QTest::newRow("large-mask") << QByteArray("value=\"15\"") << QByteArray("value=\"16\"");
        QTest::newRow("fractional-mask") << QByteArray("type=\"int\" value=\"15\"") << QByteArray("type=\"float\" value=\"1.5\"");
    }

    void rejectsInvalidSurfaceMetadata()
    {
        QFETCH(QByteArray, from);
        QFETCH(QByteArray, to);
        const QString root = QFileInfo(QFINDTESTDATA("../assets/maps/first-room.tmx")).absolutePath();
        const QString tiles = QDir(root).absoluteFilePath("../tilesets/");
        QFile source(tiles + "water.tsx");
        QVERIFY(source.open(QIODevice::ReadOnly));
        QByteArray tsx = source.readAll();
        QVERIFY(tsx.contains(from));
        tsx.replace(from, to);
        tsx.replace("source=\"image-", ("source=\"" + tiles + "image-").toUtf8());
        QTemporaryDir directory;
        QFile output(directory.filePath("water.tsx"));
        QVERIFY(output.open(QIODevice::WriteOnly));
        QCOMPARE(output.write(tsx), qint64(tsx.size()));
        output.close();
        QFile mapSource(root + "/first-room.tmx");
        QVERIFY(mapSource.open(QIODevice::ReadOnly));
        auto tmx = mapSource.readAll();
        tmx.replace("../tilesets/water.tsx", output.fileName().toUtf8());
        tmx.replace("../tilesets/", tiles.toUtf8());
        QFile map(directory.filePath("invalid.tmx"));
        QVERIFY(map.open(QIODevice::WriteOnly));
        QCOMPARE(map.write(tmx), qint64(tmx.size()));
        map.close();
        QString error;
        QVERIFY(!loadLevel(map.fileName(), error));
        QVERIFY2(error.contains("Water needs"), qPrintable(error));
    }

    void floodedVaultKeyboardRoute()
    {
        GameWindow window;
        window.setAutomaticUpdates(false);
        QString error;
        QVERIFY2(window.load(QStringLiteral(":/onekeyout/assets/maps/first-room.tmx"), error), qPrintable(error));
        window.show();
        QCoreApplication::processEvents();
        QTest::keyClick(&window, Qt::Key_R);
        capture(window, "08-flooded-start");
        walkTo(window, {208, 176});
        QCOMPARE(window.world().surface(), Surface::Water);
        capture(window, "09-wading");
        walkTo(window, {304, 400});
        QCOMPARE(window.world().surface(), Surface::Boardwalk);
        capture(window, "10-boardwalk");
        walkTo(window, window.world().level().key.center());
        QVERIFY(window.world().hasKey());
        walkTo(window, window.world().level().exit.center());
        QCOMPARE(window.world().state(), GameWorld::State::Won);
        capture(window, "11-flooded-complete");
    }

    void caveProjectionKeepsGroundCollision()
    {
        QString error;
        auto cave = loadLevel(QFINDTESTDATA("../assets/maps/cave-vault.tmx"), error);
        QVERIFY2(cave, qPrintable(error));
        auto old = loadLevel(QStringLiteral(":/onekeyout/assets/maps/first-room.tmx"), error);
        QVERIFY2(old, qPrintable(error));
        auto embedded = loadLevel(QStringLiteral(":/onekeyout/assets/maps/cave-vault.tmx"), error);
        QVERIFY2(embedded, qPrintable(error));
        QCOMPARE(cave->data.walls, embedded->data.walls);
        QCOMPARE(cave->caveSprites.size(), embedded->caveSprites.size());
        QVERIFY(cave->caveSprites.size() > 100);
        QCOMPARE(cave->data.spawn, old->data.spawn + QPointF(0,64));
        QCOMPARE(cave->data.key, old->data.key.translated(0,64));
        QCOMPARE(cave->data.exit, old->data.exit.translated(0,64));
        for (int y=0; y<old->data.height; ++y) {
            for (int x=0; x<old->data.width; ++x) {
                const int a=y*24+x, b=(y+2)*24+x;
                QCOMPARE(cave->data.walls[b], old->data.walls[a]);
                QCOMPARE(cave->data.waterCorners[b], old->data.waterCorners[a]);
                QCOMPARE(cave->data.boardwalkEdges[b], old->data.boardwalkEdges[a]);
            }
        }
        for (int i=0; i<old->data.propColliders.size(); ++i)
            QCOMPARE(cave->data.propColliders[i],old->data.propColliders[i].translated(0,64));
        // A front face consists of two separate tile images with one shared ground anchor.
        const QPointF upperPosition(9*32,8*32), lowerPosition(9*32,9*32);
        int found=0;
        for (const auto &part : cave->caveSprites) {
            if (part.topLeft == upperPosition || part.topLeft == lowerPosition) {
                QCOMPARE(part.groundY, 320.0);
                QCOMPARE(part.image.size(), QSize(32,32));
                ++found;
            }
        }
        QCOMPARE(found,2);
        // Standing behind the projected face is legal; walking into its base is not.
        QVERIFY(hasTerrainClearance(cave->data,GameWorld::footprintAt({304,272})));
        QVERIFY(!hasTerrainClearance(cave->data,GameWorld::footprintAt({304,304})));
        QCOMPARE(slideMove(cave->data,GameWorld::footprintAt({304,336}),{0,-64}),QPointF(0,-11));
    }

    void cavePartsShareDepthAnchor()
    {
        QString error;
        auto level=loadLevel(fixture(),error);
        QVERIFY2(level,qPrintable(error));
        level->props.clear();
        const QColor marker(230,30,190);
        QPixmap part(32,32); part.fill(marker);
        level->caveSprites={{part,{64,0},96},{part,{64,32},96},{part,{64,64},96}};
        GameWorld world;
        RenderProbe view(world);
        view.setLevel(level.get());
        const auto render=[&](qreal y) {
            auto data=level->data; data.spawn={80,y}; world.reset(data);
            return view.renderWorld();
        };
        QCOMPARE(render(62).pixelColor(80,45),marker); // Upper wall hides player behind it.
        QCOMPARE(render(94).pixelColor(80,77),marker);
        QVERIFY(render(98).pixelColor(80,77)!=marker); // Player in front covers lower wall.
        QVERIFY(render(96).pixelColor(80,77)!=marker); // Stable tie across all parts.
    }

    void rejectsStaleCaveOutput_data()
    {
        QTest::addColumn<QString>("changed");
        QTest::newRow("missing-face") << QString("CaveUpper");
        QTest::newRow("stale-collision") << QString("Walls");
        QTest::newRow("edited-source") << QString("CaveShape");
    }

    void rejectsStaleCaveOutput()
    {
        QFETCH(QString,changed);
        QString error;
        auto level=loadLevel(QFINDTESTDATA("../assets/maps/cave-vault.tmx"),error);
        QVERIFY2(level,qPrintable(error));
        bool edited=false;
        for (auto *base : level->map->layers()) {
            if (base->name()!=changed) continue;
            auto *tiles=base->asTileLayer();
            for (int y=0;y<tiles->height() && !edited;++y)
                for (int x=0;x<tiles->width() && !edited;++x)
                    if (!tiles->cellAt(x,y).isEmpty()) {
                        tiles->setCell(x,y,Tiled::Cell()); edited=true;
                    }
        }
        QVERIFY(edited);
        QTemporaryDir directory;
        const auto path=directory.filePath("cave-stale.tmx");
        QVERIFY(Tiled::MapWriter().writeMap(level->map.get(),path));
        QVERIFY(!loadLevel(path,error));
        QVERIFY2(error.contains("Stale cave output"),qPrintable(error));
    }

    void caveKeyboardRoute()
    {
        GameWindow window;
        window.setAutomaticUpdates(false);
        QString error;
        QVERIFY2(window.load(QStringLiteral(":/onekeyout/assets/maps/cave-vault.tmx"),error),qPrintable(error));
        window.resize(1000,850);
        window.show();
        QCoreApplication::processEvents();
        capture(window,"12-cave-start");
        walkTo(window,{304,272}); // Walk behind a two-tile wall without an extra collider.
        capture(window,"13-cave-behind-wall");
        walkTo(window,{400,304}); // Opening between the wall ends.
        capture(window,"14-cave-doorway");
        walkTo(window,{208,240});
        QCOMPARE(window.world().surface(),Surface::Water);
        walkTo(window,{304,464});
        QCOMPARE(window.world().surface(),Surface::Boardwalk);
        capture(window,"15-cave-front");
        walkTo(window,window.world().level().key.center());
        QVERIFY(window.world().hasKey());
        walkTo(window,window.world().level().exit.center());
        QCOMPARE(window.world().state(),GameWorld::State::Won);
        capture(window,"16-cave-complete");
    }
};

QTEST_MAIN(GameTest)
#include "test_game.moc"
