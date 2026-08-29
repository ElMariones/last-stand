#include "sim/Targeting.h"

#include <limits>

namespace ls {

namespace {

constexpr float kInf = std::numeric_limits<float>::infinity();

}  // namespace

uint32_t FirstStrategy::select(const EnemyPool& enemies,
                               const SpatialHash& hash, Vec2 origin,
                               float range, float splashRadius,
                               Vec2 basePos) const {
    (void)splashRadius;
    const SpatialQuery q = hash.query(enemies.position, origin, range);
    uint32_t best = EnemyPool::kInvalid;
    float bestD = kInf;
    for (uint32_t k = 0; k < q.count; ++k) {
        const uint32_t i = q.indices[k];
        if (enemies.health[i] <= 0.0f) continue;
        const float d = distanceSq(enemies.position[i], basePos);
        if (d < bestD) {
            bestD = d;
            best = i;
        }
    }
    return best;
}

uint32_t ClosestStrategy::select(const EnemyPool& enemies,
                                 const SpatialHash& hash, Vec2 origin,
                                 float range, float splashRadius,
                                 Vec2 basePos) const {
    (void)splashRadius;
    (void)basePos;
    const SpatialQuery q = hash.query(enemies.position, origin, range);
    uint32_t best = EnemyPool::kInvalid;
    float bestD = kInf;
    for (uint32_t k = 0; k < q.count; ++k) {
        const uint32_t i = q.indices[k];
        if (enemies.health[i] <= 0.0f) continue;
        const float d = distanceSq(enemies.position[i], origin);
        if (d < bestD) {
            bestD = d;
            best = i;
        }
    }
    return best;
}

uint32_t StrongestStrategy::select(const EnemyPool& enemies,
                                   const SpatialHash& hash, Vec2 origin,
                                   float range, float splashRadius,
                                   Vec2 basePos) const {
    (void)splashRadius;
    (void)basePos;
    const SpatialQuery q = hash.query(enemies.position, origin, range);
    uint32_t best = EnemyPool::kInvalid;
    float bestHp = 0.0f;
    for (uint32_t k = 0; k < q.count; ++k) {
        const uint32_t i = q.indices[k];
        const float hp = enemies.health[i];
        if (hp <= 0.0f) continue;
        if (hp > bestHp) {
            bestHp = hp;
            best = i;
        }
    }
    return best;
}

uint32_t DensestStrategy::select(const EnemyPool& enemies,
                                 const SpatialHash& hash, Vec2 origin,
                                 float range, float splashRadius,
                                 Vec2 basePos) const {
    (void)basePos;
    const SpatialQuery q = hash.query(enemies.position, origin, range);
    uint32_t best = EnemyPool::kInvalid;
    int bestCount = -1;
    for (uint32_t k = 0; k < q.count; ++k) {
        const uint32_t i = q.indices[k];
        if (enemies.health[i] <= 0.0f) continue;

        // Count other living enemies within the splash radius of candidate i.
        const SpatialQuery near =
            hash.query(enemies.position, enemies.position[i], splashRadius);
        int count = 0;
        for (uint32_t n = 0; n < near.count; ++n) {
            const uint32_t j = near.indices[n];
            if (j != i && enemies.health[j] > 0.0f) ++count;
        }
        // Strict > keeps the lowest index on ties, so selection is stable and
        // deterministic.
        if (count > bestCount) {
            bestCount = count;
            best = i;
        }
    }
    return best;
}

const TargetingStrategy& strategyFor(TargetingMode mode) {
    static const FirstStrategy first;
    static const ClosestStrategy closest;
    static const StrongestStrategy strongest;
    static const DensestStrategy densest;
    switch (mode) {
        case TargetingMode::First:     return first;
        case TargetingMode::Closest:   return closest;
        case TargetingMode::Strongest: return strongest;
        case TargetingMode::Densest:   return densest;
    }
    return first;
}

}  // namespace ls
