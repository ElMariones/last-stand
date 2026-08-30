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
    uint32_t best = EnemyPool::kInvalid;
    float bestD = kInf;
    hash.forEachInRadius(origin, range, [&](uint32_t i, Vec2 pos) {
        if (enemies.health[i] <= 0.0f) return;
        const float d = distanceSq(pos, basePos);
        if (d < bestD) {
            bestD = d;
            best = i;
        }
    });
    return best;
}

uint32_t ClosestStrategy::select(const EnemyPool& enemies,
                                 const SpatialHash& hash, Vec2 origin,
                                 float range, float splashRadius,
                                 Vec2 basePos) const {
    (void)splashRadius;
    (void)basePos;
    uint32_t best = EnemyPool::kInvalid;
    float bestD = kInf;
    hash.forEachInRadius(origin, range, [&](uint32_t i, Vec2 pos) {
        if (enemies.health[i] <= 0.0f) return;
        const float d = distanceSq(pos, origin);
        if (d < bestD) {
            bestD = d;
            best = i;
        }
    });
    return best;
}

uint32_t StrongestStrategy::select(const EnemyPool& enemies,
                                   const SpatialHash& hash, Vec2 origin,
                                   float range, float splashRadius,
                                   Vec2 basePos) const {
    (void)splashRadius;
    (void)basePos;
    uint32_t best = EnemyPool::kInvalid;
    float bestHp = 0.0f;
    hash.forEachInRadius(origin, range, [&](uint32_t i, Vec2) {
        const float hp = enemies.health[i];
        if (hp <= 0.0f) return;
        if (hp > bestHp) {
            bestHp = hp;
            best = i;
        }
    });
    return best;
}

uint32_t DensestStrategy::select(const EnemyPool& enemies,
                                 const SpatialHash& hash, Vec2 origin,
                                 float range, float splashRadius,
                                 Vec2 basePos) const {
    (void)basePos;
    uint32_t best = EnemyPool::kInvalid;
    int bestCount = -1;
    hash.forEachInRadius(origin, range, [&](uint32_t i, Vec2 pos) {
        if (enemies.health[i] <= 0.0f) return;

        // Count other living enemies within the splash radius of candidate i.
        // This nested walk is why the hash exposes a callback rather than a
        // shared result buffer: the outer traversal is still in flight.
        int count = 0;
        hash.forEachInRadius(pos, splashRadius, [&](uint32_t j, Vec2) {
            if (j != i && enemies.health[j] > 0.0f) ++count;
        });
        // Strict > keeps the first candidate visited on ties. Visit order is
        // the hash's cell-major order, which is fixed for a given build, so
        // selection stays stable and deterministic.
        if (count > bestCount) {
            bestCount = count;
            best = i;
        }
    });
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
