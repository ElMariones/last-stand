#include <doctest/doctest.h>
#include "sim/MovementSystem.h"
#include "sim/LevelMap.h"
#include "sim/EnemyPool.h"
#include "sim/EnemyType.h"
#include "ai/FlowField.h"
#include "sim/SpatialHash.h"
#include "math/Rng.h"

#include <vector>

using ls::EnemyPool;
using ls::EnemyType;
using ls::FlowField;
using ls::LevelMap;
using ls::MovementParams;
using ls::SpatialHash;
using ls::Vec2;

namespace {
LevelMap makeOpenMap() {
    LevelMap m;
    m.grid = ls::Grid{20, 20, 10.0f};
    m.walkable.assign(static_cast<size_t>(m.grid.cellCount()), 1u);
    m.baseCell = m.grid.index(19, 10);
    return m;
}

// Separation reads the hash, so every step has to rebuild it over the
// positions it is about to consume — exactly what World::tick does.
void step(EnemyPool& p, const FlowField& f, SpatialHash& h,
          const MovementParams& params, int ticks = 1) {
    std::vector<Vec2> push(EnemyPool::kCapacity, Vec2{0.0f, 0.0f});
    for (int i = 0; i < ticks; ++i) {
        h.build(p.position, p.count());
        ls::updateMovement(p, f, h, 1.0f / 60.0f, params, push);
    }
}

SpatialHash makeHash() { return SpatialHash{200.0f, 200.0f, 16.0f, 1024u}; }

}  // namespace

TEST_CASE("an enemy moves toward the base") {
    const LevelMap m = makeOpenMap();
    FlowField f;
    f.build(m);

    EnemyPool p;
    p.spawn(m.grid.cellCenter(2, 10), EnemyType::Grunt);
    const float startX = p.position[0].x;

    MovementParams params;
    SpatialHash h = makeHash();
    step(p, f, h, params, 10);

    CHECK(p.position[0].x > startX);
}

TEST_CASE("prevPosition captures the position before the move") {
    const LevelMap m = makeOpenMap();
    FlowField f;
    f.build(m);

    EnemyPool p;
    const Vec2 start = m.grid.cellCenter(2, 10);
    p.spawn(start, EnemyType::Grunt);

    MovementParams params;
    SpatialHash h = makeHash();
    step(p, f, h, params);

    CHECK(p.prevPosition[0].x == doctest::Approx(start.x));
    CHECK(p.position[0].x != doctest::Approx(start.x));
}

TEST_CASE("per-enemy speed governs how far enemies travel") {
    const LevelMap m = makeOpenMap();
    FlowField f;
    f.build(m);

    EnemyPool p;
    p.spawn(m.grid.cellCenter(2, 10), EnemyType::Grunt);    // 40 u/s
    p.spawn(m.grid.cellCenter(2, 10), EnemyType::Runner);   // 100 u/s

    const Vec2 g0 = p.position[0];
    const Vec2 r0 = p.position[1];

    MovementParams params;
    params.separationRadius = 0.0f;          // no separation to muddy distances
    params.separationStrength = 0.0f;
    SpatialHash h = makeHash();
    step(p, f, h, params, 30);

    const float gruntTravel = ls::distanceSq(p.position[0], g0);
    const float runnerTravel = ls::distanceSq(p.position[1], r0);
    CHECK(runnerTravel > gruntTravel);
}

TEST_CASE("distant enemies do not push each other") {
    const LevelMap m = makeOpenMap();
    FlowField f;
    f.build(m);

    EnemyPool p;
    p.spawn(m.grid.cellCenter(2, 2), EnemyType::Grunt);
    p.spawn(m.grid.cellCenter(2, 18), EnemyType::Grunt);

    MovementParams params;
    params.separationRadius = 5.0f;
    SpatialHash h = makeHash();
    step(p, f, h, params);

    // Velocity should be pure flow: magnitude equals the enemy's speed.
    CHECK(ls::length(p.velocity[0]) == doctest::Approx(p.speed[0]).epsilon(0.01));
}

TEST_CASE("overlapping enemies push apart") {
    const LevelMap m = makeOpenMap();
    FlowField f;
    f.build(m);

    EnemyPool p;
    const Vec2 at = m.grid.cellCenter(5, 10);
    p.spawn(at, EnemyType::Grunt);
    p.spawn(at + Vec2{1.0f, 0.0f}, EnemyType::Grunt);

    MovementParams params;
    SpatialHash h = makeHash();
    const float before = ls::distanceSq(p.position[0], p.position[1]);
    step(p, f, h, params, 20);
    const float after = ls::distanceSq(p.position[0], p.position[1]);

    CHECK(after > before);
}

TEST_CASE("perfectly coincident enemies separate deterministically") {
    const LevelMap m = makeOpenMap();
    FlowField f;
    f.build(m);

    EnemyPool p;
    const Vec2 at = m.grid.cellCenter(5, 10);
    p.spawn(at, EnemyType::Grunt);
    p.spawn(at, EnemyType::Grunt);

    MovementParams params;
    SpatialHash h = makeHash();
    step(p, f, h, params, 20);

    CHECK(ls::distanceSq(p.position[0], p.position[1]) > 0.01f);
}

