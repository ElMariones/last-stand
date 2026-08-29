#include <doctest/doctest.h>
#include "sim/Base.h"
#include "sim/EnemyPool.h"

using ls::Base;
using ls::EnemyPool;
using ls::Vec2;

TEST_CASE("an enemy inside the radius damages the base and despawns") {
    Base b{Vec2{100.0f, 100.0f}, 30.0f, 1000.0f, 1000.0f};
    EnemyPool p;
    p.spawn(Vec2{110.0f, 100.0f}, 250.0f, 0u);

    CHECK(ls::applyArrivals(p, b) == 1u);
    CHECK(b.health == doctest::Approx(750.0f));
    CHECK(p.count() == 0u);
}

TEST_CASE("an enemy outside the radius is untouched") {
    Base b{Vec2{100.0f, 100.0f}, 30.0f, 1000.0f, 1000.0f};
    EnemyPool p;
    p.spawn(Vec2{200.0f, 100.0f}, 250.0f, 0u);

    CHECK(ls::applyArrivals(p, b) == 0u);
    CHECK(b.health == doctest::Approx(1000.0f));
    CHECK(p.count() == 1u);
}

TEST_CASE("damage scales with the arriving enemy's remaining health") {
    Base b{Vec2{0.0f, 0.0f}, 30.0f, 5000.0f, 5000.0f};
    EnemyPool p;
    p.spawn(Vec2{0.0f, 0.0f}, 2000.0f, 2u);

    ls::applyArrivals(p, b);
    CHECK(b.health == doctest::Approx(3000.0f));
}

TEST_CASE("multiple simultaneous arrivals are all processed") {
    Base b{Vec2{0.0f, 0.0f}, 50.0f, 1000.0f, 1000.0f};
    EnemyPool p;
    for (int i = 0; i < 5; ++i) p.spawn(Vec2{0.0f, 0.0f}, 100.0f, 0u);

    CHECK(ls::applyArrivals(p, b) == 5u);
    CHECK(p.count() == 0u);
    CHECK(b.health == doctest::Approx(500.0f));
}

TEST_CASE("mixed arrivals leave the distant enemies alive") {
    Base b{Vec2{0.0f, 0.0f}, 50.0f, 1000.0f, 1000.0f};
    EnemyPool p;
    p.spawn(Vec2{0.0f, 0.0f},     100.0f, 0u);
    p.spawn(Vec2{500.0f, 0.0f},   100.0f, 0u);
    p.spawn(Vec2{10.0f, 10.0f},   100.0f, 0u);
    p.spawn(Vec2{600.0f, 0.0f},   100.0f, 0u);

    CHECK(ls::applyArrivals(p, b) == 2u);
    CHECK(p.count() == 2u);
    for (uint32_t i = 0; i < p.count(); ++i) {
        CHECK(ls::length(p.position[i]) > 100.0f);
    }
}

TEST_CASE("health floors at zero and the base reports destroyed") {
    Base b{Vec2{0.0f, 0.0f}, 30.0f, 100.0f, 100.0f};
    EnemyPool p;
    p.spawn(Vec2{0.0f, 0.0f}, 5000.0f, 0u);

    ls::applyArrivals(p, b);
    CHECK(b.health == doctest::Approx(0.0f));
    CHECK(b.isDestroyed());
}

TEST_CASE("a healthy base does not report destroyed") {
    Base b{Vec2{0.0f, 0.0f}, 30.0f, 1.0f, 100.0f};
    CHECK_FALSE(b.isDestroyed());
}

TEST_CASE("an empty pool is a no-op") {
    Base b{Vec2{0.0f, 0.0f}, 30.0f, 100.0f, 100.0f};
    EnemyPool p;
    CHECK(ls::applyArrivals(p, b) == 0u);
    CHECK(b.health == doctest::Approx(100.0f));
}
