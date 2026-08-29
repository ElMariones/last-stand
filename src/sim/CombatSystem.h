#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "math/Vec2.h"
#include "sim/EnemyPool.h"
#include "sim/SpatialHash.h"
#include "sim/Turret.h"

namespace ls {

constexpr uint32_t kMaxTracers = 256;

struct Tracer {
    Vec2  from{0.0f, 0.0f};
    Vec2  to{0.0f, 0.0f};
    float ttl = 0.0f;   // seconds of life remaining
};

// Advances every turret's cooldown (and ability timers), acquires a target
// through the spatial hash, and resolves the shot according to the turret's
// kind: instant single-target hitscan (Machine Gun), splash + knockback
// (Cannon), or cone-applied Burn (Flamethrower). Kills are DEFERRED: a
// killed enemy is left at health 0 and removed by cullDead() after every
// turret has fired, so the spatial hash stays coherent for the whole loop.
void updateCombat(std::vector<Turret>& turrets, EnemyPool& enemies,
                  const SpatialHash& hash, Vec2 basePos, float dt,
                  std::array<Tracer, kMaxTracers>& tracers,
                  uint32_t& tracerCount);

// Applies Burn damage-over-time and, when `ignite` is set (Flamethrower's
// Ignite node), spreads burn to nearby enemies. Runs once per tick after all
// turrets have fired.
void applyBurn(EnemyPool& enemies, const SpatialHash& hash, float dt,
               bool ignite);

// Swap-removes every enemy with health <= 0. Returns how many were culled.
uint32_t cullDead(EnemyPool& enemies);

// Applies damage to enemy i, clamping health at 0. Returns true when this
// shot transitions the enemy from alive to dead.
inline bool applyDamage(EnemyPool& enemies, uint32_t i, float damage) {
    const float before = enemies.health[i];
    float after = before - damage;
    if (after < 0.0f) after = 0.0f;
    enemies.health[i] = after;
    return (before > 0.0f && after <= 0.0f);
}

inline void appendTracer(std::array<Tracer, kMaxTracers>& tracers,
                         uint32_t& tracerCount, Vec2 from, Vec2 to,
                         float ttl) {
    if (tracerCount >= kMaxTracers) return;
    tracers[tracerCount] = Tracer{from, to, ttl};
    ++tracerCount;
}

}  // namespace ls
