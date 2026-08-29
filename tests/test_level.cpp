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

TEST_CASE("Level 2 totals 250 and includes Runners") {
    const Level lvl = ls::makeLevel2();
    CHECK(lvl.name == "Refinery Gate");
    CHECK(lvl.recommendedPower == 25u);
    CHECK(lvl.totalEnemies == 250u);

    uint32_t sum = 0u;
    bool hasRunner = false;
    for (const auto& e : lvl.schedule) {
        sum += e.count;
        if (e.type == ls::EnemyType::Runner) hasRunner = true;
    }
    CHECK(sum == lvl.totalEnemies);
    CHECK(hasRunner);
}

TEST_CASE("Level 3 totals 600 and includes Tanks and Runners") {
    const Level lvl = ls::makeLevel3();
    CHECK(lvl.name == "The Narrows");
    CHECK(lvl.totalEnemies == 600u);

    bool hasRunner = false;
    bool hasTank = false;
    for (const auto& e : lvl.schedule) {
        if (e.type == ls::EnemyType::Runner) hasRunner = true;
        if (e.type == ls::EnemyType::Tank) hasTank = true;
    }
    CHECK(hasRunner);
    CHECK(hasTank);
}
