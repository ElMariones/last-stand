#include "sim/MovementSystem.h"

#include <cmath>

namespace ls {

void updateMovement(EnemyPool& pool,
                    const FlowField& field,
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
        const Vec2 desired = flow * params.speed;

        // ------------------------------------------------------------------
        // STAGE 0 (GDD 15): O(n^2) separation. DELIBERATELY UNOPTIMISED.
        // This is the baseline the M5 spatial hash is measured against.
        // Do not replace it before that measurement is committed.
        // ------------------------------------------------------------------
        Vec2 push{0.0f, 0.0f};
        for (uint32_t j = 0; j < n; ++j) {
            if (j == i) continue;

            const Vec2  delta = pool.position[i] - pool.position[j];
            const float dSq   = lengthSq(delta);
            if (dSq > radiusSq) continue;

            if (dSq <= 1e-6f) {
                // Perfectly coincident. Nudge by index order rather than
                // randomly, so the simulation stays reproducible.
                push += Vec2{(i < j) ? -1.0f : 1.0f, 0.0f};
                continue;
            }

            const float dist = std::sqrt(dSq);
            const float falloff = (radius - dist) / radius;   // 1 at 0, 0 at radius
            push += (delta * (1.0f / dist)) * falloff;
        }

        pool.velocity[i] = desired + push * params.separationStrength;
        pool.position[i] += pool.velocity[i] * dt;
    }
}

}  // namespace ls
