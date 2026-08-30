#include <doctest/doctest.h>

#include "fx/Corpses.h"
#include "fx/DamageNumbers.h"
#include "fx/Juice.h"
#include "fx/Particles.h"

using ls::CorpseRing;
using ls::DamageNumbers;
using ls::Juice;
using ls::ParticleKind;
using ls::ParticlePool;
using ls::Vec2;

// ---------------------------------------------------------------- juice ----

TEST_CASE("screenshake decays to exactly zero and stays there") {
    Juice j;
    j.onKills(20u);
    REQUIRE(j.amplitude() > 0.0f);
    for (int i = 0; i < 600; ++i) j.update(1.0f / 60.0f);
    CHECK(j.amplitude() == doctest::Approx(0.0f));
    j.update(1.0f / 60.0f);
    CHECK(j.amplitude() == doctest::Approx(0.0f));
}

TEST_CASE("screenshake is hard-clamped so the late game cannot induce nausea") {
    Juice j;
    ls::JuiceParams p;
    j.setParams(p);
    j.onKills(100000u);
    CHECK(j.amplitude() <= p.shakeMax);
}

TEST_CASE("more kills in a tick shake harder") {
    Juice a;
    Juice b;
    a.onKills(1u);
    b.onKills(8u);
    CHECK(b.amplitude() > a.amplitude());
}

TEST_CASE("the shake scale setting reaches zero shake") {
    Juice j;
    j.setScale(0.0f);
    j.onKills(50u);
    CHECK(j.amplitude() == doctest::Approx(0.0f));
    CHECK(j.offset(7u).x == doctest::Approx(0.0f));
}

TEST_CASE("hitstop freezes for its budget, then releases") {
    Juice j;
    ls::JuiceParams p;
    j.onDetonation(4.0f);
    CHECK(j.frozen());

    int frames = 0;
    while (j.frozen() && frames < 1000) {
        j.update(1.0f / 240.0f);
        ++frames;
    }
    CHECK_FALSE(j.frozen());
    // 40 ms at 240 Hz is ten frames, give or take the final partial one.
    CHECK(frames <= static_cast<int>(p.hitstopSeconds * 240.0f) + 2);
}

TEST_CASE("hitstop can be switched off without switching off the shake") {
    Juice j;
    j.setHitstopEnabled(false);
    j.onDetonation(6.0f);
    CHECK_FALSE(j.frozen());
    CHECK(j.amplitude() > 0.0f);
}

TEST_CASE("the shake offset is a pure function of the frame") {
    Juice j;
    j.onKills(10u);
    const Vec2 a = j.offset(42u);
    const Vec2 b = j.offset(42u);
    CHECK(a.x == doctest::Approx(b.x));
    CHECK(a.y == doctest::Approx(b.y));
    CHECK(j.offset(43u).x != doctest::Approx(a.x));
}

// ------------------------------------------------------------ particles ----

TEST_CASE("a burst spawns what it was asked for and expires on schedule") {
    ParticlePool pool;
    ls::Pcg32 rng{1u};
    CHECK(pool.emitBurst(Vec2{10.0f, 10.0f}, Vec2{1.0f, 0.0f}, 12u,
                         ParticleKind::Spark, 80.0f, 0.4f, rng) == 12u);
    CHECK(pool.count() == 12u);

    for (int i = 0; i < 60; ++i) pool.update(1.0f / 60.0f);
    CHECK(pool.count() == 0u);
}

TEST_CASE("the pool never exceeds capacity and reports what it dropped") {
    ParticlePool pool;
    ls::Pcg32 rng{2u};
    const uint32_t spawned =
        pool.emitBurst(Vec2{0.0f, 0.0f}, Vec2{0.0f, 0.0f},
                       ParticlePool::kCapacity + 500u, ParticleKind::Ember,
                       40.0f, 5.0f, rng);
    CHECK(spawned == ParticlePool::kCapacity);
    CHECK(pool.count() == ParticlePool::kCapacity);
    CHECK_FALSE(pool.emitFlash(Vec2{0.0f, 0.0f}, Vec2{1.0f, 0.0f}));
}

TEST_CASE("expiry frees slots for reuse") {
    ParticlePool pool;
    ls::Pcg32 rng{3u};
    pool.emitBurst(Vec2{0.0f, 0.0f}, Vec2{1.0f, 0.0f}, ParticlePool::kCapacity,
                   ParticleKind::Spark, 50.0f, 0.2f, rng);
    for (int i = 0; i < 60; ++i) pool.update(1.0f / 60.0f);
    REQUIRE(pool.count() == 0u);
    CHECK(pool.emitFlash(Vec2{1.0f, 1.0f}, Vec2{1.0f, 0.0f}));
}

