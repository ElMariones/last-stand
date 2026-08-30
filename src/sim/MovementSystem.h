#pragma once
#include "ai/FlowField.h"
#include "sim/EnemyPool.h"
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
// direction, add a local separation force, integrate.
//
// `hash` must have been built over the pool's CURRENT positions. Separation
// reads it for neighbours; the naive path ignores it.
void updateMovement(EnemyPool& pool,
                    const FlowField& field,
                    const SpatialHash& hash,
                    float dt,
                    const MovementParams& params);

}  // namespace ls
