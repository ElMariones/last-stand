#include <doctest/doctest.h>
#include "app/Session.h"

using ls::NodeId;
using ls::Phase;
using ls::Session;
using ls::TargetingMode;

namespace {

// A session with no save path touches no filesystem: load fails, saveNow is a
// no-op, so these tests are hermetic and order-independent.
Session freshSession() { return Session{nullptr}; }

// Runs the current battle to its end state and returns the phase reached.
Phase pump(Session& s, int maxTicks = 20000) {
    for (int i = 0; i < maxTicks && s.phase() == Phase::Battle; ++i) {
        s.updateBattle(1.0f / 60.0f);
    }
    return s.phase();
}

Phase playOut(Session& s) {
    s.startBattle();
    return pump(s);
}

// Grinds retries until the session can afford `target` Scrap. Level 1 with the
// starting loadout is a loss worth ~23 Scrap, exactly as designed (GDD 2:
// losing pays), so a first purchase takes a couple of runs.
void earn(Session& s, uint32_t target) {
    if (s.phase() == Phase::Prepare) playOut(s);
    for (int guard = 0; guard < 20 && s.scrap() < target; ++guard) {
        s.retry();
        pump(s);
    }
}

float firstTurretDamage(const Session& s) {
    REQUIRE(s.world() != nullptr);
    REQUIRE_FALSE(s.world()->turrets().empty());
    return s.world()->turrets().front().damage;
}

}  // namespace

TEST_CASE("a fresh session starts in Prepare with the level 1 loadout") {
    Session s = freshSession();
    CHECK(s.phase() == Phase::Prepare);
    CHECK(s.levelIndex() == 0);
    REQUIRE(s.world() != nullptr);
    CHECK(s.world()->turrets().size() == s.level().map.hardpoints.size());
}

TEST_CASE("a battle runs to an end state and pays out") {
    Session s = freshSession();
    CHECK(playOut(s) == Phase::Report);
    CHECK(s.hasResult());
    CHECK(s.payout().scrap > 0u);
    CHECK(s.scrap() == s.payout().scrap);
}

TEST_CASE("a purchased upgrade applies to the turrets on the very next retry") {
    // Regression: the loadout cached turrets built from the effects in force
    // when they were PLACED, and retry copied that loadout verbatim — so
    // buying Damage and pressing RETRY changed nothing at all. That breaks the
    // one loop the whole game is built on (GDD 3).
    Session s = freshSession();
    const float before = firstTurretDamage(s);

    earn(s, s.tree().cost(NodeId::Damage));
    REQUIRE(s.phase() == Phase::Report);
    s.openTree();
    REQUIRE(s.phase() == Phase::Tree);

    s.buy(NodeId::Damage);
    REQUIRE(s.tree().level(NodeId::Damage) == 1u);

    s.retry();
    CHECK(firstTurretDamage(s) == doctest::Approx(before * 1.20f));
}

TEST_CASE("a respec rolls the turrets back to their unupgraded stats") {
    Session s = freshSession();
    const float before = firstTurretDamage(s);

    earn(s, s.tree().cost(NodeId::Damage));
    s.openTree();
    s.buy(NodeId::Damage);
    REQUIRE(firstTurretDamage(s) == doctest::Approx(before * 1.20f));

    s.respec();
    CHECK(s.tree().level(NodeId::Damage) == 0u);
    CHECK(firstTurretDamage(s) == doctest::Approx(before));
}

TEST_CASE("a targeting choice survives a retry") {
    // Regression: cycleTargeting mutated the live world's turrets, which
    // resetWorld then overwrote from the loadout.
    Session s = freshSession();
    s.cycleTargeting();
    REQUIRE(s.world()->turrets().front().mode == TargetingMode::Closest);

    REQUIRE(playOut(s) == Phase::Report);
    s.retry();
    CHECK(s.world()->turrets().front().mode == TargetingMode::Closest);
}

TEST_CASE("DENSEST is not reachable by cycling until its node is owned") {
    Session s = freshSession();
    for (int i = 0; i < 3; ++i) s.cycleTargeting();   // First -> ... -> Strongest+1
    CHECK(s.world()->turrets().front().mode == TargetingMode::First);
}

TEST_CASE("a level replays from an identical invasion every time") {
    // GDD 4.1: a level is a fixed invasion. Two runs of the same level with
    // the same loadout must produce the same battle, bit for bit.
    Session a = freshSession();
    Session b = freshSession();
    REQUIRE(playOut(a) == Phase::Report);
    REQUIRE(playOut(b) == Phase::Report);
    CHECK(a.result().kills == b.result().kills);
    CHECK(a.world()->stateHash() == b.world()->stateHash());

    // ...including after a retry, which used to reseed from the clear count.
    const uint32_t firstRunKills = a.result().kills;
    a.retry();
    pump(a);
    CHECK(a.result().kills == firstRunKills);
}
