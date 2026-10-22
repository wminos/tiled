#include "gameworld.h"
#include "collision.h"

#include <algorithm>
#include <cmath>

namespace OneKeyOut {

void GameWorld::reset(const LevelData &level)
{
    m_level = level;
    restart();
}

void GameWorld::restart()
{
    m_position = m_level.spawn;
    m_facing = {0, 1};
    m_state = State::Playing;
    m_hasKey = false;
    m_moving = false;
    m_elapsed = 0;
    m_pickupAge = 100;
}

void GameWorld::togglePause()
{
    if (m_state == State::Playing)
        m_state = State::Paused;
    else if (m_state == State::Paused)
        m_state = State::Playing;
    m_moving = false;
}

void GameWorld::pause()
{
    if (m_state == State::Playing)
        m_state = State::Paused;
    m_moving = false;
}

QRectF GameWorld::playerBounds() const
{
    return footprintAt(m_position);
}

QRectF GameWorld::footprintAt(QPointF anchor)
{
    return {anchor.x() - PlayerHalfWidth, anchor.y() - PlayerHalfDepth,
            PlayerHalfWidth * 2, PlayerHalfDepth * 2};
}

bool GameWorld::canOccupy(QPointF anchor) const
{
    const QRectF body = footprintAt(anchor);
    return hasTerrainClearance(m_level, body) && (m_hasKey || !body.intersects(m_level.exit));
}

bool GameWorld::nearExit() const
{
    return m_level.exit.adjusted(-16, -16, 16, 16).intersects(playerBounds());
}

void GameWorld::step(QPointF direction, double seconds)
{
    if (m_state != State::Playing || !std::isfinite(seconds) || seconds <= 0)
        return;
    // A suspended window must not turn one callback into an unbounded simulation.
    seconds = std::min(seconds, 0.1);
    m_elapsed += seconds;
    m_pickupAge += seconds;
    const double length = std::hypot(direction.x(), direction.y());
    if (!std::isfinite(length))
        direction = {};
    else if (length > 1)
        direction /= length;
    if (!direction.isNull())
        m_facing = direction;

    const QPointF before = m_position;
    const QPointF delta = direction * (Speed * seconds);
    // Sweeps prevent solid tunneling; short substeps also sample pickup/exit triggers.
    const int steps = std::max(1, int(std::ceil(std::max(std::abs(delta.x()), std::abs(delta.y())) / 3)));
    for (int i = 0; i < steps; ++i) {
        // Resample each <=3px step when entering water or leaving a boardwalk.
        m_position += slideMove(m_level, playerBounds(), delta / steps * movementScale(surface()),
                                m_hasKey ? QRectF() : m_level.exit);
        if (!m_hasKey && playerBounds().intersects(m_level.key)) {
            m_hasKey = true;
            m_pickupAge = 0;
        }
        if (m_hasKey && playerBounds().intersects(m_level.exit)) {
            m_state = State::Won;
            break;
        }
    }
    m_moving = m_state == State::Playing && before != m_position;
}

} // namespace OneKeyOut
