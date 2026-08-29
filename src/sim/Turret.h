#pragma once
#include <cstdint>

#include "math/Vec2.h"
#include "sim/Targeting.h"

namespace ls {

// A placed turret. M2 has only the Machine Gun: 5 damage, 8 shots/sec, mid
// range (GDD 5.2). A dedicated TurretPool with {index,generation} handles
// arrives with placement/removal in M3; here turrets live in a std::vector
// owned by World and populated once before battle.
struct Turret {
    Vec2  position{0.0f, 0.0f};
    float range        = 160.0f;
    float damage       = 5.0f;
    float fireInterval = 0.125f;   // 8 shots per second
    float cooldown     = 0.0f;
    TargetingMode mode = TargetingMode::First;
    uint32_t shotsFired = 0u;
    uint32_t kills      = 0u;
};

}  // namespace ls
