#pragma once
#include <array>
#include <cstdint>
#include <utility>
#include <vector>

#include "ai/FlowField.h"
#include "math/Rng.h"
#include "sim/Base.h"
#include "sim/CombatSystem.h"
#include "sim/EnemyPool.h"
#include "sim/LevelMap.h"
#include "sim/MovementSystem.h"
#include "sim/SpatialHash.h"
#include "sim/Turret.h"

namespace ls {

// Owns the whole simulation. Contains no rendering, no input, no wall-clock
// time — which is what lets it run headless in the benchmark and produce
// bit-identical results from the same seed.
class World {
public:
    World(LevelMap levelMap, uint64_t seed);

    void spawnWave(uint32_t count);
    void placeTurret(Vec2 position);   // appends a default Machine Gun
    void tick(float dt);

    bool     isOver() const { return base_.isDestroyed(); }
    uint64_t ticks() const { return ticks_; }
    uint32_t totalArrived() const { return totalArrived_; }
    uint64_t totalShots() const { return totalShots_; }
    uint32_t totalKills() const { return totalKills_; }

    // FNV-1a over live enemy state, turret cooldowns, and base health. Used
    // by the determinism test and, from M5, by the golden-hash regression test.
    uint64_t stateHash() const;

    const LevelMap&  map() const { return map_; }
    const FlowField& flowField() const { return field_; }
    const EnemyPool& enemies() const { return enemies_; }
    EnemyPool&       enemies() { return enemies_; }
    const Base&      base() const { return base_; }
    const std::vector<Turret>& turrets() const { return turrets_; }
    const std::array<Tracer, kMaxTracers>& tracers() const { return tracers_; }
    uint32_t tracerCount() const { return tracerCount_; }

private:
    LevelMap       map_;
    FlowField      field_;
    EnemyPool      enemies_;
    Base           base_;
    Pcg32          rng_;
    MovementParams movement_;
    SpatialHash    hash_;
    std::vector<Turret>             turrets_;
    std::array<Tracer, kMaxTracers> tracers_{};
    uint32_t        tracerCount_  = 0u;
    uint64_t        ticks_        = 0u;
    uint64_t        totalShots_   = 0u;
    uint32_t        totalArrived_ = 0u;
    uint32_t        totalKills_   = 0u;
};

}  // namespace ls
