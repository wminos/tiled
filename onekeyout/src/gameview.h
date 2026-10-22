#pragma once

#include <QGraphicsView>
#include <QPixmap>
#include <memory>

namespace Tiled { class MapRenderer; }

namespace OneKeyOut {
class GameWorld;
struct LoadedLevel;

class GameView : public QGraphicsView {
    Q_OBJECT
public:
    explicit GameView(GameWorld &world, QWidget *parent = nullptr);
    ~GameView() override;
    void setLevel(const LoadedLevel *level);

signals:
    void actionRequested();

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void drawForeground(QPainter *painter, const QRectF &rect) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void fitLevel();
    void drawLayer(QPainter *painter, const QString &name, const QRectF &rect);
    QRectF actionRect() const;
    GameWorld &m_world;
    const LoadedLevel *m_level = nullptr;
    std::unique_ptr<Tiled::MapRenderer> m_renderer;
    QPixmap m_actors;
};
} // namespace OneKeyOut
