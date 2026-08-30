#include <doctest/doctest.h>
#include "sim/SpatialHash.h"

#include <algorithm>
#include <cmath>
#include <vector>

using ls::SpatialHash;
using ls::Vec2;

namespace {

// Collects a radius query into a plain vector, preserving visit order.
std::vector<uint32_t> collect(const SpatialHash& h,
                              const std::vector<Vec2>& p, Vec2 c, float r) {
    std::vector<uint32_t> out;
    h.forEachInRadius(p, c, r, [&](uint32_t i) { out.push_back(i); });
    return out;
}

bool found(const std::vector<uint32_t>& v, uint32_t want) {
    return std::find(v.begin(), v.end(), want) != v.end();
}

}  // namespace

TEST_CASE("spatial hash dimensions") {
    SpatialHash h{1280.0f, 720.0f, 64.0f, 1024u};
    CHECK(h.cols() == 20);
    CHECK(h.rows() == 12);          // ceil(720/64) == 12
    CHECK(h.cellSize() == doctest::Approx(64.0f));
}

TEST_CASE("an entity at the origin is found within range") {
    SpatialHash h{1000.0f, 1000.0f, 50.0f, 1024u};
    std::vector<Vec2> p{{10.0f, 10.0f}};
    h.build(p, 1u);

    const auto q = collect(h, p, Vec2{10.0f, 10.0f}, 100.0f);
    REQUIRE(q.size() == 1u);
    CHECK(q[0] == 0u);
}

TEST_CASE("an entity beyond radius is not found") {
    SpatialHash h{1000.0f, 1000.0f, 50.0f, 1024u};
    std::vector<Vec2> p{{0.0f, 0.0f}, {500.0f, 0.0f}};
    h.build(p, 2u);

    const auto q = collect(h, p, Vec2{0.0f, 0.0f}, 50.0f);
    REQUIRE(q.size() == 1u);
    CHECK(q[0] == 0u);
}

TEST_CASE("query respects cell boundaries") {
    SpatialHash h{1000.0f, 1000.0f, 64.0f, 1024u};
    // Entity just inside a cell relative to a probe just outside the next.
    std::vector<Vec2> p{{128.0f, 64.0f}};   // cell (2,1)
    h.build(p, 1u);

    // Probe centered in a neighbouring cell, radius not reaching (128,64).
    CHECK(collect(h, p, Vec2{200.0f, 0.0f}, 30.0f).empty());
    CHECK(collect(h, p, Vec2{160.0f, 64.0f}, 40.0f).size() == 1u);
}

TEST_CASE("clamped off-world entities are not dropped or mis-indexed") {
    SpatialHash h{1000.0f, 1000.0f, 50.0f, 1024u};
    std::vector<Vec2> p{{-100.0f, -100.0f}, {5000.0f, 5000.0f}};
    h.build(p, 2u);   // must not corrupt indices via a negative cell

    // A radius covering both true positions finds both: the clamp only
    // places them in the edge cell, it does not drop them from the set.
    const auto q = collect(h, p, Vec2{500.0f, 500.0f}, 100000.0f);
    CHECK(q.size() == 2u);
    CHECK(found(q, 0u));
    CHECK(found(q, 1u));
}

TEST_CASE("a query far outside the world returns empty") {
    SpatialHash h{1000.0f, 1000.0f, 50.0f, 1024u};
    std::vector<Vec2> p{{100.0f, 100.0f}};
    h.build(p, 1u);

    CHECK(collect(h, p, Vec2{5000.0f, 5000.0f}, 50.0f).empty());
}

TEST_CASE("build is stable across an identical rebuild") {
    SpatialHash h{1000.0f, 1000.0f, 64.0f, 1024u};
    std::vector<Vec2> p{{10.0f, 10.0f}, {90.0f, 90.0f}, {200.0f, 30.0f}};
    h.build(p, 3u);
    const auto a = collect(h, p, Vec2{100.0f, 100.0f}, 100.0f);
    h.build(p, 3u);
    const auto b = collect(h, p, Vec2{100.0f, 100.0f}, 100.0f);
    CHECK(a == b);
}

TEST_CASE("a nested query does not disturb the enclosing one") {
    // Regression: the old buffer-returning query() shared one result buffer,
    // so an inner query silently overwrote the outer query's index list —
    // which is exactly what DENSEST targeting does on every shot.
    SpatialHash h{1000.0f, 1000.0f, 50.0f, 1024u};
    std::vector<Vec2> p;
    for (int i = 0; i < 40; ++i) {
        p.push_back(Vec2{static_cast<float>(10 + i * 5), 100.0f});
    }
    h.build(p, static_cast<uint32_t>(p.size()));

    const auto expected = collect(h, p, Vec2{100.0f, 100.0f}, 90.0f);
    REQUIRE(expected.size() > 4u);

    std::vector<uint32_t> observed;
    h.forEachInRadius(p, Vec2{100.0f, 100.0f}, 90.0f, [&](uint32_t i) {
        int inner = 0;
        h.forEachInRadius(p, p[i], 40.0f, [&](uint32_t) { ++inner; });
        CHECK(inner > 0);
        observed.push_back(i);
    });
    CHECK(observed == expected);
}

TEST_CASE("cell occupancy is exposed for density-driven LOD") {
    SpatialHash h{1000.0f, 1000.0f, 50.0f, 1024u};
    std::vector<Vec2> p{{10.0f, 10.0f}, {20.0f, 20.0f}, {400.0f, 400.0f}};
    h.build(p, 3u);

    CHECK(h.countInCell(h.cellOfPosition(Vec2{15.0f, 15.0f})) == 2);
    CHECK(h.countInCell(h.cellOfPosition(Vec2{400.0f, 400.0f})) == 1);
    CHECK(h.countInCell(h.cellOfPosition(Vec2{900.0f, 900.0f})) == 0);
}
