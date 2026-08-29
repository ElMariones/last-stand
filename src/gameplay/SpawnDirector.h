#pragma once
#include <cstddef>

#include "gameplay/Level.h"
#include "sim/World.h"

namespace ls {

// Emits a level's spawn schedule over time. Deterministic: it advances an
// internal clock by the tick dt and releases each SpawnEvent whose time has
// been reached, calling into World's seeded spawn path. Construct one per
// battle; it holds no state that must survive the battle.
class SpawnDirector {
public:
    // Advances the clock and emits every due burst. Call once per tick.
    void update(World& world, const Level& level, float dt);

    // True once every burst in the schedule has been emitted.
    bool exhausted(const Level& level) const { return cursor_ >= level.schedule.size(); }

    float elapsedSeconds() const { return clock_; }

private:
    float  clock_  = 0.0f;
    size_t cursor_ = 0u;
};

}  // namespace ls
