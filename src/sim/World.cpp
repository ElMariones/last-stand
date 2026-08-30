#include "sim/World.h"

#include <cstring>

namespace ls {

namespace {

constexpr uint64_t kFnvOffset = 1469598103934665603ULL;
constexpr uint64_t kFnvPrime  = 1099511628211ULL;
// GDD 14.5 says "tuned to roughly the largest query radius", which was right
// while the only client was turret acquisition. From M5 the hash also serves
// separation, whose radius is 12 — and separation is the query that runs
// n times per tick rather than a few dozen. A 64-unit cell made every
// separation query sift a 192x192 neighbourhood to find neighbours within 12.
// The cell is now sized for the hot query — one cell per separation radius,
// so a separation query touches exactly the 3x3 block that can hold a
// neighbour. Turret acquisition just walks more (cheap) cells. Measured at
// 5,000 entities: 64 -> 1.90 ms, 32 -> 1.55, 16 -> 1.25, 12 -> 1.17, 8 -> 1.13.
// Below 12 the curve flattens while the cell array quadruples, so 12 it is.
constexpr float    kHashCell   = 12.0f;

inline void hashBytes(uint64_t& h, const void* data, size_t n) {
    const auto* p = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < n; ++i) {
        h ^= static_cast<uint64_t>(p[i]);
        h *= kFnvPrime;
    }
}

inline void hashFloat(uint64_t& h, float v) {
    uint32_t bits = 0u;
    std::memcpy(&bits, &v, sizeof(bits));
    hashBytes(h, &bits, sizeof(bits));
}

inline void hashU64(uint64_t& h, uint64_t v) { hashBytes(h, &v, sizeof(v)); }

}  // namespace

World::World(LevelMap levelMap, uint64_t seed)
    : map_(std::move(levelMap)),
      rng_(seed),
      hash_(map_.grid.worldWidth(), map_.grid.worldHeight(), kHashCell,
            EnemyPool::kCapacity),
      burnScratch_(static_cast<size_t>(EnemyPool::kCapacity), 0u),
      pushScratch_(static_cast<size_t>(EnemyPool::kCapacity), Vec2{0.0f, 0.0f}) {
    deaths_.reserve(kMaxLoggedDeaths);
    field_.build(map_);
    base_.position = map_.baseCenter();
    base_.radius = map_.grid.cellSize() * 1.5f;
}

void World::spawnWave(uint32_t count, EnemyType type) {
    if (map_.spawnCells.empty()) return;

    const float jitter = map_.grid.cellSize() * 0.4f;
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t pick =
            rng_.nextBounded(static_cast<uint32_t>(map_.spawnCells.size()));
        const Vec2 centre =
            map_.grid.cellCenterAt(map_.spawnCells[static_cast<size_t>(pick)]);
        const Vec2 pos{centre.x + rng_.nextRange(-jitter, jitter),
                       centre.y + rng_.nextRange(-jitter, jitter)};
        const uint32_t idx = enemies_.spawn(pos, type);
        if (idx == EnemyPool::kInvalid) return;
        enemies_.health[idx] *= healthMult_;
        ++spawned_;
    }
}

void World::placeTurret(Vec2 position) {
    turrets_.push_back(Turret{});   // allocation happens in Prepare, never in-tick
    turrets_.back().position = position;
}

void World::addTracer(Vec2 from, Vec2 to, float ttl) {
    appendTracer(tracers_, tracerCount_, from, to, ttl);
}

void World::tick(float dt) {
    if (isOver()) return;

    if (!base_.isDestroyed() && base_.regenPerSecond > 0.0f) {
        base_.health += base_.regenPerSecond * dt;
        if (base_.health > base_.maxHealth) base_.health = base_.maxHealth;
    }

    // Built twice per tick, deliberately. Separation needs bins over the
    // positions it is about to read; combat needs bins over the positions
    // movement just wrote. A counting-sort build is O(n + cells) and costs
    // far less than either consumer querying stale cells would cost in
    // correctness.
    hash_.build(enemies_.position, enemies_.count());
    updateMovement(enemies_, map_, field_, hash_, dt, movement_, pushScratch_);
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

    bool anyIgnite = false;
    for (const Turret& t : turrets_) anyIgnite = anyIgnite || t.ignite;
    applyBurn(enemies_, hash_, dt, anyIgnite, burnScratch_);

    // Log the dead before they are swap-removed. Presentation only; capped
    // and drawn from reserved capacity, so a tick still never allocates.
    deaths_.clear();
    for (uint32_t i = 0; i < enemies_.count(); ++i) {
        if (enemies_.health[i] > 0.0f) continue;
        if (deaths_.size() >= kMaxLoggedDeaths) break;
        const Vec2 v = enemies_.velocity[i];
        deaths_.push_back(Death{enemies_.position[i],
                                (lengthSq(v) > 0.0f) ? normalized(v)
                                                     : Vec2{1.0f, 0.0f},
                                enemies_.type[i]});
    }

    totalKills_ += cullDead(enemies_);

    uint64_t shots = 0u;
    for (const Turret& t : turrets_) shots += t.shotsFired;
    totalShots_ = shots;

    totalArrived_ += applyArrivals(enemies_, base_);
    ++ticks_;
}

uint64_t World::stateHash() const {
    // Everything a replay has to reproduce. The golden-hash regression test
    // (tests/test_golden.cpp) pins this value, so anything left out here is
    // something that test cannot catch drifting.
    uint64_t h = kFnvOffset;
    const uint32_t n = enemies_.count();
    hashU64(h, n);
    hashU64(h, ticks_);
    hashU64(h, spawned_);
    hashU64(h, totalKills_);
    hashU64(h, totalArrived_);
    hashU64(h, totalShots_);

    for (uint32_t i = 0; i < n; ++i) {
        hashFloat(h, enemies_.position[i].x);
        hashFloat(h, enemies_.position[i].y);
        hashFloat(h, enemies_.velocity[i].x);
        hashFloat(h, enemies_.velocity[i].y);
        hashFloat(h, enemies_.health[i]);
        hashFloat(h, enemies_.burnDps[i]);
        hashFloat(h, enemies_.burnTtl[i]);
        hashBytes(h, &enemies_.type[i], sizeof(uint8_t));
    }
    for (const Turret& t : turrets_) {
        hashFloat(h, t.cooldown);
        hashFloat(h, t.overchargeTtl);
        hashFloat(h, t.overheatTtl);
        hashU64(h, t.shotsFired);
        hashU64(h, t.kills);
    }
    hashFloat(h, base_.health);
    return h;
}

}  // namespace ls
