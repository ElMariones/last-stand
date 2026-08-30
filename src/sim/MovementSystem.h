#pragma once
#include <vector>

#include "ai/FlowField.h"
#include "sim/EnemyPool.h"
#include "sim/LevelMap.h"
#include "sim/SpatialHash.h"

namespace ls {

struct MovementParams {
    // Per-enemy speed lives in EnemyPool::speed (it varies by type); the flow
    // sample is scaled by that, not by a single global number.
    float separationRadius   = 12.0f;
    float separationStrength = 50.0f;

    // Stage 0 (GDD 15): compare every pair, O(n^2). Kept switchable rather
    // than deleted so the baseline the M5 optimisation is measured against
    // stays reproducible on any machine — `--bench --naive-separation`.
    bool  naiveSeparation    = false;
};

// Advances every live enemy by one tick: sample the flow field for
// direction, add a local separation force, integrate, and resolve against
// walls.
//
// The wall resolution is not optional. The flow field keeps enemies out of
// walls on its own, but separation does not know about geometry, and in a
// dense chokepoint it will happily shove someone into a wall block. The flow
// field is zero inside a wall, so that enemy then stands there for the rest
// of the battle — alive, unkillable in practice, and enough to stop the
// victory condition from ever firing.
//
// `hash` must have been built over the pool's CURRENT positions; separation
// reads neighbours from it. `pushScratch` is a caller-owned buffer of at
// least EnemyPool::kCapacity entries — separation accumulates into it, and
// owning it here is what keeps a tick allocation-free.
//
// Separation is order-independent: every enemy is pushed by where its
// neighbours were at the START of the tick, never by where the ones already
// processed have moved to.
void updateMovement(EnemyPool& pool,
                    const LevelMap& map,
                    const FlowField& field,
                    const SpatialHash& hash,
                    float dt,
                    const MovementParams& params,
                    std::vector<Vec2>& pushScratch);

}  // namespace ls
