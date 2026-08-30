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

// A death, logged for presentation only. Nothing in the simulation reads this
// back: it is a write-only record of who died this tick, so the fx layer can
// put a corpse and a burst of sparks where the enemy actually was. The
// alternative was for the renderer to diff a swap-removed pool between ticks,
// which is not possible, and the golden hashes prove behaviour is unchanged.
struct Death {
    Vec2    position{0.0f, 0.0f};
    Vec2    direction{1.0f, 0.0f};
    uint8_t type = 0u;
};

// Owns the whole simulation. Contains no rendering, no input, no wall-clock
// time — which is what lets it run headless in the benchmark and produce
// bit-identical results from the same seed.
class World {
public:
    World(LevelMap levelMap, uint64_t seed);

    void spawnWave(uint32_t count, EnemyType type = EnemyType::Grunt);
    void placeTurret(Vec2 position);   // appends a default Machine Gun
    void addTracer(Vec2 from, Vec2 to, float ttl);
    void setLevelTotal(uint32_t total) { levelTotal_ = total; }
    // Stage 0 baseline switch; see MovementParams::naiveSeparation.
    void setNaiveSeparation(bool naive) { movement_.naiveSeparation = naive; }
    void tick(float dt);

    // A battle is over when the base falls (defeat) or the invasion is fully
    // spent with no enemies left alive (victory). levelTotal_ (and thus
    // victory) is only meaningful once setLevelTotal has been called.
    bool     isDefeat() const { return base_.isDestroyed(); }
    bool     isVictory() const {
        return !isDefeat() && levelTotal_ > 0u && spawned_ >= levelTotal_ &&
               enemies_.count() == 0u;
    }
    bool     isOver() const { return isDefeat() || isVictory(); }
    uint64_t ticks() const { return ticks_; }
    uint32_t spawned() const { return spawned_; }
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
    Base&            base() { return base_; }
    const std::vector<Turret>& turrets() const { return turrets_; }
    std::vector<Turret>& turrets() { return turrets_; }
    const std::array<Tracer, kMaxTracers>& tracers() const { return tracers_; }
    uint32_t tracerCount() const { return tracerCount_; }

    // Who died during the last tick. Cleared at the start of every tick and
    // capped, because past a few thousand simultaneous deaths nobody can see
    // an individual corpse anyway.
    static constexpr size_t kMaxLoggedDeaths = 4096u;
    const std::vector<Death>& deaths() const { return deaths_; }

    // The renderer reads cell occupancy from here to pick an LOD tier per
    // enemy (GDD 12.2: the trigger is LOCAL density, not global count).
    // Reading it is safe — render never mutates the simulation.
    const SpatialHash& hash() const { return hash_; }

private:
    LevelMap       map_;
    FlowField      field_;
    EnemyPool      enemies_;
    Base           base_;
    Pcg32          rng_;
    MovementParams movement_;
    SpatialHash    hash_;
    std::vector<Turret>             turrets_;
    // Sized once at construction, so no tick ever allocates: applyBurn's
    // "who was alight at the start of the tick" snapshot, and separation's
    // per-enemy force accumulator.
    std::vector<uint8_t>            burnScratch_;
    std::vector<Vec2>               pushScratch_;
    std::vector<Death>              deaths_;
    std::array<Tracer, kMaxTracers> tracers_{};
    uint32_t        tracerCount_  = 0u;
    uint64_t        ticks_        = 0u;
    uint64_t        totalShots_   = 0u;
    uint32_t        totalArrived_ = 0u;
    uint32_t        totalKills_   = 0u;
    uint32_t        spawned_      = 0u;
    uint32_t        levelTotal_   = 0u;
};

}  // namespace ls
