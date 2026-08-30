#pragma once
#include <cstdint>

#include "math/Vec2.h"

namespace ls {

// GDD 12.3. Hitstop and screenshake are pacing and presentation, so they live
// out here in fx/ with no dependency on raylib and no ability to touch the
// simulation. The simulation cannot see them: hitstop withholds whole ticks
// from the fixed timestep rather than scaling dt, so determinism is untouched
// and a replay of the same inputs still produces the same battle.
struct JuiceParams {
    float shakePerKill    = 0.28f;   // amplitude added per kill
    float shakePerHit     = 3.0f;    // amplitude added per base hit
    float shakeDecay      = 7.0f;    // amplitude lost per second
    float shakeMax        = 16.0f;   // hard clamp, in pixels
    float hitstopSeconds  = 0.04f;   // 40 ms, per the GDD
    float hitstopMax      = 0.12f;   // never freeze longer than this
};

class Juice {
public:
    explicit Juice(const JuiceParams& params = JuiceParams{}) : params_(params) {}

    void setParams(const JuiceParams& p) { params_ = p; }
    void setScale(float scale) { scale_ = (scale < 0.0f) ? 0.0f : scale; }
    void setHitstopEnabled(bool on) { hitstopEnabled_ = on; }

    // Feed from the simulation, once per tick.
    void onKills(uint32_t kills);
    void onBaseHit(uint32_t arrivals);
    // Ability detonations and elite kills: loud, deliberate, and allowed to
    // freeze the frame.
    void onDetonation(float shake);

    void update(float dt);
    void reset();

    // True while the frame is frozen. The caller withholds simulation ticks.
    bool  frozen() const { return hitstopLeft_ > 0.0f; }
    float amplitude() const { return shake_ * scale_; }

    // A deterministic offset for the given frame — same frame, same shake, so
    // a screenshot or a recorded run reproduces exactly.
    Vec2 offset(uint64_t frame) const;

private:
    void addShake(float amount);

    JuiceParams params_;
    float shake_ = 0.0f;
    float hitstopLeft_ = 0.0f;
    float scale_ = 1.0f;
    bool  hitstopEnabled_ = true;
};

}  // namespace ls
