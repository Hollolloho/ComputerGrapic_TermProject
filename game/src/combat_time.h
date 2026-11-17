#pragma once
#include <algorithm>
#include <cmath>

namespace aegis::combat {
struct CombatFrameTime { double realDt{},gameDt{},scale{}; };
// Receives world time from the movement owner. Does not infer movement from keys.
class CombatTime {
public:
    static constexpr double ShotDuration=0.25;
    void Reset() { remaining=0; scale=1; }
    bool CanFire() const { return remaining<=0; }
    bool RequestShot() {
        if(!CanFire()) return false;
        remaining=ShotDuration; return true;
    }
    CombatFrameTime Advance(double realDt,double baseGameDt,bool forceNormal=false,bool suspended=false) {
        if(suspended || !std::isfinite(realDt) || !std::isfinite(baseGameDt)
            || realDt<=0 || baseGameDt<=0) return {};
        const double base=std::clamp(baseGameDt/realDt,0.0,1.0);
        const double shotDt=std::min(realDt,remaining);
        const double gameDt=forceNormal ? realDt:shotDt+(realDt-shotDt)*base;
        remaining=std::max(0.0,remaining-realDt);
        if(remaining<1e-9) remaining=0;
        scale=gameDt/realDt;
        return {realDt,gameDt,scale};
    }
    double ShotRemaining() const { return remaining; }
    double Scale() const { return scale; }
private:
    double remaining{},scale{1};
};
}
