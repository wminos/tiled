#pragma once

#include "gameworld.h"
#include "levelloader.h"
#include <QElapsedTimer>
#include <QSet>
#include <QTimer>
#include <QWidget>

namespace OneKeyOut {
class GameView;

class GameWindow : public QWidget {
    Q_OBJECT
public:
    explicit GameWindow(QWidget *parent = nullptr);
    bool load(const QString &path, QString &error);
    const GameWorld &world() const { return m_world; }
    // The same fixed-step path is used by the timer and deterministic input tests.
    void advance(double seconds);
    void setAutomaticUpdates(bool enabled);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void tick();
    void clearInput();
    std::unique_ptr<LoadedLevel> m_level;
    GameWorld m_world;
    GameView *m_view;
    QSet<int> m_keys;
    QTimer m_timer;
    QElapsedTimer m_clock;
    double m_accumulator = 0;
};
} // namespace OneKeyOut
