#include "sim/MovementSystem.h"

#include <cmath>

namespace ls {

namespace {

// The pairwise separation contribution of neighbour j on enemy i. Shared by
// both paths so the only difference between them is which js they visit.
inline Vec2 separationFrom(const EnemyPool& pool, uint32_t i, uint32_t j,
                           float radius, float radiusSq) {
    const Vec2  delta = pool.position[i] - pool.position[j];
    const float dSq   = lengthSq(delta);
    if (dSq > radiusSq) return Vec2{0.0f, 0.0f};

    if (dSq <= 1e-6f) {
        // Perfectly coincident. Nudge by index order rather than randomly,
        // so the simulation stays reproducible.
        return Vec2{(i < j) ? -1.0f : 1.0f, 0.0f};
    }

    const float dist = std::sqrt(dSq);
    const float falloff = (radius - dist) / radius;   // 1 at 0, 0 at radius
    return (delta * (1.0f / dist)) * falloff;
}

}  // namespace

void updateMovement(EnemyPool& pool,
                    const FlowField& field,
                    const SpatialHash& hash,
                    float dt,
                    const MovementParams& params) {
    const uint32_t n = pool.count();
    if (n == 0u) return;

    for (uint32_t i = 0; i < n; ++i) {
        pool.prevPosition[i] = pool.position[i];
    }

    const float radius   = params.separationRadius;
    const float radiusSq = radius * radius;

    for (uint32_t i = 0; i < n; ++i) {
        const Vec2 flow = field.sample(pool.position[i]);
        const Vec2 desired = flow * pool.speed[i];

        Vec2 push{0.0f, 0.0f};
        if (params.naiveSeparation) {
            // ------------------------------------------------------------
            // STAGE 0 (GDD 15): every pair, every tick. 5,000 enemies is
            // 25,000,000 distance checks per tick, and it shows.
            // ------------------------------------------------------------
            for (uint32_t j = 0; j < n; ++j) {
                if (j == i) continue;
                push += separationFrom(pool, i, j, radius, radiusSq);
            }
        } else {
            // ------------------------------------------------------------
            // STAGE 1: only the enemies binned within the separation radius
            // are considered. The visited set is identical to Stage 0's
            // non-zero contributions; only the summation ORDER differs, so
            // the two paths agree to float tolerance rather than bit-for-bit.
            // ------------------------------------------------------------
            hash.forEachInRadius(pool.position, pool.position[i], radius,
                                 [&](uint32_t j) {
                                     if (j == i) return;
                                     push += separationFrom(pool, i, j, radius,
                                                            radiusSq);
                                 });
        }

        pool.velocity[i] = desired + push * params.separationStrength;
        pool.position[i] += pool.velocity[i] * dt;
    }
}

}  // namespace ls
