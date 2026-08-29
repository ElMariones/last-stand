#include <doctest/doctest.h>
#include "sim/LevelMap.h"

using ls::LevelMap;

TEST_CASE("M1 map has the expected shape") {
    const LevelMap m = ls::makeM1Map();
    CHECK(m.grid.cols() == 64);
    CHECK(m.grid.rows() == 36);
    CHECK(m.walkable.size() == static_cast<size_t>(64 * 36));
}

TEST_CASE("M1 map walls block the upper and lower blocks") {
    const LevelMap m = ls::makeM1Map();
    CHECK_FALSE(m.isWalkable(25, 5));
    CHECK_FALSE(m.isWalkable(22, 2));
    CHECK_FALSE(m.isWalkable(28, 15));
    CHECK_FALSE(m.isWalkable(25, 30));
}

TEST_CASE("M1 map leaves a five-cell chokepoint") {
    const LevelMap m = ls::makeM1Map();
    for (int cy = 16; cy <= 20; ++cy) {
        CHECK(m.isWalkable(25, cy));
    }
    CHECK_FALSE(m.isWalkable(25, 15));
    CHECK_FALSE(m.isWalkable(25, 21));
}

TEST_CASE("M1 map open areas are walkable") {
    const LevelMap m = ls::makeM1Map();
    CHECK(m.isWalkable(1, 18));
    CHECK(m.isWalkable(58, 18));
    CHECK(m.isWalkable(40, 4));
}

TEST_CASE("isWalkable is false out of bounds rather than crashing") {
    const LevelMap m = ls::makeM1Map();
    CHECK_FALSE(m.isWalkable(-1, 18));
    CHECK_FALSE(m.isWalkable(64, 18));
    CHECK_FALSE(m.isWalkable(18, -1));
    CHECK_FALSE(m.isWalkable(18, 36));
}

TEST_CASE("base and spawns are placed on walkable cells") {
    const LevelMap m = ls::makeM1Map();
    REQUIRE(m.baseCell >= 0);
    CHECK(m.isWalkableIndex(m.baseCell));
    CHECK(m.baseCell == m.grid.index(58, 18));

    REQUIRE_FALSE(m.spawnCells.empty());
    for (const int c : m.spawnCells) {
        CHECK(m.isWalkableIndex(c));
    }
}

TEST_CASE("baseCenter matches the base cell centre") {
    const LevelMap m = ls::makeM1Map();
    CHECK(m.baseCenter().x == doctest::Approx(m.grid.cellCenter(58, 18).x));
    CHECK(m.baseCenter().y == doctest::Approx(m.grid.cellCenter(58, 18).y));
}
