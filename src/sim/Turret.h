#pragma once
#include <cstdint>

#include "math/Vec2.h"
#include "sim/Targeting.h"

namespace ls {

enum class TurretKind : uint8_t { MachineGun, Cannon, Flamethrower };

// A placed turret. Combat behaviour branches on `kind` (GDD 14.4: virtual
// dispatch is only for targeting; the few turret kinds are a switch). The
// transformation-line toggles (ricochet, cluster, …) are resolved into these
// fields at placement time so the per-tick combat path reads only a struct.
struct Turret {
    Vec2 position{0.0f, 0.0f};
    TurretKind kind = TurretKind::MachineGun;
    TargetingMode mode = TargetingMode::First;

    float range        = 160.0f;
    float damage       = 5.0f;
    float fireInterval = 0.125f;   // 8 shots per second (Machine Gun)
    float cooldown     = 0.0f;

    // Cannon
    float splashRadius = 0.0f;
    float knockback    = 0.0f;

    // Flamethrower
    float burnPerHit    = 6.0f;
    float burnDuration  = 3.0f;
    float coneHalfAngle = 35.0f;   // degrees

    // Transformation toggles (resolved from the tree at placement)
    bool ricochet    = false;
    bool bulletStorm = false;
    bool clusterShot = false;
    bool ignite      = false;

    // Armor-piercing damage multiplier vs Tanks (1.0 without the node).
    float armorPierce = 1.0f;

    // Ability state (mutated by Overcharge; consumed each tick by combat)
    float overchargeTtl = 0.0f;   // remaining seconds of 2x fire rate
    float overheatTtl   = 0.0f;   // remaining seconds of no firing

    uint32_t shotsFired = 0u;
    uint32_t kills      = 0u;
};

}  // namespace ls
