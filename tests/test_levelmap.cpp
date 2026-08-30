#include <doctest/doctest.h>
#include "sim/LevelMap.h"
#include "ai/FlowField.h"

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

TEST_CASE("every sector's deploy anchor is somewhere a turret can stand") {
    const ls::LevelMap maps[8] = {
        ls::makeOutskirtsMap(),  ls::makeRefineryMap(),
        ls::makeNarrowsMap(),    ls::makeSplitMap(),
        ls::makeSpiralMap(),     ls::makeCrossroadsMap(),
        ls::makeGauntletMap(),   ls::makeOpenGroundMap(),
    };
    for (const ls::LevelMap& m : maps) {
        int cx = 0;
        int cy = 0;
        REQUIRE(m.grid.worldToCell(m.deployAnchor, cx, cy));
        CHECK(m.isWalkable(cx, cy));
    }
}

TEST_CASE("a default defence finds room on every sector") {
    const ls::LevelMap maps[8] = {
        ls::makeOutskirtsMap(),  ls::makeRefineryMap(),
        ls::makeNarrowsMap(),    ls::makeSplitMap(),
        ls::makeSpiralMap(),     ls::makeCrossroadsMap(),
        ls::makeGauntletMap(),   ls::makeOpenGroundMap(),
    };
    for (const ls::LevelMap& m : maps) {
        const auto spots = ls::defaultDeployPositions(m, 8);
        CHECK(spots.size() == 8u);
        for (const ls::Vec2& p : spots) {
            int cx = 0;
            int cy = 0;
            REQUIRE(m.grid.worldToCell(p, cx, cy));
            CHECK(m.isWalkable(cx, cy));
        }
    }
}

TEST_CASE("every spawn cell on every sector can reach its base") {
    // A spawn the flow field cannot path from is an enemy that stands still
    // for the whole battle: alive, so the victory condition never fires, and
    // the run hangs. Cheap to check, impossible to spot by eye on eight maps.
    const ls::LevelMap maps[8] = {
        ls::makeOutskirtsMap(),  ls::makeRefineryMap(),
        ls::makeNarrowsMap(),    ls::makeSplitMap(),
        ls::makeSpiralMap(),     ls::makeCrossroadsMap(),
        ls::makeGauntletMap(),   ls::makeOpenGroundMap(),
    };
    for (const ls::LevelMap& m : maps) {
        REQUIRE_FALSE(m.spawnCells.empty());
        ls::FlowField field;
        field.build(m);

        for (const int cell : m.spawnCells) {
            const int cx = cell % m.grid.cols();
            const int cy = cell / m.grid.cols();
            CHECK(m.isWalkable(cx, cy));
            CHECK(field.isReachable(cx, cy));
        }
    }
}

TEST_CASE("every sector's base sits somewhere reachable") {
    const ls::LevelMap maps[8] = {
        ls::makeOutskirtsMap(),  ls::makeRefineryMap(),
        ls::makeNarrowsMap(),    ls::makeSplitMap(),
        ls::makeSpiralMap(),     ls::makeCrossroadsMap(),
        ls::makeGauntletMap(),   ls::makeOpenGroundMap(),
    };
    for (const ls::LevelMap& m : maps) {
        CHECK(m.isWalkableIndex(m.baseCell));
    }
}
