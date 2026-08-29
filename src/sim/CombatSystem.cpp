#include "sim/CombatSystem.h"

namespace ls {

namespace {
constexpr float kTracerTtl = 0.08f;   // ~5 ticks at 60Hz
}  // namespace

void updateCombat(std::vector<Turret>& turrets, EnemyPool& enemies,
                  const SpatialHash& hash, Vec2 basePos, float dt,
                  std::array<Tracer, kMaxTracers>& tracers,
                  uint32_t& tracerCount) {
    for (Turret& t : turrets) {
        t.cooldown -= dt;
        if (t.cooldown > 0.0f) continue;

        const uint32_t target = strategyFor(t.mode).select(
            enemies, hash, t.position, t.range, basePos);
        if (target == EnemyPool::kInvalid) {
            // No live target in range: hold the shot, don't accrue a cooldown
            // debt that would fire the instant an enemy appears.
            t.cooldown = 0.0f;
            continue;
        }

        const bool killed = applyDamage(enemies, target, t.damage);
        appendTracer(tracers, tracerCount, t.position, enemies.position[target],
                     kTracerTtl);
        ++t.shotsFired;
        if (killed) ++t.kills;

        t.cooldown += t.fireInterval;
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
