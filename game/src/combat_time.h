// 실제 시간은 플레이어/UI에, 느린 세계 시간은 적과 탄환에 사용한다.
#pragma once
#include <algorithm>
#include <cmath>

namespace aegis::combat
{
struct CombatFrameTime
{
    double realDeltaTime{}, gameDeltaTime{}, scale{};
};

// Receives world time from the movement owner. Does not infer movement from keys.
class CombatTime
{
public:
    static constexpr double ShotDuration = 0.25;

    void Reset()
    {
        remaining = 0;
        scale = 1;
    }

    bool CanFire() const
    {
        return remaining <= 0;
    }

    bool RequestShot()
    {
        if (!CanFire())
        {
            return false;
        }
        remaining = ShotDuration;
        return true;
    }

    CombatFrameTime Advance(double realDeltaTime, double baseGameDeltaTime, bool forceNormal = false,
                            bool suspended = false)
    {
        if (suspended || !std::isfinite(realDeltaTime) || !std::isfinite(baseGameDeltaTime) ||
            realDeltaTime <= 0 || baseGameDeltaTime <= 0)
        {
            return {};
        }
        const double base = std::clamp(baseGameDeltaTime / realDeltaTime, 0.0, 1.0);
        const double shotDt = std::min(realDeltaTime, remaining);
        const double gameDeltaTime = forceNormal ? realDeltaTime : shotDt + (realDeltaTime - shotDt) * base;
        remaining = std::max(0.0, remaining - realDeltaTime);
        if (remaining < 1e-9)
        {
            remaining = 0;
        }
        scale = gameDeltaTime / realDeltaTime;
        return {realDeltaTime, gameDeltaTime, scale};
    }

    double ShotRemaining() const
    {
        return remaining;
    }

    double Scale() const
    {
        return scale;
    }

private:
    double remaining{};
    double scale{1};
};
} // namespace aegis::combat
