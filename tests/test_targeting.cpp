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
        pool.spawn(Vec2{10.0f, 0.0f}, 50.0f, 0u);    // idx 0: closest to turret
        pool.spawn(Vec2{80.0f, 0.0f}, 500.0f, 1u);   // idx 1: closest to base, strongest
        pool.spawn(Vec2{500.0f, 0.0f}, 10.0f, 2u);   // idx 2: far, weak
        hash.build(pool.position, pool.count());
    }

    uint32_t select(TargetingMode m) const {
        return ls::strategyFor(m).select(pool, hash, Vec2{0.0f, 0.0f}, 200.0f,
                                         Vec2{100.0f, 0.0f});
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
              .select(p, h, Vec2{0.0f, 0.0f}, 200.0f, Vec2{0.0f, 0.0f}) ==
          EnemyPool::kInvalid);
}

TEST_CASE("enemies beyond range are ignored") {
    Fixture f;
    // Range too short to reach anything but the closest (idx 0 at 10 units).
    const uint32_t r = ls::strategyFor(TargetingMode::First).select(
        f.pool, f.hash, Vec2{0.0f, 0.0f}, 15.0f, Vec2{100.0f, 0.0f});
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
