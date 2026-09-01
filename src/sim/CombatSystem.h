#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "math/Vec2.h"
#include "sim/EnemyPool.h"
#include "sim/LevelMap.h"
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
// `map` is needed because Cannon knockback MOVES enemies, and a shot that
// shoves someone into a wall leaves them there permanently: the flow field is
// zero inside geometry, so they never walk out, never die, and the victory
// condition never fires. Knockback goes through slideAlongWalls for the same
// reason movement does.
void updateCombat(std::vector<Turret>& turrets, EnemyPool& enemies,
                  const SpatialHash& hash, const LevelMap& map, Vec2 basePos,
                  float dt, std::array<Tracer, kMaxTracers>& tracers,
                  uint32_t& tracerCount);

// Applies Burn damage-over-time and, when `ignite` is set (Flamethrower's
// Ignite node), spreads burn to nearby enemies. Runs once per tick after all
// turrets have fired.
//
// `scratch` is a caller-owned buffer of at least EnemyPool::kCapacity bytes,
// used to snapshot who was burning at the START of the tick. Without that
// snapshot, spreading to a higher-indexed enemy would light it in time for
// the same loop to spread from it again, and one flamethrower would ignite
// the entire connected crowd in a single tick.
void applyBurn(EnemyPool& enemies, const SpatialHash& hash, float dt,
               bool ignite, std::vector<uint8_t>& scratch);

// Swap-removes every enemy with health <= 0. Returns how many were culled.
uint32_t cullDead(EnemyPool& enemies);

// Applies damage to enemy i with no mitigation at all, clamping health at 0.
// Returns true when this transitions the enemy from alive to dead. Burn takes
// this path: it ticks sixty times a second, so subtracting flat armour from
// each tick would make any armour whatsoever total immunity to fire.
inline bool applyDamageRaw(EnemyPool& enemies, uint32_t i, float damage) {
    const float before = enemies.health[i];
    float after = before - damage;
    if (after < 0.0f) after = 0.0f;
    enemies.health[i] = after;
    return (before > 0.0f && after <= 0.0f);
}

// A single hit, after the target's armour has taken its cut.
//
// `pierce` is the turret's Armor Piercing multiplier, and it DIVIDES the
// armour rather than multiplying the damage. That is the whole point of the
// node: it is worth exactly as much as the armour it is up against, and
// nothing at all against a target that has none — so it is a considered
// purchase against a Brute sector rather than a flat damage upgrade wearing
// a different name.
inline bool applyDamage(EnemyPool& enemies, uint32_t i, float damage,
                        float pierce = 1.0f) {
    const EnemyStats& s = enemyStatsTable()[enemies.type[i]];
    if (s.armor > 0.0f) {
        const float armor = (pierce > 1.0f) ? (s.armor / pierce) : s.armor;
        const float floorDamage = damage * kMinDamageFraction;
        damage = damage - armor;
        if (damage < floorDamage) damage = floorDamage;
    }
    return applyDamageRaw(enemies, i, damage);
}

inline void appendTracer(std::array<Tracer, kMaxTracers>& tracers,
                         uint32_t& tracerCount, Vec2 from, Vec2 to,
                         float ttl) {
    if (tracerCount >= kMaxTracers) return;
    tracers[tracerCount] = Tracer{from, to, ttl};
    ++tracerCount;
}

}  // namespace ls
