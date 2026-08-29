#include <doctest/doctest.h>
#include "sim/Grid.h"

using ls::Grid;
using ls::Vec2;

TEST_CASE("grid dimensions") {
    Grid g{64, 36, 20.0f};
    CHECK(g.cols() == 64);
    CHECK(g.rows() == 36);
    CHECK(g.cellCount() == 64 * 36);
    CHECK(g.worldWidth() == doctest::Approx(1280.0f));
    CHECK(g.worldHeight() == doctest::Approx(720.0f));
}

TEST_CASE("index is row-major") {
    Grid g{64, 36, 20.0f};
    CHECK(g.index(0, 0) == 0);
    CHECK(g.index(1, 0) == 1);
    CHECK(g.index(0, 1) == 64);
    CHECK(g.index(63, 35) == 64 * 36 - 1);
}

TEST_CASE("inBounds rejects out-of-range cells") {
    Grid g{64, 36, 20.0f};
    CHECK(g.inBounds(0, 0));
    CHECK(g.inBounds(63, 35));
    CHECK_FALSE(g.inBounds(-1, 0));
    CHECK_FALSE(g.inBounds(0, -1));
    CHECK_FALSE(g.inBounds(64, 0));
    CHECK_FALSE(g.inBounds(0, 36));
}

TEST_CASE("cellCenter sits at the middle of the cell") {
    Grid g{64, 36, 20.0f};
    CHECK(g.cellCenter(0, 0).x == doctest::Approx(10.0f));
    CHECK(g.cellCenter(0, 0).y == doctest::Approx(10.0f));
    CHECK(g.cellCenter(3, 2).x == doctest::Approx(70.0f));
    CHECK(g.cellCenter(3, 2).y == doctest::Approx(50.0f));
}

TEST_CASE("cellCenterAt matches cellCenter") {
    Grid g{64, 36, 20.0f};
    const Vec2 a = g.cellCenter(5, 7);
    const Vec2 b = g.cellCenterAt(g.index(5, 7));
    CHECK(a.x == doctest::Approx(b.x));
    CHECK(a.y == doctest::Approx(b.y));
}

TEST_CASE("worldToCell round-trips through cellCenter") {
    Grid g{64, 36, 20.0f};
    int cx = -1, cy = -1;
    REQUIRE(g.worldToCell(g.cellCenter(12, 9), cx, cy));
    CHECK(cx == 12);
    CHECK(cy == 9);
}

TEST_CASE("worldToCell reports failure outside the grid") {
    Grid g{64, 36, 20.0f};
    int cx = 0, cy = 0;
    CHECK_FALSE(g.worldToCell(Vec2{-1.0f, 10.0f}, cx, cy));
    CHECK_FALSE(g.worldToCell(Vec2{10.0f, -1.0f}, cx, cy));
    CHECK_FALSE(g.worldToCell(Vec2{1280.0f, 10.0f}, cx, cy));
    CHECK_FALSE(g.worldToCell(Vec2{10.0f, 720.0f}, cx, cy));
}
