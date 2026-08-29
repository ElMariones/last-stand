#include <doctest/doctest.h>
#include "sim/CombatSystem.h"
#include "sim/World.h"

#include <vector>

using ls::EnemyPool;
using ls::SpatialHash;
using ls::Tracer;
using ls::Turret;
using ls::Vec2;

TEST_CASE("applyDamage reduces health and clamps at zero") {
    EnemyPool e;
    e.spawn(Vec2{0.0f, 0.0f}, 100.0f, 0u);
    CHECK_FALSE(ls::applyDamage(e, 0u, 30.0f));
    CHECK(e.health[0] == doctest::Approx(70.0f));
    CHECK(ls::applyDamage(e, 0u, 500.0f));   // crosses zero -> kill
    CHECK(e.health[0] == doctest::Approx(0.0f));
}

TEST_CASE("cullDead removes only the dead and reports the count") {
    EnemyPool e;
    e.spawn(Vec2{0.0f, 0.0f}, 100.0f, 0u);
    e.spawn(Vec2{1.0f, 1.0f}, 0.0f, 0u);     // pre-dead
    e.spawn(Vec2{2.0f, 2.0f}, 50.0f, 0u);
    e.spawn(Vec2{3.0f, 3.0f}, 0.0f, 0u);     // pre-dead

    CHECK(ls::cullDead(e) == 2u);
    CHECK(e.count() == 2u);
    CHECK(e.health[0] > 0.0f);
    CHECK(e.health[1] > 0.0f);
}

TEST_CASE("a turret fires at an enemy in range and kills it") {
    EnemyPool e;
    e.spawn(Vec2{50.0f, 0.0f}, 100.0f, 0u);
    SpatialHash h{1000.0f, 1000.0f, 64.0f, 1024u};
    h.build(e.position, e.count());

    std::vector<Turret> t(1);
    t[0].position = Vec2{0.0f, 0.0f};
    t[0].damage = 100.0f;
    t[0].fireInterval = 1.0f;
    t[0].range = 200.0f;

    std::array<Tracer, ls::kMaxTracers> tracers;
    uint32_t tc = 0u;

    ls::updateCombat(t, e, h, Vec2{500.0f, 0.0f}, 1.0f / 60.0f, tracers, tc);

    CHECK(t[0].shotsFired == 1u);
    CHECK(t[0].kills == 1u);
    CHECK(e.count() == 1u);            // not culled yet
    CHECK(e.health[0] == doctest::Approx(0.0f));
    CHECK(tc == 1u);                   // one tracer recorded

    CHECK(ls::cullDead(e) == 1u);
    CHECK(e.count() == 0u);
}

TEST_CASE("a turret respects its fire interval") {
    EnemyPool e;
    e.spawn(Vec2{50.0f, 0.0f}, 10000.0f, 0u);   // survives many shots
    SpatialHash h{1000.0f, 1000.0f, 64.0f, 1024u};
    h.build(e.position, e.count());

    std::vector<Turret> t(1);
    t[0].position = Vec2{0.0f, 0.0f};
    t[0].damage = 5.0f;
    t[0].fireInterval = 0.125f;       // 8/s
    t[0].range = 200.0f;

    std::array<Tracer, ls::kMaxTracers> tracers;
    uint32_t tc = 0u;

    // 60 ticks = 1 second -> expect ~8 shots (within tiny drift tolerance).
    for (int i = 0; i < 60; ++i) {
        ls::updateCombat(t, e, h, Vec2{500.0f, 0.0f}, 1.0f / 60.0f, tracers, tc);
    }

    CHECK(t[0].shotsFired == 8u);
    CHECK(e.health[0] == doctest::Approx(10000.0f - 8.0f * 5.0f).epsilon(0.01));
}

TEST_CASE("a turret with no enemies in range never fires") {
    EnemyPool e;
    e.spawn(Vec2{50.0f, 0.0f}, 100.0f, 0u);
    SpatialHash h{1000.0f, 1000.0f, 64.0f, 1024u};
    h.build(e.position, e.count());

    std::vector<Turret> t(1);
    t[0].position = Vec2{0.0f, 0.0f};
    t[0].range = 10.0f;   // cannot reach (50,0)

    std::array<Tracer, ls::kMaxTracers> tracers;
    uint32_t tc = 0u;

    for (int i = 0; i < 60; ++i) {
        ls::updateCombat(t, e, h, Vec2{500.0f, 0.0f}, 1.0f / 60.0f, tracers, tc);
    }
    CHECK(t[0].shotsFired == 0u);
    CHECK(e.health[0] == doctest::Approx(100.0f));
    CHECK(tc == 0u);
}

TEST_CASE("a World with a turret in the funnel kills Grunts") {
    ls::World w{ls::makeM1Map(), 42u};
    w.placeTurret(w.map().grid.cellCenter(30, 18));
    w.spawnWave(100u);

    for (int i = 0; i < 6000 && !w.isOver(); ++i) w.tick(1.0f / 60.0f);

    CHECK(w.totalShots() > 0u);
    CHECK(w.totalKills() > 0u);
    CHECK(w.turrets().size() == 1u);
}

TEST_CASE("a World behaves identically with no turrets (M1 regression)") {
    ls::World w{ls::makeM1Map(), 1234u};
    CHECK(w.turrets().empty());
    w.spawnWave(100u);
    for (int i = 0; i < 5000 && !w.isOver(); ++i) w.tick(1.0f / 60.0f);
    CHECK(w.isOver());
    CHECK(w.totalKills() == 0u);   // nothing was shot, everything leaked
}
