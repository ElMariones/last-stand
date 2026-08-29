#pragma once
#include "ai/FlowField.h"
#include "sim/EnemyPool.h"

namespace ls {

struct MovementParams {
    // Per-enemy speed lives in EnemyPool::speed (it varies by type); the flow
    // sample is scaled by that, not by a single global number.
    float separationRadius   = 12.0f;
    float separationStrength = 50.0f;
};

// Advances every live enemy by one tick: sample the flow field for
// direction, add a local separation force, integrate.
void updateMovement(EnemyPool& pool,
                    const FlowField& field,
                    float dt,
                    const MovementParams& params);

}  // namespace ls