TEST_CASE("an enemy on an unreachable cell does not drift") {
    LevelMap m = makeOpenMap();
    m.walkable[static_cast<size_t>(m.grid.index(1, 0))] = 0u;
    m.walkable[static_cast<size_t>(m.grid.index(0, 1))] = 0u;
    m.walkable[static_cast<size_t>(m.grid.index(1, 1))] = 0u;

    FlowField f;
    f.build(m);

    EnemyPool p;
    const Vec2 start = m.grid.cellCenter(0, 0);
    p.spawn(start, EnemyType::Grunt);

    MovementParams params;
    SpatialHash h = makeHash();
    step(p, f, h, params, 10);

    CHECK(p.position[0].x == doctest::Approx(start.x));
    CHECK(p.position[0].y == doctest::Approx(start.y));
}

TEST_CASE("an empty pool is a no-op") {
    const LevelMap m = makeOpenMap();
    FlowField f;
    f.build(m);
    EnemyPool p;
    MovementParams params;
    SpatialHash h = makeHash();
    step(p, f, h, params);
    CHECK(p.count() == 0u);
}

TEST_CASE("hashed separation visits the same neighbours as the O(n^2) loop") {
    // The Stage 1 optimisation is only legitimate if it changes cost, not
    // behaviour. The two paths sum the same set of contributions in a
    // different order, so they agree to float tolerance, not bit-for-bit.
    const LevelMap m = makeOpenMap();
    FlowField f;
    f.build(m);

    const auto fill = [&](EnemyPool& p) {
        ls::Pcg32 rng{99u};
        for (int i = 0; i < 400; ++i) {
            p.spawn(Vec2{rng.nextRange(20.0f, 180.0f),
                         rng.nextRange(20.0f, 180.0f)},
                    EnemyType::Grunt);
        }
    };

    EnemyPool naive;
    EnemyPool hashed;
    fill(naive);
    fill(hashed);

    MovementParams naiveParams;
    naiveParams.naiveSeparation = true;
    MovementParams hashParams;

    SpatialHash hn = makeHash();
    SpatialHash hh = makeHash();
    step(naive, f, hn, naiveParams, 10);
    step(hashed, f, hh, hashParams, 10);

    REQUIRE(naive.count() == hashed.count());
    for (uint32_t i = 0; i < naive.count(); ++i) {
        CHECK(hashed.position[i].x ==
              doctest::Approx(naive.position[i].x).epsilon(0.001));
        CHECK(hashed.position[i].y ==
              doctest::Approx(naive.position[i].y).epsilon(0.001));
    }
}

TEST_CASE("hashed separation still resolves a dense pile-up") {
    // A pathological case for a cell-based query: far more enemies than
    // cells, all inside one cell, all pushing on each other.
    const LevelMap m = makeOpenMap();
    FlowField f;
    f.build(m);

    EnemyPool p;
    const Vec2 at = m.grid.cellCenter(5, 10);
    for (int i = 0; i < 50; ++i) p.spawn(at, EnemyType::Grunt);

    MovementParams params;
    SpatialHash h = makeHash();
    step(p, f, h, params, 60);

    // Nobody is still sitting exactly on top of anybody else.
    for (uint32_t i = 0; i < p.count(); ++i) {
        for (uint32_t j = i + 1u; j < p.count(); ++j) {
            CHECK(ls::distanceSq(p.position[i], p.position[j]) > 1e-4f);
        }
    }
}

TEST_CASE("a separation radius wider than a hash cell still finds everyone") {
    // The cell-pair walk only visits the 3x3 block, so it is correct only
    // while the radius fits in one cell. Wider radii must fall back to the
    // per-entity query rather than silently missing neighbours.
    const LevelMap m = makeOpenMap();
    FlowField f;
    f.build(m);

    const auto fill = [&](EnemyPool& p) {
        ls::Pcg32 rng{7u};
        for (int i = 0; i < 200; ++i) {
            p.spawn(Vec2{rng.nextRange(40.0f, 160.0f),
                         rng.nextRange(40.0f, 160.0f)},
                    EnemyType::Grunt);
        }
    };

    EnemyPool naive;
    EnemyPool wide;
    fill(naive);
    fill(wide);

    MovementParams naiveParams;
    naiveParams.naiveSeparation = true;
    naiveParams.separationRadius = 20.0f;      // > the 16-unit test cell
    MovementParams wideParams;
    wideParams.separationRadius = 20.0f;

    SpatialHash hn = makeHash();
    SpatialHash hw = makeHash();
    REQUIRE(wideParams.separationRadius > hw.cellSize());
    step(naive, f, hn, naiveParams, 10);
    step(wide, f, hw, wideParams, 10);

    for (uint32_t i = 0; i < naive.count(); ++i) {
        CHECK(wide.position[i].x ==
              doctest::Approx(naive.position[i].x).epsilon(0.001));
        CHECK(wide.position[i].y ==
              doctest::Approx(naive.position[i].y).epsilon(0.001));
    }
}
