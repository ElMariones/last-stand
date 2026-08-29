#include <doctest/doctest.h>
#include "sim/World.h"

using ls::World;

namespace {
uint64_t runAndHash(uint64_t seed, uint32_t spawn, int ticks) {
    World w{ls::makeM1Map(), seed};
    w.spawnWave(spawn);
    for (int i = 0; i < ticks; ++i) w.tick(1.0f / 60.0f);
    return w.stateHash();
}
}  // namespace

TEST_CASE("the same seed produces an identical world state") {
    CHECK(runAndHash(1234u, 200u, 600) == runAndHash(1234u, 200u, 600));
}

TEST_CASE("determinism holds over a long run") {
    CHECK(runAndHash(777u, 100u, 5000) == runAndHash(777u, 100u, 5000));
}

TEST_CASE("different seeds produce different states") {
    CHECK(runAndHash(1u, 200u, 600) != runAndHash(2u, 200u, 600));
}

TEST_CASE("spawn placement is reproducible from the seed") {
    World a{ls::makeM1Map(), 42u};
    World b{ls::makeM1Map(), 42u};
    a.spawnWave(500u);
    b.spawnWave(500u);

    REQUIRE(a.enemies().count() == b.enemies().count());
    for (uint32_t i = 0; i < a.enemies().count(); ++i) {
        CHECK(a.enemies().position[i].x == doctest::Approx(b.enemies().position[i].x));
        CHECK(a.enemies().position[i].y == doctest::Approx(b.enemies().position[i].y));
    }
}

TEST_CASE("interleaving two worlds does not couple them") {
    // Catches hidden global/static state in any system.
    World a{ls::makeM1Map(), 5u};
    World b{ls::makeM1Map(), 5u};
    a.spawnWave(100u);
    b.spawnWave(100u);

    for (int i = 0; i < 600; ++i) {
        a.tick(1.0f / 60.0f);
        b.tick(1.0f / 60.0f);
    }
    CHECK(a.stateHash() == b.stateHash());
}
