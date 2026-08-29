#include <doctest/doctest.h>
#include "gameplay/Level.h"

using ls::Level;

TEST_CASE("Level 1 totals 100 grunts and is named") {
    const Level lvl = ls::makeLevel1();
    CHECK(lvl.name == "The Outskirts");
    CHECK(lvl.recommendedPower == 10u);
    CHECK(lvl.totalEnemies == 100u);
}

TEST_CASE("Level 1 schedule is non-empty, sorted, and starts at time 0") {
    const Level lvl = ls::makeLevel1();
    REQUIRE_FALSE(lvl.schedule.empty());
    CHECK(lvl.schedule.front().timeSeconds == doctest::Approx(0.0f));

    float last = -1.0f;
    uint32_t sum = 0u;
    for (const auto& e : lvl.schedule) {
        CHECK(e.timeSeconds > last);
        last = e.timeSeconds;
        sum += e.count;
    }
    CHECK(sum == lvl.totalEnemies);
}

TEST_CASE("Level 1 map has hardpoints to defend from") {
    const Level lvl = ls::makeLevel1();
    CHECK(lvl.map.hardpoints.size() == 4u);
}

TEST_CASE("economy constants are non-negative and sane") {
    const Level lvl = ls::makeLevel1();
    CHECK(lvl.killValue > 0.0f);
    CHECK(lvl.depthBonusWeight > 0.0f);
}
