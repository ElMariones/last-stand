#pragma once
#include <cstdint>

#include "math/Vec2.h"
#include "sim/EnemyPool.h"
#include "sim/SpatialHash.h"

namespace ls {

enum class TargetingMode : uint8_t {
    First,
    Closest,
    Strongest,
    Densest,
};

// Virtual dispatch lives here, and deliberately only here, per GDD 14.4:
// there are a handful of strategies called a few hundred times per second, so
// the indirection is unmeasurable and the extensibility (Tesla's chain logic
// in V1) is genuinely worth it. Enemies, by contrast, have no virtual
// functions at all.
class TargetingStrategy {
public:
    virtual ~TargetingStrategy() = default;

    // Returns the chosen live enemy index within `range` of `origin`, or
    // EnemyPool::kInvalid when nothing is in range. Skips enemies with
    // health <= 0 (killed but not yet culled this tick). `splashRadius` is
    // what DENSEST scores density over (unused by the single-target modes).
    virtual uint32_t select(const EnemyPool& enemies, const SpatialHash& hash,
                            Vec2 origin, float range, float splashRadius,
                            Vec2 basePos) const = 0;
};

class FirstStrategy final : public TargetingStrategy {
public:
    uint32_t select(const EnemyPool&, const SpatialHash&, Vec2 origin,
                    float range, float splashRadius, Vec2 basePos) const override;
};

class ClosestStrategy final : public TargetingStrategy {
public:
    uint32_t select(const EnemyPool&, const SpatialHash&, Vec2 origin,
                    float range, float splashRadius, Vec2 basePos) const override;
};

class StrongestStrategy final : public TargetingStrategy {
public:
    uint32_t select(const EnemyPool&, const SpatialHash&, Vec2 origin,
                    float range, float splashRadius, Vec2 basePos) const override;
};

class DensestStrategy final : public TargetingStrategy {
public:
    uint32_t select(const EnemyPool&, const SpatialHash&, Vec2 origin,
                    float range, float splashRadius, Vec2 basePos) const override;
};

// Returns a reference to a shared, stateless strategy instance per mode.
const TargetingStrategy& strategyFor(TargetingMode mode);

}  // namespace ls
