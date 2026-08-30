#pragma once
#include <cstdint>

#include "math/Vec2.h"
#include "sim/EnemyPool.h"

namespace ls {

struct Base {
    Vec2  position{0.0f, 0.0f};
    float radius    = 30.0f;
    float health    = 2000.0f;
    float maxHealth = 2000.0f;
    float regenPerSecond = 0.0f;   // 0 by default (M1/M2 behaviour unchanged)

    bool isDestroyed() const { return health <= 0.0f; }
};

// Despawns every enemy within the base radius, each dealing damage equal to
// its remaining health (GDD 4.5). Returns how many arrived.
uint32_t applyArrivals(EnemyPool& pool, Base& base);

}  // namespace ls
