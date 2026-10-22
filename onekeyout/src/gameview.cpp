#include "gameview.h"
#include "gameworld.h"
#include "levelloader.h"
#include "maprenderer.h"
#include "tilelayer.h"

#include <QGraphicsScene>
#include <QMouseEvent>
#include <QPainter>
#include <QRadialGradient>
#include <cmath>

namespace OneKeyOut {
namespace {
void glow(QPainter *p, QPointF center, qreal radius, QColor color)
{
    QRadialGradient gradient(center, radius);
    gradient.setColorAt(0, color);
    color.setAlpha(0);
    gradient.setColorAt(1, color);
    p->setPen(Qt::NoPen);
    p->setBrush(gradient);
    p->drawEllipse(center, radius, radius);
}

QFont uiFont(int size, bool bold = false)
{
    QFont result(QStringLiteral("Avenir Next"));
    result.setPixelSize(size);
    result.setBold(bold);
    return result;
}
}

GameView::GameView(GameWorld &world, QWidget *parent)
    : QGraphicsView(parent), m_world(world),
      m_actors(QStringLiteral(":/onekeyout/assets/images/actors.png"))
{
    setScene(new QGraphicsScene(this));
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFocusPolicy(Qt::NoFocus);
    setBackgroundBrush(QColor(QStringLiteral("#101d24")));
    setRenderHint(QPainter::SmoothPixmapTransform, false);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
}

GameView::~GameView() = default;

void GameView::setLevel(const LoadedLevel *level)
{
    m_renderer.reset();
    m_level = level;
    m_renderer = Tiled::MapRenderer::create(level->map.get());
    setSceneRect(level->data.bounds());
    fitLevel();
}

void GameView::fitLevel()
{
    if (!m_level)
        return;
    const QRectF bounds = m_level->data.bounds();
    qreal factor = qMin(viewport()->width() / bounds.width(), viewport()->height() / bounds.height());
    // Preserve integer pixel scaling whenever the window has enough space.
    if (factor >= 1)
        factor = std::floor(factor);
    resetTransform();
    scale(qMax(0.05, factor), qMax(0.05, factor));
    centerOn(bounds.center());
}

void GameView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    fitLevel();
}

void GameView::drawLayer(QPainter *p, const QString &name, const QRectF &rect)
{
    for (auto *layer : m_level->map->layers())
        if (layer->name() == name)
            if (const auto *tiles = layer->asTileLayer())
                m_renderer->drawTileLayer(p, tiles, rect);
}

void GameView::drawBackground(QPainter *p, const QRectF &rect)
{
    p->fillRect(rect, QColor(QStringLiteral("#101d24")));
    if (!m_level)
        return;
    p->save();
    p->setClipRect(m_level->data.bounds());
    drawLayer(p, QStringLiteral("Ground"), rect);
    drawLayer(p, QStringLiteral("Walls"), rect);
    p->restore();
}

QRectF GameView::actionRect() const
{
    const auto center = sceneRect().center();
    return {center.x() - 112, center.y() + 38, 224, 40};
}

void GameView::drawForeground(QPainter *p, const QRectF &rect)
{
    if (!m_level)
        return;
    p->save();
    p->setClipRect(m_level->data.bounds());
    const double time = m_world.elapsed();
    const QPointF exit = m_level->data.exit.center();
    const QPointF key = m_level->data.key.center();
    const QPointF player = m_world.position();
    p->setRenderHint(QPainter::Antialiasing, true);

    glow(p, exit, 58, m_world.hasKey() ? QColor(102, 235, 196, 75) : QColor(96, 175, 171, 27));
    if (!m_world.hasKey())
        glow(p, key, 54 + 3 * std::sin(time * 2), QColor(246, 193, 93, 65));
    glow(p, player, 45, QColor(254, 218, 140, 24));
    p->setRenderHint(QPainter::Antialiasing, false);

    auto sprite = [&](int index, QPointF center, qreal bob = 0) {
        const QPoint origin(qRound(center.x()) - 16, qRound(center.y()) - 20 + qRound(bob));
        p->drawPixmap(QRect(origin, QSize(32, 32)), m_actors, QRect(index * 32, 0, 32, 32));
    };
    sprite(m_world.hasKey() ? 7 : 6, exit, 4);
    if (!m_world.hasKey()) {
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(0, 0, 0, 65));
        p->drawEllipse(QRectF(key.x() - 9, key.y() + 7, 18, 5));
        sprite(5, key, std::sin(time * 3) * 2);
    }
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(0, 0, 0, 100));
    p->drawEllipse(QRectF(player.x() - 10, player.y() + 7, 20, 6));
    const int frame = m_world.moving() ? 1 + int(time * 9) % 2 : 0;
    int playerFrame = frame;
    if (m_world.facing().y() < -0.4)
        playerFrame = m_world.moving() ? 4 : 3;
    sprite(playerFrame, player);
    drawLayer(p, QStringLiteral("Foreground"), rect);

    p->setRenderHint(QPainter::Antialiasing, true);
    if (m_world.pickupAge() < 0.65) {
        const qreal progress = m_world.pickupAge() / 0.65;
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(QColor(248, 208, 126, int(220 * (1 - progress))), 2));
        p->drawEllipse(key, 10 + progress * 42, 10 + progress * 42);
    }

    if (m_world.state() != GameWorld::State::Playing) {
        p->fillRect(sceneRect(), QColor(6, 15, 21, 185));
        const bool won = m_world.state() == GameWorld::State::Won;
        const QPointF center = sceneRect().center();
        const QRectF panel(center.x() - 204, center.y() - 112, 408, 216);
        p->setPen(QPen(QColor(QStringLiteral("#526b66")), 1));
        p->setBrush(QColor(QStringLiteral("#172b30")));
        p->drawRoundedRect(panel, 16, 16);
        p->setPen(QColor(QStringLiteral("#e5bc73")));
        p->setFont(uiFont(12, true));
        p->drawText(panel.adjusted(16, 20, -16, -166), Qt::AlignCenter,
                    won ? QStringLiteral("ONE KEY. ONE WAY OUT.") : QStringLiteral("TAKE YOUR TIME"));
        p->setPen(QColor(QStringLiteral("#f3ebd7")));
        p->setFont(uiFont(32, true));
        p->drawText(QRectF(center.x() - 190, center.y() - 63, 380, 46), Qt::AlignCenter,
                    won ? QStringLiteral("A little escape.") : QStringLiteral("Paused"));
        p->setFont(uiFont(14));
        p->setPen(QColor(QStringLiteral("#aec3ba")));
        p->drawText(QRectF(center.x() - 190, center.y() - 13, 380, 30), Qt::AlignCenter,
                    won ? QStringLiteral("You found the key and opened the vault.")
                        : QStringLiteral("Your way out will still be here."));
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(QStringLiteral("#e7be78")));
        p->drawRoundedRect(actionRect(), 7, 7);
        p->setPen(QColor(QStringLiteral("#18282c")));
        p->setFont(uiFont(13, true));
        p->drawText(actionRect(), Qt::AlignCenter,
                    won ? QStringLiteral("PLAY AGAIN  /  R") : QStringLiteral("CONTINUE  /  ESC"));
    }
    p->restore();
}

void GameView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_world.state() != GameWorld::State::Playing &&
        actionRect().contains(mapToScene(event->position().toPoint()))) {
        emit actionRequested();
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}
} // namespace OneKeyOut
