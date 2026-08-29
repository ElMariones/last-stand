#include <doctest/doctest.h>
#include "math/Rect.h"

using ls::Rect;
using ls::Vec2;

TEST_CASE("contains includes the boundary but not the outside") {
    const Rect r{{0.0f, 0.0f}, {10.0f, 10.0f}};
    CHECK(ls::contains(r, Vec2{0.0f, 0.0f}));    // corner
    CHECK(ls::contains(r, Vec2{10.0f, 10.0f}));  // far corner
    CHECK(ls::contains(r, Vec2{5.0f, 5.0f}));
    CHECK_FALSE(ls::contains(r, Vec2{10.0f + 0.001f, 5.0f}));
    CHECK_FALSE(ls::contains(r, Vec2{5.0f, -0.001f}));
}

TEST_CASE("fromCenter produces the correct extents") {
    const Rect r = ls::fromCenter(Vec2{100.0f, 200.0f}, 30.0f, 40.0f);
    CHECK(r.min.x == doctest::Approx(70.0f));
    CHECK(r.min.y == doctest::Approx(160.0f));
    CHECK(r.max.x == doctest::Approx(130.0f));
    CHECK(r.max.y == doctest::Approx(240.0f));
}

TEST_CASE("width and height are correct") {
    CHECK(ls::width(Rect{{1.0f, 2.0f}, {6.0f, 10.0f}}) == doctest::Approx(5.0f));
    CHECK(ls::height(Rect{{1.0f, 2.0f}, {6.0f, 10.0f}}) == doctest::Approx(8.0f));
}
