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
