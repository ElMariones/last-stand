#include <doctest/doctest.h>
#include "math/Vec2.h"

using ls::Vec2;

TEST_CASE("Vec2 arithmetic") {
    Vec2 a{3.0f, 4.0f};
    Vec2 b{1.0f, 2.0f};

    CHECK((a + b).x == doctest::Approx(4.0f));
    CHECK((a + b).y == doctest::Approx(6.0f));
    CHECK((a - b).x == doctest::Approx(2.0f));
    CHECK((a * 2.0f).y == doctest::Approx(8.0f));
}

TEST_CASE("Vec2 length") {
    Vec2 a{3.0f, 4.0f};
    CHECK(ls::lengthSq(a) == doctest::Approx(25.0f));
    CHECK(ls::length(a) == doctest::Approx(5.0f));
}

TEST_CASE("Vec2 normalized produces a unit vector") {
    Vec2 n = ls::normalized(Vec2{3.0f, 4.0f});
    CHECK(ls::length(n) == doctest::Approx(1.0f));
    CHECK(n.x == doctest::Approx(0.6f));
}

TEST_CASE("Vec2 normalized of zero is zero, not NaN") {
    Vec2 n = ls::normalized(Vec2{0.0f, 0.0f});
    CHECK(n.x == 0.0f);
    CHECK(n.y == 0.0f);
}

TEST_CASE("Vec2 lerp") {
    Vec2 r = ls::lerp(Vec2{0.0f, 0.0f}, Vec2{10.0f, 20.0f}, 0.5f);
    CHECK(r.x == doctest::Approx(5.0f));
    CHECK(r.y == doctest::Approx(10.0f));
}

TEST_CASE("Vec2 distanceSq") {
    CHECK(ls::distanceSq(Vec2{0.0f, 0.0f}, Vec2{3.0f, 4.0f}) == doctest::Approx(25.0f));
}
