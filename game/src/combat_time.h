// 실제 시간은 플레이어/UI에, 느린 세계 시간은 적과 탄환에 사용한다.
#pragma once
#include <algorithm>
#include <cmath>

namespace aegis::combat
{
struct CombatFrameTime
{
    double realDeltaTime{};
    double gameDeltaTime{};
    double scale{};
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
        // 이동량으로 정한 기본 배속. 정지 상태는 보통 0.05, 정상 이동은 1이다.
        const double movementScale = std::clamp(baseGameDeltaTime / realDeltaTime, 0.0, 1.0);

        // 사격 직후 0.25초 동안은 적도 정상 속도로 진행한다.
        // 그 시간이 프레임 중간에 끝나면 남은 부분만 느린 배속을 적용한다.
        const double normalTime = std::min(realDeltaTime, remaining);
        const double slowTime = realDeltaTime - normalTime;
        double gameDeltaTime = normalTime + slowTime * movementScale;
        if (forceNormal)
        {
            gameDeltaTime = realDeltaTime;
        }
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
