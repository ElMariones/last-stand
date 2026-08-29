#include <doctest/doctest.h>
#include "gameplay/SpawnDirector.h"
#include "gameplay/Level.h"

using ls::SpawnDirector;
using ls::Level;

namespace {
Level makeTinyLevel() {
    Level lvl;
    lvl.name = "tiny";
    lvl.schedule = {{0.0f, 3u}, {1.0f, 5u}, {2.5f, 2u}};
    lvl.totalEnemies = 10u;
    lvl.map = ls::makeM1Map();
    return lvl;
}
}  // namespace

TEST_CASE("director emits every enemy by the end of the schedule") {
    const Level lvl = makeTinyLevel();
    ls::World w{ls::makeM1Map(), 7u};
    SpawnDirector d;

    for (int i = 0; i < 200; ++i) d.update(w, lvl, 1.0f / 60.0f);

    CHECK(w.enemies().count() == 10u);
    CHECK(d.exhausted(lvl));
}

TEST_CASE("no enemies spawn before their event time") {
    const Level lvl = makeTinyLevel();
    ls::World w{ls::makeM1Map(), 7u};
    SpawnDirector d;

    d.update(w, lvl, 0.5f);   // only the t=0 burst (3) is due
    CHECK(w.enemies().count() == 3u);
    CHECK_FALSE(d.exhausted(lvl));
}

TEST_CASE("an empty schedule is immediately exhausted and spawns nothing") {
    Level lvl;
    lvl.map = ls::makeM1Map();
    ls::World w{ls::makeM1Map(), 7u};
    SpawnDirector d;

    d.update(w, lvl, 1.0f);
    CHECK(w.enemies().count() == 0u);
    CHECK(d.exhausted(lvl));
}