TEST_CASE("a Scrap arc actually arrives at the counter") {
    ParticlePool pool;
    const Vec2 from{100.0f, 400.0f};
    const Vec2 to{1200.0f, 40.0f};
    REQUIRE(pool.emitScrapArc(from, to));

    Vec2 last = pool.position[0];
    for (int i = 0; i < 41; ++i) {
        pool.update(1.0f / 60.0f);
        if (pool.count() == 0u) break;
        last = pool.position[0];
    }
    // Within a few pixels of the counter by the time it expires.
    CHECK(ls::distanceSq(last, to) < 40.0f * 40.0f);
}

TEST_CASE("progress runs 0 to 1 across a particle's life") {
    ParticlePool pool;
    ls::Pcg32 rng{4u};
    pool.emitBurst(Vec2{0.0f, 0.0f}, Vec2{1.0f, 0.0f}, 1u, ParticleKind::Smoke,
                   10.0f, 1.0f, rng);
    const float first = pool.progress(0u);
    for (int i = 0; i < 20; ++i) pool.update(1.0f / 60.0f);
    REQUIRE(pool.count() == 1u);
    CHECK(pool.progress(0u) > first);
    CHECK(pool.progress(0u) <= 1.0f);
}

// -------------------------------------------------------------- corpses ----

TEST_CASE("corpses accumulate and then fade out") {
    CorpseRing ring;
    for (int i = 0; i < 10; ++i) {
        ring.add(Vec2{static_cast<float>(i), 0.0f}, Vec2{1.0f, 0.0f}, 0u);
    }
    CHECK(ring.count() == 10u);
    CHECK(ring.fade(ring.indexAt(0u)) == doctest::Approx(0.0f));

    for (int i = 0; i < 60; ++i) ring.update(1.0f / 60.0f);
    CHECK(ring.count() == 10u);
    CHECK(ring.fade(ring.indexAt(0u)) == doctest::Approx(0.5f).epsilon(0.05));

    for (int i = 0; i < 120; ++i) ring.update(1.0f / 60.0f);
    CHECK(ring.count() == 0u);
}

TEST_CASE("the ring forgets the oldest corpse rather than growing") {
    CorpseRing ring;
    for (uint32_t i = 0; i < CorpseRing::kCapacity + 300u; ++i) {
        ring.add(Vec2{static_cast<float>(i), 0.0f}, Vec2{1.0f, 0.0f}, 0u);
    }
    CHECK(ring.count() == CorpseRing::kCapacity);

    // The oldest surviving corpse is #300, not #0.
    CHECK(ring.positionAt(ring.indexAt(0u)).x == doctest::Approx(300.0f));
}

// -------------------------------------------------------- damage numbers ----

TEST_CASE("nearby damage merges into one number instead of stacking") {
    DamageNumbers dn;
    for (int i = 0; i < 40; ++i) dn.add(Vec2{200.0f, 200.0f}, 5.0f);
    CHECK(dn.count() == 1u);
    CHECK(dn.amountAt(0u) == doctest::Approx(200.0f));
}

TEST_CASE("damage far apart stays separate") {
    DamageNumbers dn;
    dn.add(Vec2{100.0f, 100.0f}, 10.0f);
    dn.add(Vec2{600.0f, 400.0f}, 10.0f);
    CHECK(dn.count() == 2u);
}

TEST_CASE("a full board folds new damage in rather than dropping it") {
    DamageNumbers dn;
    float total = 0.0f;
    for (uint32_t i = 0; i < DamageNumbers::kCapacity + 20u; ++i) {
        // Spaced well beyond the merge radius so nothing merges naturally.
        dn.add(Vec2{static_cast<float>(i) * 200.0f, 0.0f}, 3.0f);
        total += 3.0f;
    }
    CHECK(dn.count() == DamageNumbers::kCapacity);

    float carried = 0.0f;
    for (uint32_t i = 0; i < dn.count(); ++i) carried += dn.amountAt(i);
    CHECK(carried == doctest::Approx(total));
}

TEST_CASE("numbers rise as they age and then expire") {
    DamageNumbers dn;
    dn.add(Vec2{300.0f, 300.0f}, 25.0f);
    const float y0 = dn.positionAt(0u).y;
    for (int i = 0; i < 30; ++i) dn.update(1.0f / 60.0f);
    REQUIRE(dn.count() == 1u);
    CHECK(dn.positionAt(0u).y < y0);

    for (int i = 0; i < 60; ++i) dn.update(1.0f / 60.0f);
    CHECK(dn.count() == 0u);
}

TEST_CASE("zero and negative damage is ignored") {
    DamageNumbers dn;
    dn.add(Vec2{0.0f, 0.0f}, 0.0f);
    dn.add(Vec2{0.0f, 0.0f}, -5.0f);
    CHECK(dn.count() == 0u);
}
