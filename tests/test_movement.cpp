#include <doctest/doctest.h>
#include "sim/MovementSystem.h"
#include "sim/LevelMap.h"
#include "sim/EnemyPool.h"
#include "ai/FlowField.h"

using ls::EnemyPool;
using ls::FlowField;
using ls::LevelMap;
using ls::MovementParams;
using ls::Vec2;

namespace {
LevelMap makeOpenMap() {
    LevelMap m;
    m.grid = ls::Grid{20, 20, 10.0f};
    m.walkable.assign(static_cast<size_t>(m.grid.cellCount()), 1u);
    m.baseCell = m.grid.index(19, 10);
    return m;
}
}  // namespace

TEST_CASE("an enemy moves toward the base") {
    const LevelMap m = makeOpenMap();
    FlowField f;
    f.build(m);

    EnemyPool p;
    p.spawn(m.grid.cellCenter(2, 10), 100.0f, 0u);
    const float startX = p.position[0].x;

    MovementParams params;
    for (int i = 0; i < 10; ++i) ls::updateMovement(p, f, 1.0f / 60.0f, params);

    CHECK(p.position[0].x > startX);
}

TEST_CASE("prevPosition captures the position before the move") {
    const LevelMap m = makeOpenMap();
    FlowField f;
    f.build(m);

    EnemyPool p;
    const Vec2 start = m.grid.cellCenter(2, 10);
    p.spawn(start, 100.0f, 0u);

    MovementParams params;
    ls::updateMovement(p, f, 1.0f / 60.0f, params);

    CHECK(p.prevPosition[0].x == doctest::Approx(start.x));
    CHECK(p.position[0].x != doctest::Approx(start.x));
}

TEST_CASE("distant enemies do not push each other") {
    const LevelMap m = makeOpenMap();
    FlowField f;
    f.build(m);

    EnemyPool p;
    p.spawn(m.grid.cellCenter(2, 2), 100.0f, 0u);
    p.spawn(m.grid.cellCenter(2, 18), 100.0f, 0u);

    MovementParams params;
    params.separationRadius = 5.0f;
    ls::updateMovement(p, f, 1.0f / 60.0f, params);

    CHECK(ls::length(p.velocity[0]) == doctest::Approx(params.speed).epsilon(0.01));
}

TEST_CASE("overlapping enemies push apart") {
    const LevelMap m = makeOpenMap();
    FlowField f;
    f.build(m);

    EnemyPool p;
    const Vec2 at = m.grid.cellCenter(5, 10);
    p.spawn(at, 100.0f, 0u);
    p.spawn(at + Vec2{1.0f, 0.0f}, 100.0f, 0u);

    MovementParams params;
    const float before = ls::distanceSq(p.position[0], p.position[1]);
    for (int i = 0; i < 20; ++i) ls::updateMovement(p, f, 1.0f / 60.0f, params);
    const float after = ls::distanceSq(p.position[0], p.position[1]);

    CHECK(after > before);
}

TEST_CASE("perfectly coincident enemies separate deterministically") {
    const LevelMap m = makeOpenMap();
    FlowField f;
    f.build(m);

    EnemyPool p;
    const Vec2 at = m.grid.cellCenter(5, 10);
    p.spawn(at, 100.0f, 0u);
    p.spawn(at, 100.0f, 0u);

    MovementParams params;
    for (int i = 0; i < 20; ++i) ls::updateMovement(p, f, 1.0f / 60.0f, params);

    CHECK(ls::distanceSq(p.position[0], p.position[1]) > 0.01f);
}

TEST_CASE("an enemy on an unreachable cell does not drift") {
    LevelMap m = makeOpenMap();
    m.walkable[static_cast<size_t>(m.grid.index(1, 0))] = 0u;
    m.walkable[static_cast<size_t>(m.grid.index(0, 1))] = 0u;
    m.walkable[static_cast<size_t>(m.grid.index(1, 1))] = 0u;

    FlowField f;
    f.build(m);

    EnemyPool p;
    const Vec2 start = m.grid.cellCenter(0, 0);
    p.spawn(start, 100.0f, 0u);

    MovementParams params;
    for (int i = 0; i < 10; ++i) ls::updateMovement(p, f, 1.0f / 60.0f, params);

    CHECK(p.position[0].x == doctest::Approx(start.x));
    CHECK(p.position[0].y == doctest::Approx(start.y));
}

TEST_CASE("an empty pool is a no-op") {
    const LevelMap m = makeOpenMap();
    FlowField f;
    f.build(m);
    EnemyPool p;
    MovementParams params;
    ls::updateMovement(p, f, 1.0f / 60.0f, params);
    CHECK(p.count() == 0u);
}
