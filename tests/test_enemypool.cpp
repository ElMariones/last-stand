#include <doctest/doctest.h>
#include "sim/EnemyPool.h"

using ls::EnemyPool;
using ls::Vec2;

TEST_CASE("a new pool is empty but fully reserved") {
    EnemyPool p;
    CHECK(p.count() == 0u);
    CHECK(p.position.size() == EnemyPool::kCapacity);
    CHECK(p.health.size() == EnemyPool::kCapacity);
}

TEST_CASE("spawn returns sequential indices and stores the payload") {
    EnemyPool p;
    CHECK(p.spawn(Vec2{1.0f, 2.0f}, 100.0f, 0u) == 0u);
    CHECK(p.spawn(Vec2{3.0f, 4.0f}, 40.0f, 1u) == 1u);
    CHECK(p.count() == 2u);

    CHECK(p.position[0].x == doctest::Approx(1.0f));
    CHECK(p.health[1] == doctest::Approx(40.0f));
    CHECK(p.type[1] == 1u);
}

TEST_CASE("spawn initialises prevPosition to position so interpolation is stable") {
    EnemyPool p;
    p.spawn(Vec2{7.0f, 9.0f}, 100.0f, 0u);
    CHECK(p.prevPosition[0].x == doctest::Approx(7.0f));
    CHECK(p.prevPosition[0].y == doctest::Approx(9.0f));
}

TEST_CASE("spawn zeroes velocity") {
    EnemyPool p;
    p.spawn(Vec2{7.0f, 9.0f}, 100.0f, 0u);
    CHECK(p.velocity[0].x == 0.0f);
    CHECK(p.velocity[0].y == 0.0f);
}

TEST_CASE("kill swap-removes: the last element fills the hole") {
    EnemyPool p;
    p.spawn(Vec2{0.0f, 0.0f}, 10.0f, 0u);
    p.spawn(Vec2{1.0f, 1.0f}, 20.0f, 1u);
    p.spawn(Vec2{2.0f, 2.0f}, 30.0f, 2u);

    p.kill(1u);

    CHECK(p.count() == 2u);
    CHECK(p.health[1] == doctest::Approx(30.0f));
    CHECK(p.type[1] == 2u);
    CHECK(p.position[1].x == doctest::Approx(2.0f));
    CHECK(p.health[0] == doctest::Approx(10.0f));
}

TEST_CASE("killing the last element just shrinks the count") {
    EnemyPool p;
    p.spawn(Vec2{0.0f, 0.0f}, 10.0f, 0u);
    p.spawn(Vec2{1.0f, 1.0f}, 20.0f, 1u);
    p.kill(1u);
    CHECK(p.count() == 1u);
    CHECK(p.health[0] == doctest::Approx(10.0f));
}

TEST_CASE("killing out of range is a no-op rather than corruption") {
    EnemyPool p;
    p.spawn(Vec2{0.0f, 0.0f}, 10.0f, 0u);
    p.kill(5u);
    CHECK(p.count() == 1u);
}

TEST_CASE("clear empties the pool") {
    EnemyPool p;
    for (int i = 0; i < 10; ++i) p.spawn(Vec2{0.0f, 0.0f}, 10.0f, 0u);
    p.clear();
    CHECK(p.count() == 0u);
}

TEST_CASE("spawning past capacity returns kInvalid instead of growing") {
    EnemyPool p;
    for (uint32_t i = 0; i < EnemyPool::kCapacity; ++i) {
        REQUIRE(p.spawn(Vec2{0.0f, 0.0f}, 1.0f, 0u) != EnemyPool::kInvalid);
    }
    CHECK(p.spawn(Vec2{0.0f, 0.0f}, 1.0f, 0u) == EnemyPool::kInvalid);
    CHECK(p.count() == EnemyPool::kCapacity);
    CHECK(p.position.size() == EnemyPool::kCapacity);
}
