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

TEST_CASE("a multi-wave level releases its waves on the authored curve") {
    // Regression: Level 2 mixes a Grunt wave running to t=26 with a Runner
    // wave starting at t=6. With an unsorted schedule the Runners were held
    // until the Grunt wave finished and then all released in a single tick.
    const Level lvl = ls::makeLevel2();
    ls::World w{lvl.map, 7u};
    SpawnDirector d;

    // 9.5 seconds in, only the bursts authored before then have been emitted.
    // (9.5 rather than a whole second: no authored event sits near it, so
    // accumulated float error in the clock cannot flip the expectation.)
    uint32_t due = 0u;
    for (const auto& e : lvl.schedule) {
        if (e.timeSeconds <= 9.5f) due += e.count;
    }
    for (int i = 0; i < 570; ++i) d.update(w, lvl, 1.0f / 60.0f);

    CHECK(w.spawned() == due);
    CHECK(due < lvl.totalEnemies);          // the level is not front-loaded
    CHECK_FALSE(d.exhausted(lvl));

    // And Runners are already on the field at that point, not queued behind
    // the rest of the Grunts.
    bool sawRunner = false;
    for (uint32_t i = 0; i < w.enemies().count(); ++i) {
        if (w.enemies().type[i] == static_cast<uint8_t>(ls::EnemyType::Runner)) {
            sawRunner = true;
        }
    }
    CHECK(sawRunner);
}
