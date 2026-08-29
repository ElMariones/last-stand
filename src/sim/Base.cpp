#include "sim/Base.h"

namespace ls {

uint32_t applyArrivals(EnemyPool& pool, Base& base) {
    const float radiusSq = base.radius * base.radius;
    uint32_t arrived = 0u;

    // Iterate downward: kill() swap-removes the last element into the hole,
    // and walking down means that element has already been examined.
    for (uint32_t i = pool.count(); i-- > 0u;) {
        if (distanceSq(pool.position[i], base.position) > radiusSq) continue;

        base.health -= pool.health[i];
        if (base.health < 0.0f) base.health = 0.0f;
        pool.kill(i);
        ++arrived;
    }
    return arrived;
}

}  // namespace ls
