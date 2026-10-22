#include "gamewindow.h"
#include "gameview.h"

#include <QApplication>
#include <QKeyEvent>
#include <QPainter>
#include <algorithm>
#include <cmath>

namespace OneKeyOut {
namespace {
QFont uiFont(int size, bool bold = false)
{
    QFont result(QStringLiteral("Avenir Next"));
    result.setPixelSize(size);
    result.setBold(bold);
    return result;
}
}

GameWindow::GameWindow(QWidget *parent)
    : QWidget(parent), m_view(new GameView(m_world, this))
{
    setWindowTitle(QStringLiteral("One Key Out"));
    setMinimumSize(832, 700);
    resize(960, 760);
    setFocusPolicy(Qt::StrongFocus);
    connect(&m_timer, &QTimer::timeout, this, &GameWindow::tick);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(m_view, &GameView::actionRequested, this, [this] {
        clearInput();
        if (m_world.state() == GameWorld::State::Won)
            m_world.restart();
        else
            m_world.togglePause();
        m_view->viewport()->update();
        update();
    });
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state != Qt::ApplicationActive && isVisible()) {
            clearInput();
            m_world.pause();
            update();
            m_view->viewport()->update();
        }
    });
    m_clock.start();
    m_timer.start(16);
}

bool GameWindow::load(const QString &path, QString &error)
{
    auto candidate = loadLevel(path, error);
    if (!candidate)
        return false;
    // Destroy the old renderer before replacing the map it references.
    m_view->setLevel(candidate.get());
    m_level = std::move(candidate);
    m_world.reset(m_level->data);
    clearInput();
    m_view->viewport()->update();
    update();
    return true;
}

void GameWindow::setAutomaticUpdates(bool enabled)
{
    m_clock.restart();
    if (enabled)
        m_timer.start(16);
    else
        m_timer.stop();
}

void GameWindow::clearInput()
{
    m_keys.clear();
    m_accumulator = 0;
    m_clock.restart();
}

void GameWindow::advance(double seconds)
{
    if (!m_level || !std::isfinite(seconds) || seconds <= 0)
        return;
    m_accumulator += std::min(seconds, 0.1);
    constexpr double step = 1.0 / 60.0;
    while (m_accumulator + 1e-9 >= step) {
        const int x = int(m_keys.contains(Qt::Key_D) || m_keys.contains(Qt::Key_Right)) -
                      int(m_keys.contains(Qt::Key_A) || m_keys.contains(Qt::Key_Left));
        const int y = int(m_keys.contains(Qt::Key_S) || m_keys.contains(Qt::Key_Down)) -
                      int(m_keys.contains(Qt::Key_W) || m_keys.contains(Qt::Key_Up));
        m_world.step(QPointF(x, y), step);
        m_accumulator -= step;
    }
    m_view->viewport()->update();
    update();
}

void GameWindow::tick()
{
    const auto elapsed = m_clock.nsecsElapsed();
    m_clock.restart();
    advance(double(elapsed) / 1e9);
}

void GameWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat()) {
        event->accept();
        return;
    }
    switch (event->key()) {
    case Qt::Key_R:
        clearInput();
        m_world.restart();
        break;
    case Qt::Key_Escape:
        clearInput();
        m_world.togglePause();
        break;
    case Qt::Key_Return:
    case Qt::Key_Space:
        if (m_world.state() == GameWorld::State::Won) {
            clearInput();
            m_world.restart();
        } else if (m_world.state() == GameWorld::State::Paused) {
            clearInput();
            m_world.togglePause();
        }
        break;
    case Qt::Key_W: case Qt::Key_A: case Qt::Key_S: case Qt::Key_D:
    case Qt::Key_Up: case Qt::Key_Left: case Qt::Key_Down: case Qt::Key_Right:
        if (m_world.state() == GameWorld::State::Playing)
            m_keys.insert(event->key());
        break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
    event->accept();
    update();
    m_view->viewport()->update();
}

void GameWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (!event->isAutoRepeat())
        m_keys.remove(event->key());
    event->accept();
}

void GameWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::ActivationChange && !isActiveWindow() && isVisible()) {
        clearInput();
        m_world.pause();
        update();
        m_view->viewport()->update();
    }
    QWidget::changeEvent(event);
}

void GameWindow::resizeEvent(QResizeEvent *event)
{
    m_view->setGeometry(32, 108, width() - 64, height() - 204);
    QWidget::resizeEvent(event);
}

void GameWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(QStringLiteral("#101d24")));
    p.setPen(QColor(QStringLiteral("#ddbd83")));
    p.setFont(uiFont(11, true));
    p.drawText(QRect(36, 24, 500, 20), QStringLiteral("A SMALL ADVENTURE  /  01"));
    p.setPen(QColor(QStringLiteral("#f3ebd7")));
    p.setFont(uiFont(31, true));
    p.drawText(QRect(34, 47, 450, 43), QStringLiteral("One Key Out"));

    const QRect badge(width() - 242, 42, 208, 38);
    p.setPen(QPen(QColor(QStringLiteral("#47645c")), 1));
    p.setBrush(QColor(QStringLiteral("#203630")));
    p.drawRoundedRect(badge, 19, 19);
    p.setPen(QColor(QStringLiteral("#e8c888")));
    p.setFont(uiFont(12, true));
    QString objective = QStringLiteral("01  /  FIND THE KEY");
    if (m_world.hasKey())
        objective = QStringLiteral("02  /  REACH THE EXIT");
    if (m_world.state() == GameWorld::State::Won)
        objective = QStringLiteral("VAULT COMPLETE");
    p.drawText(badge, Qt::AlignCenter, objective);

    p.setPen(QPen(QColor(QStringLiteral("#31454a")), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(m_view->geometry().adjusted(-1, -1, 1, 1), 4, 4);
    p.setFont(uiFont(11, true));
    p.setPen(QColor(QStringLiteral("#c6d3c5")));
    const QString title = m_level ? m_level->data.title : QString();
    p.drawText(QRect(36, height() - 77, 340, 20), p.fontMetrics().elidedText(title, Qt::ElideRight, 340));
    p.setFont(uiFont(13));
    p.setPen(QColor(QStringLiteral("#93aaa6")));
    QString hint = QStringLiteral("Find the golden key. Open the mint-lit gate.");
    if (m_world.hasKey())
        hint = QStringLiteral("Key found. The gate is open — find your way out.");
    else if (m_world.nearExit())
        hint = QStringLiteral("The gate is sealed. Find the golden key first.");
    if (m_world.state() == GameWorld::State::Won)
        hint = QStringLiteral("A little courage. A little key. A way out.");
    p.drawText(QRect(36, height() - 49, width() - 70, 24), hint);
    p.setFont(uiFont(12));
    p.setPen(QColor(QStringLiteral("#c6d3c5")));
    p.drawText(QRect(width() - 440, height() - 77, 404, 20), Qt::AlignRight,
               QStringLiteral("WASD / ARROWS   Move     R   Restart     ESC   Pause"));
}
} // namespace OneKeyOut
