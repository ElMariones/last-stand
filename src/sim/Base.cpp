#include "sim/Base.h"

namespace ls {

uint32_t applyArrivals(EnemyPool& pool, Base& base) {
    const float radiusSq = base.radius * base.radius;
    const EnemyStats* stats = enemyStatsTable();
    uint32_t arrived = 0u;

    // Iterate downward: kill() swap-removes the last element into the hole,
    // and walking down means that element has already been examined.
    for (uint32_t i = pool.count(); i-- > 0u;) {
        if (distanceSq(pool.position[i], base.position) > radiusSq) continue;

        // Damage is remaining health times the kind's arrival multiplier, so
        // a Behemoth that lands is a different event from a Grunt that does -
        // and hurting it on the way in still pays, because health is what it
        // spends.
        base.health -= pool.health[i] * stats[pool.type[i]].arrivalMult;
        if (base.health < 0.0f) base.health = 0.0f;
        pool.kill(i);
        ++arrived;
    }
    return arrived;
}

}  // namespace ls
