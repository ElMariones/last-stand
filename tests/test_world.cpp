#include <doctest/doctest.h>
#include "sim/World.h"

using ls::World;

TEST_CASE("a new world is not over and has no enemies") {
    World w{ls::makeM1Map(), 1234u};
    CHECK_FALSE(w.isOver());
    CHECK(w.enemies().count() == 0u);
    CHECK(w.ticks() == 0u);
}

TEST_CASE("spawnWave creates the requested number of enemies") {
    World w{ls::makeM1Map(), 1234u};
    w.spawnWave(100u);
    CHECK(w.enemies().count() == 100u);
}

TEST_CASE("spawned enemies start on walkable, reachable cells") {
    World w{ls::makeM1Map(), 1234u};
    w.spawnWave(100u);
    for (uint32_t i = 0; i < w.enemies().count(); ++i) {
        int cx = -1, cy = -1;
        REQUIRE(w.map().grid.worldToCell(w.enemies().position[i], cx, cy));
        CHECK(w.map().isWalkable(cx, cy));
        CHECK(w.flowField().isReachable(cx, cy));
    }
}

TEST_CASE("tick advances the tick counter") {
    World w{ls::makeM1Map(), 1234u};
    w.tick(1.0f / 60.0f);
    w.tick(1.0f / 60.0f);
    CHECK(w.ticks() == 2u);
}

TEST_CASE("enemies eventually reach the base and damage it") {
    World w{ls::makeM1Map(), 1234u};
    w.spawnWave(100u);
    const float startHealth = w.base().health;

    for (int i = 0; i < 5000 && !w.isOver(); ++i) w.tick(1.0f / 60.0f);

    CHECK(w.totalArrived() > 0u);
    CHECK(w.base().health < startHealth);
}

TEST_CASE("enough enemies destroy the base and the world ends") {
    World w{ls::makeM1Map(), 1234u};
    w.spawnWave(100u);
    for (int i = 0; i < 5000 && !w.isOver(); ++i) w.tick(1.0f / 60.0f);

    CHECK(w.isOver());
    CHECK(w.base().isDestroyed());
}

TEST_CASE("ticking a finished world is a no-op") {
    World w{ls::makeM1Map(), 1234u};
    w.spawnWave(100u);
    for (int i = 0; i < 5000 && !w.isOver(); ++i) w.tick(1.0f / 60.0f);
    REQUIRE(w.isOver());

    const uint64_t before = w.stateHash();
    w.tick(1.0f / 60.0f);
    CHECK(w.stateHash() == before);
}

TEST_CASE("stateHash changes as the world evolves") {
    World w{ls::makeM1Map(), 1234u};
    w.spawnWave(50u);
    const uint64_t a = w.stateHash();
    for (int i = 0; i < 60; ++i) w.tick(1.0f / 60.0f);
    CHECK(w.stateHash() != a);
}

TEST_CASE("the world reports victory when the invasion is fully cleared") {
    World w{ls::makeM1Map(), 7u};
    w.placeTurret(w.map().baseCenter());
    // An overwhelming turret: one shot kills, range covers the whole field.
    w.turrets().back().range = 5000.0f;
    w.turrets().back().damage = 10000.0f;

    w.setLevelTotal(1u);
    w.spawnWave(1u);

    for (int i = 0; i < 600 && !w.isOver(); ++i) w.tick(1.0f / 60.0f);

    CHECK(w.isVictory());
    CHECK_FALSE(w.isDefeat());
    CHECK(w.totalKills() == 1u);
}

TEST_CASE("the world reports defeat, not victory, when the base falls") {
    World w{ls::makeM1Map(), 7u};
    w.setLevelTotal(100u);
    w.spawnWave(100u);

    for (int i = 0; i < 5000 && !w.isOver(); ++i) w.tick(1.0f / 60.0f);

    CHECK(w.isDefeat());
    CHECK_FALSE(w.isVictory());
}

TEST_CASE("base regen heals toward max health and never overshoots") {
    World w{ls::makeM1Map(), 7u};
    w.base().health = 500.0f;             // simulate prior damage
    w.base().regenPerSecond = 20.0f;

    for (int i = 0; i < 60; ++i) w.tick(1.0f / 60.0f);   // 1 second

    CHECK(w.base().health == doctest::Approx(520.0f).epsilon(0.001));

    // Fast regen must clamp at maxHealth and never exceed it.
    w.base().regenPerSecond = 100000.0f;
    w.tick(1.0f / 60.0f);
    CHECK(w.base().health == doctest::Approx(w.base().maxHealth));
}
