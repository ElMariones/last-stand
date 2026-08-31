#include <doctest/doctest.h>

#include <cstdio>

#include "gameplay/Level.h"
#include "gameplay/SpawnDirector.h"
#include "sim/World.h"

// The determinism regression test (GDD 14.9). Each scenario runs a fixed
// number of ticks from a fixed seed and is checked against a hash committed
// here. This is the single test that protects replay, reproducible bug
// reports and headless benchmarking — everything downstream of "the same
// seed produces the same battle" — and unlike the self-comparison tests in
// test_determinism.cpp it also catches a change that is deterministic but
// WRONG: a retuned constant, a reordered system, an accidental behaviour
// change inside an optimisation.
//
// It is portable because the simulation avoids every documented source of
// float divergence: a fixed timestep, an explicit PCG32, no unordered
// container iteration, no wall-clock, and -ffp-contract=off so the compiler
// cannot fuse a multiply-add in one build type and not another. If it ever
// fails on a new toolchain rather than on a code change, that is a finding,
// not a nuisance: re-bless the constant in its own commit, with the
// divergence explained in the message.

namespace {

// Four fixed emplacements in world units, chosen once and never derived from
// anything the game might retune.
constexpr ls::Vec2 kGoldenTurretSpots[4] = {
    {690.0f, 250.0f}, {690.0f, 450.0f}, {930.0f, 210.0f}, {930.0f, 530.0f},
};

uint64_t report(const char* name, uint64_t actual, uint64_t expected) {
    if (actual != expected) {
        std::printf("GOLDEN %s: expected %llu, got %llu\n", name,
                    static_cast<unsigned long long>(expected),
                    static_cast<unsigned long long>(actual));
    }
    return actual;
}

// Pure movement and separation: no turrets, no combat, 2,000 enemies pushing
// through the chokepoint for 1,000 ticks.
uint64_t hordeOnly() {
    ls::World w{ls::makeM1Map(), 20260829u};
    w.spawnWave(2000u, ls::EnemyType::Grunt);
    for (int i = 0; i < 1000; ++i) w.tick(1.0f / 60.0f);
    return w.stateHash();
}

// The whole Level 1 battle, spawn curve and all four machine guns.
uint64_t level1Battle() {
    const ls::Level level = ls::makeLevel1();
    ls::World w{level.map, 0x5EEDu};
    // Hardcoded, deliberately. Asking defaultDeployPositions where to stand
    // would make these hashes move whenever that arrangement is tuned — and
    // a golden hash that moves for a convenience tweak stops meaning
    // anything. Only a simulation change may move them.
    for (const ls::Vec2& p : kGoldenTurretSpots) w.placeTurret(p);
    w.setLevelTotal(level.totalEnemies);

    ls::SpawnDirector director;
    for (int i = 0; i < 3000; ++i) {
        director.update(w, level, 1.0f / 60.0f);
        w.tick(1.0f / 60.0f);
    }
    return w.stateHash();
}

// Level 3's mixed roster against a build that exercises every damage path:
// splash with knockback and cluster, a burning cone with Ignite spreading,
// and armour piercing against the Tanks.
uint64_t level3MixedBuild() {
    const ls::Level level = ls::makeLevel3();
    ls::World w{level.map, 0x5EEFu};
    w.setLevelTotal(level.totalEnemies);

    for (size_t i = 0; i < 4u; ++i) {
        w.placeTurret(kGoldenTurretSpots[i]);
        ls::Turret& t = w.turrets().back();
        t.armorPierce = 1.5f;
        if (i % 2u == 0u) {
            t.kind = ls::TurretKind::Cannon;
            t.mode = ls::TargetingMode::Densest;
            t.range = 260.0f;
            t.damage = 90.0f;
            t.fireInterval = 2.0f;
            t.splashRadius = 45.0f;
            t.knockback = 40.0f;
            t.clusterShot = true;
        } else {
            t.kind = ls::TurretKind::Flamethrower;
            t.mode = ls::TargetingMode::Densest;
            t.range = 120.0f;
            t.damage = 0.0f;
            t.fireInterval = 0.4f;
            t.burnPerHit = 6.0f;
            t.burnDuration = 3.0f;
            t.ignite = true;
        }
    }

    ls::SpawnDirector director;
    for (int i = 0; i < 2000; ++i) {
        director.update(w, level, 1.0f / 60.0f);
        w.tick(1.0f / 60.0f);
    }
    return w.stateHash();
}

}  // namespace

// Recorded on macOS 15 / arm64 / AppleClang, Release AND Debug (they agree,
// which is the point of -ffp-contract=off).
//
// Re-blessed twice, both times for a simulation change and never for a
// cosmetic one — which is the whole point of these constants.
//   1. The economy fix doubled base HP from 1,000 to 2,000.
//   2. Movement now collides with walls, and sector 3 is a different map.
//      Both are simulation changes; both move every hash.
// The scenarios pin their turret positions by hand so that tuning where a
// default defence stands can never be mistaken for a simulation change.
constexpr uint64_t kGoldenHorde   = 13318753716922102779ull;
constexpr uint64_t kGoldenLevel1  = 672292474305532204ull;
constexpr uint64_t kGoldenLevel3  = 13622442721687783473ull;

TEST_CASE("golden: 2,000 grunts, 1,000 ticks, movement and separation only") {
    CHECK(report("hordeOnly", hordeOnly(), kGoldenHorde) == kGoldenHorde);
}

TEST_CASE("golden: Level 1, four machine guns, 3,000 ticks") {
    CHECK(report("level1Battle", level1Battle(), kGoldenLevel1) == kGoldenLevel1);
}

TEST_CASE("golden: Level 3, cannon + flamethrower build, 2,000 ticks") {
    CHECK(report("level3MixedBuild", level3MixedBuild(), kGoldenLevel3) ==
          kGoldenLevel3);
}
