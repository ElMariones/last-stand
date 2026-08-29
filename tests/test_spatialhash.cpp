#include <doctest/doctest.h>
#include "sim/SpatialHash.h"

#include <cmath>
#include <vector>

using ls::SpatialHash;
using ls::Vec2;

namespace {
// True if `want` appears in the query's indices.
bool found(const ls::SpatialQuery& q, uint32_t want) {
    for (uint32_t i = 0; i < q.count; ++i) {
        if (q.indices[i] == want) return true;
    }
    return false;
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

    const auto q = h.query(p, Vec2{10.0f, 10.0f}, 100.0f);
    REQUIRE(q.count == 1u);
    CHECK(q.indices[0] == 0u);
}

TEST_CASE("an entity beyond radius is not found") {
    SpatialHash h{1000.0f, 1000.0f, 50.0f, 1024u};
    std::vector<Vec2> p{{0.0f, 0.0f}, {500.0f, 0.0f}};
    h.build(p, 2u);

    const auto q = h.query(p, Vec2{0.0f, 0.0f}, 50.0f);
    REQUIRE(q.count == 1u);
    CHECK(q.indices[0] == 0u);
}

TEST_CASE("query respects cell boundaries") {
    SpatialHash h{1000.0f, 1000.0f, 64.0f, 1024u};
    // Entity just inside a cell relative to a probe just outside the next.
    std::vector<Vec2> p{{128.0f, 64.0f}};   // cell (2,1)
    h.build(p, 1u);

    // Probe centered in a neighbouring cell, radius not reaching (128,64).
    const auto far = h.query(p, Vec2{200.0f, 0.0f}, 30.0f);
    CHECK(far.count == 0u);

    const auto near = h.query(p, Vec2{160.0f, 64.0f}, 40.0f);
    CHECK(near.count == 1u);
}

TEST_CASE("clamped off-world entities are not dropped or mis-indexed") {
    SpatialHash h{1000.0f, 1000.0f, 50.0f, 1024u};
    std::vector<Vec2> p{{-100.0f, -100.0f}, {5000.0f, 5000.0f}};
    h.build(p, 2u);   // must not corrupt indices via a negative cell

    // A radius covering both true positions finds both: the clamp only
    // places them in the edge cell, it does not drop them from the set.
    const auto q = h.query(p, Vec2{500.0f, 500.0f}, 100000.0f);
    CHECK(q.count == 2u);
    CHECK(found(q, 0u));
    CHECK(found(q, 1u));
}

TEST_CASE("a query far outside the world returns empty") {
    SpatialHash h{1000.0f, 1000.0f, 50.0f, 1024u};
    std::vector<Vec2> p{{100.0f, 100.0f}};
    h.build(p, 1u);

    const auto q = h.query(p, Vec2{5000.0f, 5000.0f}, 50.0f);
    CHECK(q.count == 0u);
}

TEST_CASE("build is stable across an identical rebuild") {
    SpatialHash h{1000.0f, 1000.0f, 64.0f, 1024u};
    std::vector<Vec2> p{{10.0f, 10.0f}, {90.0f, 90.0f}, {200.0f, 30.0f}};
    h.build(p, 3u);
    const auto a = h.query(p, Vec2{100.0f, 100.0f}, 100.0f);
    h.build(p, 3u);
    const auto b = h.query(p, Vec2{100.0f, 100.0f}, 100.0f);
    REQUIRE(a.count == b.count);
    for (uint32_t i = 0; i < a.count; ++i) CHECK(a.indices[i] == b.indices[i]);
}
