#include "gamewindow.h"
#include "gameworld.h"
#include "levelloader.h"
#include <QtTest>
#include <QDir>
#include <QQueue>
#include <QTemporaryDir>
#include <cmath>

using namespace OneKeyOut;

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

    QString rewrittenMap(QTemporaryDir &dir, const QByteArray &from, const QByteArray &to)
    {
        QFile source(fixture());
        if (!source.open(QIODevice::ReadOnly))
            return {};
        QByteArray text = source.readAll();
        const QString tileset = QDir(QFileInfo(fixture()).absolutePath()).absoluteFilePath(
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
            path.prepend(QPointF((index % data.width) * 32 + 16, (index / data.width) * 32 + 16));
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
        QVERIFY(world.position().x() <= 96 - GameWorld::PlayerHalfSize);
        QVERIFY(world.canOccupy(world.position()));
        world.restart();
        world.step({1, 0}, 60);
        QVERIFY(world.position().x() <= 96 - GameWorld::PlayerHalfSize);
        data.walls[2 * data.width + 3] = false;
        world.reset(data);
        world.step({1, 1}, 1.0 / 60);
        const auto displacement = world.position() - data.spawn;
        QVERIFY(std::abs(std::hypot(displacement.x(), displacement.y()) - GameWorld::Speed / 60) < 0.00001);
    }

    void lockedGatePickupWinAndRestart()
    {
        GameWorld world;
        auto data = simpleLevel();
        data.spawn = data.exit.center();
        world.reset(data);
        world.step({}, 1.0 / 60);
        QCOMPARE(world.state(), GameWorld::State::Playing);
        data = simpleLevel();
        world.reset(data);
        for (int i = 0; i < 80; ++i)
            world.step({1, 0}, 1.0 / 60);
        QVERIFY(world.hasKey());
        QCOMPARE(world.state(), GameWorld::State::Won);
        const auto position = world.position();
        world.step({-1, 0}, 0.1);
        QCOMPARE(world.position(), position);
        world.restart();
        QCOMPARE(world.position(), data.spawn);
        QVERIFY(!world.hasKey());
        QCOMPARE(world.state(), GameWorld::State::Playing);
        QCOMPARE(world.elapsed(), 0.0);
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
};

QTEST_MAIN(GameTest)
#include "test_game.moc"
