#include "sim/Targeting.h"

#include <limits>

namespace ls {

namespace {

constexpr float kInf = std::numeric_limits<float>::infinity();

}  // namespace

uint32_t FirstStrategy::select(const EnemyPool& enemies,
                               const SpatialHash& hash, Vec2 origin,
                               float range, Vec2 basePos) const {
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
                                 float range, Vec2 basePos) const {
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
                                   float range, Vec2 basePos) const {
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

const TargetingStrategy& strategyFor(TargetingMode mode) {
    static const FirstStrategy first;
    static const ClosestStrategy closest;
    static const StrongestStrategy strongest;
    switch (mode) {
        case TargetingMode::First:     return first;
        case TargetingMode::Closest:   return closest;
        case TargetingMode::Strongest: return strongest;
    }
    return first;
}

}  // namespace ls
