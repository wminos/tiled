#include "gamewindow.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QTimer>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("One Key Out"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    app.setOrganizationName(QStringLiteral("prototypes"));
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Find one key. Find your way out."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("map"), QStringLiteral("Play an authored TMX map."), QStringLiteral("path")});
    parser.addOption({QStringLiteral("capture"), QStringLiteral("Save the initial window to PNG after 350 ms."), QStringLiteral("path")});
    parser.addOption({QStringLiteral("quit-after"), QStringLiteral("Exit after this many milliseconds (verification)."), QStringLiteral("ms")});
    parser.addOption({QStringLiteral("validate"), QStringLiteral("Validate the map and exit without opening a window.")});
    parser.process(app);
    bool durationOk = true;
    const int quitAfter = parser.isSet(QStringLiteral("quit-after"))
        ? parser.value(QStringLiteral("quit-after")).toInt(&durationOk) : 0;
    if (!durationOk || (parser.isSet(QStringLiteral("quit-after")) && (quitAfter < 400 || quitAfter > 600000))) {
        qCritical() << "--quit-after must be between 400 and 600000 ms.";
        return 2;
    }
    const QString map = parser.isSet(QStringLiteral("map")) ? parser.value(QStringLiteral("map"))
        : QStringLiteral(":/onekeyout/assets/maps/cave-vault.tmx");
    OneKeyOut::GameWindow window;
    QString error;
    if (!window.load(map, error)) {
        qCritical().noquote() << error;
        if (!parser.isSet(QStringLiteral("validate")) && !parser.isSet(QStringLiteral("capture")) && !quitAfter)
            QMessageBox::critical(nullptr, QStringLiteral("One Key Out — map error"), error);
        return 1;
    }
    if (parser.isSet(QStringLiteral("validate"))) {
        qInfo().noquote() << "Map valid:" << map;
        return 0;
    }
    window.show();
    if (parser.isSet(QStringLiteral("capture"))) {
        const QString path = parser.value(QStringLiteral("capture"));
        QTimer::singleShot(350, &window, [&window, path, &app] {
            if (!QDir().mkpath(QFileInfo(path).absolutePath()) || !window.grab().save(path, "PNG")) {
                qCritical().noquote() << "Cannot save capture:" << path;
                app.exit(1);
            }
        });
    }
    if (quitAfter)
        QTimer::singleShot(quitAfter, &app, &QApplication::quit);
    return app.exec();
}
