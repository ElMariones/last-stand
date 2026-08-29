#include <doctest/doctest.h>
#include "ai/FlowField.h"
#include "sim/LevelMap.h"

using ls::FlowField;
using ls::LevelMap;
using ls::Vec2;

namespace {
LevelMap makeOpenMap() {
    LevelMap m;
    m.grid = ls::Grid{20, 20, 10.0f};
    m.walkable.assign(static_cast<size_t>(m.grid.cellCount()), 1u);
    m.baseCell = m.grid.index(10, 10);
    m.spawnCells.push_back(m.grid.index(0, 10));
    return m;
}
}  // namespace

TEST_CASE("cost at the base is zero") {
    FlowField f;
    f.build(makeOpenMap());
    CHECK(f.costAt(10, 10) == doctest::Approx(0.0f));
}

TEST_CASE("cost increases with distance from the base") {
    FlowField f;
    f.build(makeOpenMap());
    CHECK(f.costAt(9, 10) < f.costAt(5, 10));
    CHECK(f.costAt(5, 10) < f.costAt(0, 10));
}

TEST_CASE("direction points toward the base on an open map") {
    FlowField f;
    f.build(makeOpenMap());
    const Vec2 d = f.dirAt(5, 10);
    CHECK(d.x > 0.5f);
    CHECK(d.y == doctest::Approx(0.0f).epsilon(0.01));
}

TEST_CASE("the base cell itself has no direction") {
    FlowField f;
    f.build(makeOpenMap());
    CHECK(f.dirAt(10, 10).x == doctest::Approx(0.0f));
    CHECK(f.dirAt(10, 10).y == doctest::Approx(0.0f));
}

TEST_CASE("directions are unit length wherever reachable and not the base") {
    FlowField f;
    f.build(makeOpenMap());
    CHECK(ls::length(f.dirAt(3, 3)) == doctest::Approx(1.0f));
    CHECK(ls::length(f.dirAt(19, 0)) == doctest::Approx(1.0f));
}

TEST_CASE("walled-off cells are unreachable") {
    LevelMap m = makeOpenMap();
    m.walkable[static_cast<size_t>(m.grid.index(1, 0))] = 0u;
    m.walkable[static_cast<size_t>(m.grid.index(0, 1))] = 0u;
    m.walkable[static_cast<size_t>(m.grid.index(1, 1))] = 0u;

    FlowField f;
    f.build(m);
    CHECK_FALSE(f.isReachable(0, 0));
    CHECK(f.costAt(0, 0) == FlowField::kUnreachable);
    CHECK(f.dirAt(0, 0).x == 0.0f);
    CHECK(f.dirAt(0, 0).y == 0.0f);
}

TEST_CASE("wall cells are unreachable") {
    LevelMap m = makeOpenMap();
    m.walkable[static_cast<size_t>(m.grid.index(4, 4))] = 0u;
    FlowField f;
    f.build(m);
    CHECK_FALSE(f.isReachable(4, 4));
}

TEST_CASE("paths route around obstacles rather than through them") {
    const LevelMap m = ls::makeM1Map();
    FlowField f;
    f.build(m);
    const float cost = f.costAt(5, 5);
    CHECK(f.isReachable(5, 5));
    CHECK(cost > 53.0f);
}

TEST_CASE("the whole M1 spawn line can reach the base") {
    const LevelMap m = ls::makeM1Map();
    FlowField f;
    f.build(m);
    for (const int c : m.spawnCells) {
        const int cx = c % m.grid.cols();
        const int cy = c / m.grid.cols();
        CHECK(f.isReachable(cx, cy));
    }
}

TEST_CASE("sample returns zero outside the grid") {
    FlowField f;
    f.build(makeOpenMap());
    CHECK(f.sample(Vec2{-5.0f, -5.0f}).x == 0.0f);
    CHECK(f.sample(Vec2{9999.0f, 9999.0f}).y == 0.0f);
}

TEST_CASE("sample agrees with dirAt for the containing cell") {
    FlowField f;
    const LevelMap m = makeOpenMap();
    f.build(m);
    const Vec2 s = f.sample(m.grid.cellCenter(5, 10));
    const Vec2 d = f.dirAt(5, 10);
    CHECK(s.x == doctest::Approx(d.x));
    CHECK(s.y == doctest::Approx(d.y));
}

TEST_CASE("build is idempotent") {
    FlowField f;
    const LevelMap m = ls::makeM1Map();
    f.build(m);
    const float first = f.costAt(5, 5);
    f.build(m);
    CHECK(f.costAt(5, 5) == doctest::Approx(first));
}
