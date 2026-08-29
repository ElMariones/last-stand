#include <doctest/doctest.h>
#include "sim/Targeting.h"
#include "sim/Turret.h"

using ls::EnemyPool;
using ls::SpatialHash;
using ls::TargetingMode;
using ls::Vec2;

namespace {
// Three enemies with distinct position/health so no tie occurs.
// Base at (100, 0). Turret at origin.
struct Fixture {
    EnemyPool pool;
    SpatialHash hash{1000.0f, 1000.0f, 64.0f, 1024u};

    Fixture() {
        pool.spawn(Vec2{10.0f, 0.0f}, ls::EnemyType::Grunt);   // idx 0: closest to turret
        pool.spawn(Vec2{80.0f, 0.0f}, ls::EnemyType::Grunt);   // idx 1: closest to base
        pool.spawn(Vec2{500.0f, 0.0f}, ls::EnemyType::Grunt);  // idx 2: far
        pool.health[0] = 50.0f;
        pool.health[1] = 500.0f;   // strongest
        pool.health[2] = 10.0f;
        hash.build(pool.position, pool.count());
    }

    uint32_t select(TargetingMode m) const {
        return ls::strategyFor(m).select(pool, hash, Vec2{0.0f, 0.0f}, 200.0f,
                                         0.0f, Vec2{100.0f, 0.0f});
    }
};
}  // namespace

TEST_CASE("FIRST picks the enemy closest to the base") {
    Fixture f;
    CHECK(f.select(TargetingMode::First) == 1u);   // (80,0) nearest base (100,0)
}

TEST_CASE("CLOSEST picks the enemy nearest the turret") {
    Fixture f;
    CHECK(f.select(TargetingMode::Closest) == 0u);  // (10,0) nearest origin
}

TEST_CASE("STRONGEST picks the enemy with the most remaining health") {
    Fixture f;
    CHECK(f.select(TargetingMode::Strongest) == 1u);  // 500 HP
}

TEST_CASE("an empty hash yields kInvalid") {
    EnemyPool p;
    SpatialHash h{1000.0f, 1000.0f, 64.0f, 1024u};
    h.build(p.position, p.count());
    CHECK(ls::strategyFor(TargetingMode::First)
              .select(p, h, Vec2{0.0f, 0.0f}, 200.0f, 0.0f, Vec2{0.0f, 0.0f}) ==
          EnemyPool::kInvalid);
}

TEST_CASE("enemies beyond range are ignored") {
    Fixture f;
    // Range too short to reach anything but the closest (idx 0 at 10 units).
    const uint32_t r = ls::strategyFor(TargetingMode::First).select(
        f.pool, f.hash, Vec2{0.0f, 0.0f}, 15.0f, 0.0f, Vec2{100.0f, 0.0f});
    CHECK(r == 0u);
}

TEST_CASE("dead enemies are skipped") {
    Fixture f;
    f.pool.health[1] = 0.0f;   // kill the would-be FIRST target
    // Now FIRST should skip idx 1 and pick idx 0 (next closest to base).
    CHECK(f.select(TargetingMode::First) == 0u);
}

TEST_CASE("strategyFor returns stable, distinct instances per mode") {
    const auto& first = ls::strategyFor(TargetingMode::First);
    const auto& close = ls::strategyFor(TargetingMode::Closest);
    const auto& strong = ls::strategyFor(TargetingMode::Strongest);
    CHECK(&first == &ls::strategyFor(TargetingMode::First));
    CHECK(&first != &close);
    CHECK(&close != &strong);
}

namespace {
// A cluster of three tightly-packed enemies plus one lone enemy.
struct DensestFixture {
    EnemyPool pool;
    SpatialHash hash{1000.0f, 1000.0f, 64.0f, 1024u};

    DensestFixture() {
        // idx 0..2 tightly clustered around (400, 400)
        pool.spawn(Vec2{400.0f, 400.0f}, ls::EnemyType::Grunt);
        pool.spawn(Vec2{405.0f, 400.0f}, ls::EnemyType::Grunt);
        pool.spawn(Vec2{400.0f, 405.0f}, ls::EnemyType::Grunt);
        // idx 3 lone enemy far from any cluster
        pool.spawn(Vec2{900.0f, 100.0f}, ls::EnemyType::Grunt);
        hash.build(pool.position, pool.count());
    }
};
}  // namespace

TEST_CASE("DENSEST picks the enemy at the densest local cluster") {
    DensestFixture f;
    const uint32_t r = ls::strategyFor(TargetingMode::Densest).select(
        f.pool, f.hash, Vec2{380.0f, 380.0f}, 100.0f, 40.0f, Vec2{0.0f, 0.0f});
    // The cluster members (0..2) each have 2 neighbours; the lone enemy has 0.
    CHECK(r == 0u);
}

TEST_CASE("DENSEST with zero splash degrades to first in range") {
    DensestFixture f;
    const uint32_t r = ls::strategyFor(TargetingMode::Densest).select(
        f.pool, f.hash, Vec2{380.0f, 380.0f}, 100.0f, 0.0f, Vec2{0.0f, 0.0f});
    CHECK(r == 0u);
}
