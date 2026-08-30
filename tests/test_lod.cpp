#include <doctest/doctest.h>
#include "render/Lod.h"
#include "sim/EnemyType.h"

using ls::LodPolicy;
using ls::LodTier;
using ls::Vec2;

TEST_CASE("LOD tier is chosen by local density, not global count") {
    CHECK(ls::tierForLocalCount(1) == LodTier::Full);
    CHECK(ls::tierForLocalCount(2) == LodTier::Silhouette);
    CHECK(ls::tierForLocalCount(5) == LodTier::Silhouette);
    CHECK(ls::tierForLocalCount(6) == LodTier::Shape);
    CHECK(ls::tierForLocalCount(400) == LodTier::Shape);
}

TEST_CASE("an empty or single-occupant cell is fully articulated") {
    // The lone scout on an empty field keeps its legs.
    CHECK(ls::tierForLocalCount(0) == LodTier::Full);
    CHECK(ls::tierForLocalCount(1) == LodTier::Full);
}

TEST_CASE("the tier thresholds are configurable") {
    const LodPolicy aggressive{1, 2};
    CHECK(ls::tierForLocalCount(1, aggressive) == LodTier::Silhouette);
    CHECK(ls::tierForLocalCount(2, aggressive) == LodTier::Shape);
}

TEST_CASE("tier cost is monotonic — more detail is never cheaper") {
    CHECK(ls::trianglesForTier(LodTier::Full) >
          ls::trianglesForTier(LodTier::Silhouette));
    CHECK(ls::trianglesForTier(LodTier::Silhouette) >
          ls::trianglesForTier(LodTier::Shape));
}

TEST_CASE("culling keeps what touches the viewport and drops what cannot") {
    const auto view = ls::viewportRect(1280.0f, 720.0f, 16.0f);
    CHECK(ls::inView(Vec2{640.0f, 360.0f}, view));
    CHECK(ls::inView(Vec2{-8.0f, 360.0f}, view));      // just off-screen, margin
    CHECK(ls::inView(Vec2{1290.0f, 700.0f}, view));
    CHECK_FALSE(ls::inView(Vec2{-40.0f, 360.0f}, view));
    CHECK_FALSE(ls::inView(Vec2{640.0f, 800.0f}, view));
    CHECK_FALSE(ls::inView(Vec2{5000.0f, 5000.0f}, view));
}

TEST_CASE("a census prices the horde without a window") {
    // The visible artifact of the LOD pass is what it submits. Counting it
    // needs no GPU, which is what makes it a benchmark number rather than an
    // impression.
    ls::EnemyPool pool;
    ls::SpatialHash hash{1280.0f, 720.0f, 12.0f, 1024u};

    // A tight knot of twelve inside one cell, plus two loners far apart.
    for (int i = 0; i < 12; ++i) {
        pool.spawn(Vec2{300.0f + static_cast<float>(i) * 0.5f, 300.0f},
                   ls::EnemyType::Grunt);
    }
    pool.spawn(Vec2{100.0f, 100.0f}, ls::EnemyType::Grunt);
    pool.spawn(Vec2{900.0f, 600.0f}, ls::EnemyType::Grunt);
    pool.spawn(Vec2{5000.0f, 5000.0f}, ls::EnemyType::Grunt);   // off-screen
    hash.build(pool.position, pool.count());

    const auto census =
        ls::lodCensus(pool, hash, ls::viewportRect(1280.0f, 720.0f, 24.0f));

    CHECK(census.culled == 1u);
    CHECK(census.drawn == 14u);
    CHECK(census.tier[2] == 12u);   // the knot collapses to one shape each
    CHECK(census.tier[0] == 2u);    // the loners keep their legs
    CHECK(census.triangles == 12u * 1u + 2u * 7u);

    // The whole point: articulating everyone would cost far more.
    CHECK(census.triangles < census.drawn * 7u);
}
