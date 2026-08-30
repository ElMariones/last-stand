#include "sim/CombatSystem.h"

#include <algorithm>
#include <cmath>

namespace ls {

namespace {

constexpr float kTracerTtl = 0.08f;   // ~5 ticks at 60Hz
constexpr float kIgniteSpreadRadius = 40.0f;
constexpr float kIgniteBurn = 4.0f;   // dps minimum applied by spread
constexpr float kIgniteTtl = 2.0f;

// Damages every living enemy within `radius` of `center` with falloff, and
// pushes them away from `center` by `knockback`. Returns kills.
uint32_t applySplashAt(EnemyPool& enemies, const SpatialHash& hash,
                       Vec2 center, float radius, float damage, float knockback,
                       float armorPierce) {
    uint32_t kills = 0u;
    if (radius <= 0.0f) return kills;

    hash.forEachInRadius(center, radius, [&](uint32_t i, Vec2) {
        if (enemies.health[i] <= 0.0f) return;

        // Knockback earlier in this traversal may already have moved this
        // enemy, so the falloff reads the live position, not the snapshot.
        const Vec2  delta = enemies.position[i] - center;
        const float d = length(delta);
        const float falloff = std::max(0.0f, 1.0f - d / radius);
        if (falloff <= 0.0f) return;

        const float pierce =
            (enemies.type[i] == static_cast<uint8_t>(EnemyType::Tank))
                ? armorPierce : 1.0f;
        if (applyDamage(enemies, i, damage * falloff * pierce)) ++kills;

        if (knockback > 0.0f && d > 1e-4f) {
            enemies.position[i] += normalized(delta) * (knockback * falloff);
        }
    });
    return kills;
}

inline float pierceFor(const EnemyPool& enemies, uint32_t i, float armorPierce) {
    return (enemies.type[i] == static_cast<uint8_t>(EnemyType::Tank)) ? armorPierce
                                                                      : 1.0f;
}

uint32_t fireMachineGun(const Turret& t, EnemyPool& enemies,
                        const SpatialHash& hash, uint32_t target,
                        std::array<Tracer, kMaxTracers>& tracers,
                        uint32_t& tracerCount) {
    appendTracer(tracers, tracerCount, t.position, enemies.position[target],
                 kTracerTtl);

    uint32_t kills = 0u;

    if (t.bulletStorm && (t.shotsFired % 20u) == 19u) {
        // Every 20th shot: a 5-bullet spread hits the target and the four
        // nearest other living enemies at full damage.
        uint32_t hits[5] = {target, EnemyPool::kInvalid, EnemyPool::kInvalid,
                            EnemyPool::kInvalid, EnemyPool::kInvalid};
        uint32_t nhits = 1u;
        hash.forEachInRadius(t.position, t.range, [&](uint32_t j, Vec2) {
            if (nhits >= 5u) return;
            if (j == target || enemies.health[j] <= 0.0f) return;
            hits[nhits++] = j;
        });
        for (uint32_t h = 0; h < nhits; ++h) {
            if (applyDamage(enemies, hits[h],
                            t.damage * pierceFor(enemies, hits[h], t.armorPierce))) {
                ++kills;
            }
        }
        return kills;
    }

    if (applyDamage(enemies, target,
                    t.damage * pierceFor(enemies, target, t.armorPierce))) {
        ++kills;
    }

    if (t.ricochet) {
        // Bounce to one additional enemy at 50% damage (GDD 5.3).
        const Vec2 impact = enemies.position[target];
        uint32_t bounce = EnemyPool::kInvalid;
        float bestD = 1e30f;
        hash.forEachInRadius(t.position, t.range, [&](uint32_t j, Vec2 jPos) {
            if (j == target || enemies.health[j] <= 0.0f) return;
            const float d = distanceSq(jPos, impact);
            if (d < bestD) { bestD = d; bounce = j; }
        });
        if (bounce != EnemyPool::kInvalid &&
            applyDamage(enemies, bounce,
                        t.damage * 0.5f *
                            pierceFor(enemies, bounce, t.armorPierce))) {
            ++kills;
        }
    }

    return kills;
}

uint32_t fireCannon(const Turret& t, EnemyPool& enemies, const SpatialHash& hash,
                    uint32_t target, std::array<Tracer, kMaxTracers>& tracers,
                    uint32_t& tracerCount) {
    const Vec2 impact = enemies.position[target];
    appendTracer(tracers, tracerCount, t.position, impact, kTracerTtl);

    uint32_t kills = applySplashAt(enemies, hash, impact, t.splashRadius,
                                   t.damage, t.knockback, t.armorPierce);

    if (t.clusterShot) {
        // Shell splits into four smaller blasts: three extra at 25% damage.
        const float r = t.splashRadius * 0.6f;
        const Vec2 offsets[3] = {Vec2{r, 0.0f}, Vec2{0.0f, r}, Vec2{r, r}};
        for (const Vec2& off : offsets) {
            kills += applySplashAt(enemies, hash, impact + off, r,
                                   t.damage * 0.25f, t.knockback * 0.5f,
                                   t.armorPierce);
        }
    }
    return kills;
}

uint32_t fireFlamethrower(const Turret& t, EnemyPool& enemies,
                          const SpatialHash& hash, uint32_t target) {
    // Aim the cone at the chosen target (DENSEST by default).
    const Vec2 dir = normalized(enemies.position[target] - t.position);
    if (lengthSq(dir) <= 0.0f) return 0u;

    const float cosHalf =
        std::cos(t.coneHalfAngle * 3.14159265358979323846f / 180.0f);

    hash.forEachInRadius(t.position, t.range, [&](uint32_t i, Vec2 pos) {
        if (enemies.health[i] <= 0.0f) return;

        const Vec2 toEnemy = pos - t.position;
        const float d = length(toEnemy);
        if (d < 1e-4f) {          // enemy right on top of the turret
            enemies.burnDps[i] += t.burnPerHit;
            enemies.burnTtl[i] = std::max(enemies.burnTtl[i], t.burnDuration);
            return;
        }
        const float cosAngle = (toEnemy.x * dir.x + toEnemy.y * dir.y) / d;
        if (cosAngle < cosHalf) return;

        enemies.burnDps[i] += t.burnPerHit;
        enemies.burnTtl[i] = std::max(enemies.burnTtl[i], t.burnDuration);
    });
    return 0u;   // burn does not kill within this call; cullDead counts it later
}

}  // namespace

void updateCombat(std::vector<Turret>& turrets, EnemyPool& enemies,
                  const SpatialHash& hash, Vec2 basePos, float dt,
                  std::array<Tracer, kMaxTracers>& tracers,
                  uint32_t& tracerCount) {
    for (Turret& t : turrets) {
        // Ability timers (Overcharge): 2x fire rate while overcharged, then it
        // overheats and cannot fire at all.
        if (t.overchargeTtl > 0.0f) {
            t.overchargeTtl -= dt;
            if (t.overchargeTtl <= 0.0f) {
                t.overchargeTtl = 0.0f;
                t.overheatTtl = 8.0f;   // GDD 4.3: 4s of 2x, then 8s silence
            }
        }
        if (t.overheatTtl > 0.0f) {
            t.overheatTtl -= dt;
            if (t.overheatTtl < 0.0f) t.overheatTtl = 0.0f;
        }

        const float rate = (t.overchargeTtl > 0.0f) ? 2.0f : 1.0f;
        t.cooldown -= rate * dt;
        if (t.overheatTtl > 0.0f) {
            t.cooldown = 0.0f;   // overheated: hold fire, no cooldown debt
            continue;
        }
        if (t.cooldown > 0.0f) continue;

        const uint32_t target = strategyFor(t.mode).select(
            enemies, hash, t.position, t.range, t.splashRadius, basePos);
        if (target == EnemyPool::kInvalid) {
            t.cooldown = 0.0f;
            continue;
        }

        uint32_t kills = 0u;
        switch (t.kind) {
            case TurretKind::MachineGun:
                kills = fireMachineGun(t, enemies, hash, target, tracers, tracerCount);
                break;
            case TurretKind::Cannon:
                kills = fireCannon(t, enemies, hash, target, tracers, tracerCount);
                break;
            case TurretKind::Flamethrower:
                kills = fireFlamethrower(t, enemies, hash, target);
                break;
        }
        ++t.shotsFired;
        t.kills += kills;
        t.cooldown += t.fireInterval;
    }
}

void applyBurn(EnemyPool& enemies, const SpatialHash& hash, float dt,
               bool ignite, std::vector<uint8_t>& scratch) {
    const uint32_t n = enemies.count();

    // Pass 1: tick every burn down and apply its damage, snapshotting who was
    // alight at the start of this tick.
    for (uint32_t i = 0; i < n; ++i) {
        float& dps = enemies.burnDps[i];
        float& ttl = enemies.burnTtl[i];
        if (ttl <= 0.0f) {
            dps = 0.0f;
            if (ignite) scratch[i] = 0u;
            continue;
        }
        if (ignite) scratch[i] = (dps > 0.0f) ? 1u : 0u;
        ttl -= dt;
        if (dps > 0.0f) applyDamage(enemies, i, dps * dt);
        if (ttl <= 0.0f) dps = 0.0f;
    }

    if (!ignite) return;

    // Pass 2: spread from the snapshot only, so fire advances one hop per
    // tick instead of flashing across the whole crowd inside one loop.
    for (uint32_t i = 0; i < n; ++i) {
        if (scratch[i] == 0u) continue;
        hash.forEachInRadius(enemies.position[i], kIgniteSpreadRadius,
                             [&](uint32_t j, Vec2) {
                                 if (j == i || enemies.health[j] <= 0.0f) return;
                                 enemies.burnDps[j] =
                                     std::max(enemies.burnDps[j], kIgniteBurn);
                                 enemies.burnTtl[j] =
                                     std::max(enemies.burnTtl[j], kIgniteTtl);
                             });
    }
}

uint32_t cullDead(EnemyPool& enemies) {
    uint32_t culled = 0u;
    for (uint32_t i = enemies.count(); i-- > 0u;) {
        if (enemies.health[i] > 0.0f) continue;
        enemies.kill(i);
        ++culled;
    }
    return culled;
}

}  // namespace ls
