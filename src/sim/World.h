#pragma once
#include <cstdint>
#include <utility>

#include "ai/FlowField.h"
#include "math/Rng.h"
#include "sim/Base.h"
#include "sim/EnemyPool.h"
#include "sim/LevelMap.h"
#include "sim/MovementSystem.h"

namespace ls {

// Owns the whole simulation. Contains no rendering, no input, no wall-clock
// time — which is what lets it run headless in the benchmark and produce
// bit-identical results from the same seed.
class World {
public:
    World(LevelMap levelMap, uint64_t seed);

    void spawnWave(uint32_t count);
    void tick(float dt);

    bool     isOver() const { return base_.isDestroyed(); }
    uint64_t ticks() const { return ticks_; }
    uint32_t totalArrived() const { return totalArrived_; }

    // FNV-1a over live enemy state and base health. Used by the determinism
    // test and, from M5, by the golden-hash regression test.
    uint64_t stateHash() const;

    const LevelMap&  map() const { return map_; }
    const FlowField& flowField() const { return field_; }
    const EnemyPool& enemies() const { return enemies_; }
    EnemyPool&       enemies() { return enemies_; }
    const Base&      base() const { return base_; }

private:
    LevelMap       map_;
    FlowField      field_;
    EnemyPool      enemies_;
    Base           base_;
    Pcg32          rng_;
    MovementParams movement_;
    uint64_t       ticks_        = 0u;
    uint32_t       totalArrived_ = 0u;
};

}  // namespace ls
