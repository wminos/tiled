#pragma once

#include "leveldata.h"

namespace OneKeyOut {

class GameWorld {
public:
    // World position is the ground anchor, independent of the 32x32 sprite.
    static constexpr qreal PlayerHalfWidth = 9;
    static constexpr qreal PlayerHalfDepth = 5;
    static constexpr qreal Speed = 144;
    enum class State { Playing, Paused, Won };

    void reset(const LevelData &level);
    void restart();
    void togglePause();
    void pause();
    void step(QPointF direction, double seconds);
    QRectF playerBounds() const;
    static QRectF footprintAt(QPointF anchor);
    bool canOccupy(QPointF center) const;

    const LevelData &level() const { return m_level; }
    QPointF position() const { return m_position; }
    QPointF facing() const { return m_facing; }
    State state() const { return m_state; }
    bool hasKey() const { return m_hasKey; }
    bool moving() const { return m_moving; }
    bool nearExit() const;
    double elapsed() const { return m_elapsed; }
    double pickupAge() const { return m_pickupAge; }

private:
    LevelData m_level;
    QPointF m_position;
    QPointF m_facing{0, 1};
    State m_state = State::Playing;
    bool m_hasKey = false;
    bool m_moving = false;
    double m_elapsed = 0;
    double m_pickupAge = 100;
};

} // namespace OneKeyOut
