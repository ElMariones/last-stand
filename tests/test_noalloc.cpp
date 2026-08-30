#include <doctest/doctest.h>

#include <cstdlib>
#include <new>

#include "app/Session.h"
#include "gameplay/Level.h"
#include "gameplay/SpawnDirector.h"
#include "sim/World.h"

// GDD 14.3: "zero heap allocation during a battle", asserted rather than
// hoped for. The pools are sized once at construction; the point of this
// test is that they STAY that way — an innocent-looking std::vector in a
// per-tick path is the classic way frame-time variance creeps back in, and
// it will not show up in a mean.
//
// Replacing global operator new affects this whole test binary, so the
// counter only runs while armed, and arming brackets the tick loop alone.

namespace {

bool g_armed = false;
long g_allocations = 0;

struct AllocGuard {
    AllocGuard() { g_allocations = 0; g_armed = true; }
    ~AllocGuard() { g_armed = false; }
    long count() const { return g_allocations; }
};

}  // namespace

void* operator new(size_t size) {
    if (g_armed) ++g_allocations;
    void* p = std::malloc(size == 0u ? 1u : size);
    if (p == nullptr) throw std::bad_alloc();
    return p;
}

void* operator new[](size_t size) { return operator new(size); }

void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }
void operator delete[](void* p, size_t) noexcept { std::free(p); }

TEST_CASE("the allocation counter itself works") {
    // A test that cannot fail is worse than no test: prove the hook fires.
    long seen = 0;
    {
        AllocGuard guard;
        // A direct call, not a new-expression: the compiler is allowed to
        // elide the latter, and in Release it does.
        void* p = ::operator new(static_cast<size_t>(64));
        seen = guard.count();
        ::operator delete(p);
    }
    CHECK(seen == 1);
}

TEST_CASE("a battle tick allocates nothing") {
    const ls::Level level = ls::makeLevel3();
    ls::World world{level.map, 4242u};
    for (const ls::Vec2& p : ls::defaultDeployPositions(level.map, 4)) {
        world.placeTurret(p);
    }
    world.setLevelTotal(level.totalEnemies);
    world.base().maxHealth = 1.0e9f;
    world.base().health = world.base().maxHealth;

    ls::SpawnDirector director;
    const float dt = 1.0f / 60.0f;

    long allocations = 0;
    {
        AllocGuard guard;
        for (int i = 0; i < 3000; ++i) {
            director.update(world, level, dt);
            world.tick(dt);
        }
        allocations = guard.count();
    }

    CHECK(world.enemies().count() > 0u);   // the ticks did real work
    CHECK(allocations == 0);
}

TEST_CASE("a burning, exploding battle still allocates nothing") {
    // The paths most likely to reach for a scratch vector: splash, cluster
    // sub-blasts, cone burn, Ignite spread, and DENSEST's nested query.
    const ls::Level level = ls::makeLevel3();
    ls::World world{level.map, 99u};
    world.setLevelTotal(level.totalEnemies);
    world.base().maxHealth = 1.0e9f;
    world.base().health = world.base().maxHealth;

    const auto spots = ls::defaultDeployPositions(level.map, 4);
    for (size_t i = 0; i < spots.size(); ++i) {
        world.placeTurret(spots[i]);
        ls::Turret& t = world.turrets().back();
        t.mode = ls::TargetingMode::Densest;
        if (i % 2u == 0u) {
            t.kind = ls::TurretKind::Cannon;
            t.range = 260.0f;
            t.damage = 90.0f;
            t.fireInterval = 2.0f;
            t.splashRadius = 45.0f;
            t.knockback = 40.0f;
            t.clusterShot = true;
        } else {
            t.kind = ls::TurretKind::Flamethrower;
            t.range = 120.0f;
            t.fireInterval = 0.4f;
            t.ignite = true;
        }
    }

    ls::SpawnDirector director;
    const float dt = 1.0f / 60.0f;

    long allocations = 0;
    {
        AllocGuard guard;
        for (int i = 0; i < 2500; ++i) {
            director.update(world, level, dt);
            world.tick(dt);
        }
        allocations = guard.count();
    }

    CHECK(world.totalKills() > 0u);        // the turrets actually fired
    CHECK(allocations == 0);
}

TEST_CASE("Session drives a battle without allocating, fx and abilities included") {
    ls::Session session{nullptr};
    session.goMenu();
    // Sector 1: the later ones are locked until they are earned, and the
    // World-driven cases above already cover the tanks-and-flamethrowers mix.
    session.selectLevel(0);
    session.startBattle();
    REQUIRE(session.phase() == ls::Phase::Battle);

    const float dt = 1.0f / 60.0f;
    int ticked = 0;
    long allocations = 0;
    {
        AllocGuard guard;
        for (int i = 0; i < 2000 && session.phase() == ls::Phase::Battle; ++i) {
            // Airstrike is locked without its node, so this walks the guard
            // clause rather than the effect; both paths must stay clean.
            session.fireAirstrike();
            session.overchargeAt(ls::Vec2{600.0f, 360.0f});
            session.updateBattle(dt);
            // The presentation pools — particles, corpses, damage numbers —
            // are fixed-capacity for exactly this reason.
            session.updatePresentation(dt);
            (void)session.takeEvents();
            ++ticked;
        }
        allocations = guard.count();
    }

    CHECK(ticked > 500);             // the battle really ran
    CHECK(session.particles().count() + session.corpses().count() > 0u);
    CHECK(allocations == 0);
}
