#include "sim/World.h"

#include <cstring>

namespace ls {

namespace {

constexpr uint64_t kFnvOffset = 1469598103934665603ULL;
constexpr uint64_t kFnvPrime  = 1099511628211ULL;
constexpr float    kHashCell   = 64.0f;   // tuned to the ~160-unit turret range

inline void hashFloat(uint64_t& h, float v) {
    uint32_t bits = 0u;
    std::memcpy(&bits, &v, sizeof(bits));
    for (int b = 0; b < 4; ++b) {
        h ^= static_cast<uint64_t>((bits >> (b * 8)) & 0xFFu);
        h *= kFnvPrime;
    }
}

}  // namespace

World::World(LevelMap levelMap, uint64_t seed)
    : map_(std::move(levelMap)),
      rng_(seed),
      hash_(map_.grid.worldWidth(), map_.grid.worldHeight(), kHashCell,
            EnemyPool::kCapacity) {
    field_.build(map_);
    base_.position = map_.baseCenter();
    base_.radius = map_.grid.cellSize() * 1.5f;
}

void World::spawnWave(uint32_t count) {
    if (map_.spawnCells.empty()) return;

    const float jitter = map_.grid.cellSize() * 0.4f;
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t pick =
            rng_.nextBounded(static_cast<uint32_t>(map_.spawnCells.size()));
        const Vec2 centre =
            map_.grid.cellCenterAt(map_.spawnCells[static_cast<size_t>(pick)]);
        const Vec2 pos{centre.x + rng_.nextRange(-jitter, jitter),
                       centre.y + rng_.nextRange(-jitter, jitter)};
        if (enemies_.spawn(pos, 100.0f, 0u) == EnemyPool::kInvalid) return;
    }
}

void World::placeTurret(Vec2 position) {
    turrets_.push_back(Turret{});   // allocation happens in Prepare, never in-tick
    turrets_.back().position = position;
}

void World::tick(float dt) {
    if (isOver()) return;

    updateMovement(enemies_, field_, dt, movement_);

    hash_.build(enemies_.position, enemies_.count());

    // Age tracers before combat so brand-new ones aren't aged this tick.
    for (uint32_t i = tracerCount_; i-- > 0u;) {
        tracers_[i].ttl -= dt;
        if (tracers_[i].ttl > 0.0f) continue;
        tracers_[i] = tracers_[tracerCount_ - 1u];
        --tracerCount_;
    }

    updateCombat(turrets_, enemies_, hash_, base_.position, dt, tracers_,
                 tracerCount_);
    totalKills_ += cullDead(enemies_);

    uint64_t shots = 0u;
    for (const Turret& t : turrets_) shots += t.shotsFired;
    totalShots_ = shots;

    totalArrived_ += applyArrivals(enemies_, base_);
    ++ticks_;
}

uint64_t World::stateHash() const {
    uint64_t h = kFnvOffset;
    const uint32_t n = enemies_.count();
    for (uint32_t i = 0; i < n; ++i) {
        hashFloat(h, enemies_.position[i].x);
        hashFloat(h, enemies_.position[i].y);
        hashFloat(h, enemies_.health[i]);
    }
    for (const Turret& t : turrets_) {
        hashFloat(h, t.cooldown);
    }
    hashFloat(h, base_.health);
    return h;
}

}  // namespace ls
