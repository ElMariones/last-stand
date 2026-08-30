#include <doctest/doctest.h>
#include "sim/CombatSystem.h"
#include "sim/EnemyType.h"
#include "sim/World.h"

#include <vector>

using ls::EnemyPool;
using ls::EnemyType;
using ls::SpatialHash;
using ls::Tracer;
using ls::Turret;
using ls::Vec2;

TEST_CASE("applyDamage reduces health and clamps at zero") {
    EnemyPool e;
    e.spawn(Vec2{0.0f, 0.0f}, EnemyType::Grunt);
    CHECK_FALSE(ls::applyDamage(e, 0u, 30.0f));
    CHECK(e.health[0] == doctest::Approx(70.0f));
    CHECK(ls::applyDamage(e, 0u, 500.0f));   // crosses zero -> kill
    CHECK(e.health[0] == doctest::Approx(0.0f));
}

TEST_CASE("cullDead removes only the dead and reports the count") {
    EnemyPool e;
    e.spawn(Vec2{0.0f, 0.0f}, EnemyType::Grunt);
    e.spawn(Vec2{1.0f, 1.0f}, EnemyType::Grunt);
    e.spawn(Vec2{2.0f, 2.0f}, EnemyType::Grunt);
    e.spawn(Vec2{3.0f, 3.0f}, EnemyType::Grunt);
    e.health[1] = 0.0f;     // pre-dead
    e.health[3] = 0.0f;     // pre-dead

    CHECK(ls::cullDead(e) == 2u);
    CHECK(e.count() == 2u);
    CHECK(e.health[0] > 0.0f);
    CHECK(e.health[1] > 0.0f);
}

TEST_CASE("a turret fires at an enemy in range and kills it") {
    EnemyPool e;
    e.spawn(Vec2{50.0f, 0.0f}, EnemyType::Grunt);
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
    e.spawn(Vec2{50.0f, 0.0f}, EnemyType::Grunt);
    e.health[0] = 10000.0f;   // survives many shots
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
    e.spawn(Vec2{50.0f, 0.0f}, EnemyType::Grunt);
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

TEST_CASE("cannon damages a cluster in its splash, not enemies outside it") {
    EnemyPool e;
    e.spawn(Vec2{50.0f, 0.0f}, EnemyType::Grunt);
    e.spawn(Vec2{52.0f, 0.0f}, EnemyType::Grunt);
    e.spawn(Vec2{50.0f, 3.0f}, EnemyType::Grunt);
    e.spawn(Vec2{300.0f, 0.0f}, EnemyType::Grunt);
    SpatialHash h{1000.0f, 1000.0f, 64.0f, 1024u};
    h.build(e.position, e.count());

    std::vector<Turret> t(1);
    t[0].kind = ls::TurretKind::Cannon;
    t[0].mode = ls::TargetingMode::Closest;
    t[0].position = Vec2{0.0f, 0.0f};
    t[0].damage = 90.0f;
    t[0].fireInterval = 1.0f;
    t[0].range = 200.0f;
    t[0].splashRadius = 60.0f;

    std::array<Tracer, ls::kMaxTracers> tracers;
    uint32_t tc = 0u;
    ls::updateCombat(t, e, h, Vec2{500.0f, 0.0f}, 1.0f / 60.0f, tracers, tc);

    CHECK(e.health[0] < 100.0f);   // cluster damaged
    CHECK(e.health[1] < 100.0f);
    CHECK(e.health[2] < 100.0f);
    CHECK(e.health[3] == doctest::Approx(100.0f));   // far enemy untouched
}

TEST_CASE("cannon knockback pushes a near enemy away from the impact") {
    EnemyPool e;
    e.spawn(Vec2{50.0f, 0.0f}, EnemyType::Grunt);   // target (closest)
    e.spawn(Vec2{56.0f, 0.0f}, EnemyType::Grunt);   // near impact
    SpatialHash h{1000.0f, 1000.0f, 64.0f, 1024u};
    h.build(e.position, e.count());

    std::vector<Turret> t(1);
    t[0].kind = ls::TurretKind::Cannon;
    t[0].mode = ls::TargetingMode::Closest;
    t[0].position = Vec2{0.0f, 0.0f};
    t[0].damage = 30.0f;          // survives
    t[0].fireInterval = 1.0f;
    t[0].range = 200.0f;
    t[0].splashRadius = 60.0f;
    t[0].knockback = 50.0f;

    std::array<Tracer, ls::kMaxTracers> tracers;
    uint32_t tc = 0u;
    const float before = e.position[1].x;
    ls::updateCombat(t, e, h, Vec2{500.0f, 0.0f}, 1.0f / 60.0f, tracers, tc);

    CHECK(e.position[1].x > before);   // pushed in +x, away from impact at 50
}

TEST_CASE("flamethrower applies Burn, which damages over subsequent ticks") {
    EnemyPool e;
    e.spawn(Vec2{50.0f, 0.0f}, EnemyType::Grunt);
    SpatialHash h{1000.0f, 1000.0f, 64.0f, 1024u};
    h.build(e.position, e.count());

    std::vector<Turret> t(1);
    t[0].kind = ls::TurretKind::Flamethrower;
    t[0].mode = ls::TargetingMode::First;
    t[0].position = Vec2{0.0f, 0.0f};
    t[0].range = 200.0f;
    t[0].burnPerHit = 6.0f;
    t[0].burnDuration = 3.0f;
    t[0].coneHalfAngle = 90.0f;
    t[0].fireInterval = 1.0f;

    std::array<Tracer, ls::kMaxTracers> tracers;
    uint32_t tc = 0u;
    ls::updateCombat(t, e, h, Vec2{500.0f, 0.0f}, 1.0f / 60.0f, tracers, tc);

    CHECK(e.burnDps[0] == doctest::Approx(6.0f));
    CHECK(e.burnTtl[0] == doctest::Approx(3.0f));

    // 60 ticks = 1 second of 6 dps -> 6 damage.
    std::vector<uint8_t> scratch(ls::EnemyPool::kCapacity, 0u);
    for (int i = 0; i < 60; ++i) {
        ls::applyBurn(e, h, 1.0f / 60.0f, false, scratch);
    }
    CHECK(e.health[0] == doctest::Approx(94.0f).epsilon(0.01));
}

TEST_CASE("flamethrower does not burn enemies outside its cone") {
    EnemyPool e;
    e.spawn(Vec2{50.0f, 0.0f}, EnemyType::Grunt);     // in cone
    e.spawn(Vec2{50.0f, 100.0f}, EnemyType::Grunt);   // outside 35-degree cone
    SpatialHash h{1000.0f, 1000.0f, 64.0f, 1024u};
    h.build(e.position, e.count());

    std::vector<Turret> t(1);
    t[0].kind = ls::TurretKind::Flamethrower;
    t[0].mode = ls::TargetingMode::First;   // aims at the enemy in the cone
    t[0].position = Vec2{0.0f, 0.0f};
    t[0].range = 300.0f;
    t[0].burnPerHit = 6.0f;
    t[0].burnDuration = 3.0f;
    t[0].coneHalfAngle = 35.0f;
    t[0].fireInterval = 1.0f;

    std::array<Tracer, ls::kMaxTracers> tracers;
    uint32_t tc = 0u;
    ls::updateCombat(t, e, h, Vec2{500.0f, 0.0f}, 1.0f / 60.0f, tracers, tc);

    CHECK(e.burnTtl[0] > 0.0f);   // burned
    CHECK(e.burnTtl[1] == doctest::Approx(0.0f));   // outside cone, not burned
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
