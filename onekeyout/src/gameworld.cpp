#include "gameworld.h"

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
    return {m_position.x() - PlayerHalfSize, m_position.y() - PlayerHalfSize,
            PlayerHalfSize * 2, PlayerHalfSize * 2};
}

bool GameWorld::canOccupy(QPointF center) const
{
    constexpr double epsilon = 0.001;
    const int left = int(std::floor((center.x() - PlayerHalfSize) / m_level.tileSize));
    const int right = int(std::floor((center.x() + PlayerHalfSize - epsilon) / m_level.tileSize));
    const int top = int(std::floor((center.y() - PlayerHalfSize) / m_level.tileSize));
    const int bottom = int(std::floor((center.y() + PlayerHalfSize - epsilon) / m_level.tileSize));
    for (int y = top; y <= bottom; ++y)
        for (int x = left; x <= right; ++x)
            if (m_level.blocked(x, y))
                return false;
    return true;
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
    // Short substeps also protect callers outside the normal 60 Hz loop.
    const int steps = std::max(1, int(std::ceil(std::max(std::abs(delta.x()), std::abs(delta.y())) / 3)));
    for (int i = 0; i < steps; ++i) {
        const QPointF xStep{m_position.x() + delta.x() / steps, m_position.y()};
        if (canOccupy(xStep))
            m_position = xStep;
        const QPointF yStep{m_position.x(), m_position.y() + delta.y() / steps};
        if (canOccupy(yStep))
            m_position = yStep;
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
