#pragma once
#include "ai/FlowField.h"
#include "sim/EnemyPool.h"

namespace ls {

struct MovementParams {
    float speed              = 40.0f;   // world units per second
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
