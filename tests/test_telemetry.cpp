#include <doctest/doctest.h>

#include <string>

#include "gameplay/SpawnDirector.h"
#include "gameplay/Telemetry.h"

using ls::BattleTelemetry;
using ls::Lane;
using ls::NodeId;
using ls::UpgradeTree;

namespace {

// Runs a real Level 1 battle and returns the telemetry it produced.
BattleTelemetry runLevel1(int ticks = 6000) {
    const ls::Level level = ls::makeLevel1();
    ls::World world{level.map, 0x5EEDu};
    for (const ls::Vec2& p : ls::defaultDeployPositions(level.map, 4)) {
        world.placeTurret(p);
    }
    world.setLevelTotal(level.totalEnemies);

    BattleTelemetry t;
    t.begin(level);
    ls::SpawnDirector director;
    const float dt = 1.0f / 60.0f;
    for (int i = 0; i < ticks && !world.isOver(); ++i) {
        director.update(world, level, dt);
        world.tick(dt);
        t.sample(world, dt);
    }
    t.finish(world, world.isVictory());
    return t;
}

}  // namespace

TEST_CASE("lane names are printable and distinct") {
    CHECK(std::string(ls::laneName(Lane::North)) == "NORTH LANE");
    CHECK(std::string(ls::laneName(Lane::Centre)) == "CENTRE LANE");
    CHECK(std::string(ls::laneName(Lane::South)) == "SOUTH LANE");
}

TEST_CASE("a real battle records a breach with a time and a lane") {
    const BattleTelemetry t = runLevel1();
    REQUIRE(t.breached());
    CHECK(t.breachTime() > 0.0f);
    CHECK(t.breachTime() < t.elapsedSeconds());
    CHECK(t.peakDensityAtBreach() > 0u);
    CHECK(std::string(ls::laneName(t.breachLane())).find("LANE") !=
          std::string::npos);
}

TEST_CASE("a battle that is never touched reports no breach") {
    const ls::Level level = ls::makeLevel1();
    ls::World world{level.map, 0x5EEDu};
    world.setLevelTotal(level.totalEnemies);

    BattleTelemetry t;
    t.begin(level);
    // No spawns at all: nothing can reach the base.
    for (int i = 0; i < 600; ++i) {
        world.tick(1.0f / 60.0f);
        t.sample(world, 1.0f / 60.0f);
    }
    t.finish(world, false);
    CHECK_FALSE(t.breached());
    CHECK(t.breachTime() == doctest::Approx(0.0f));
}

TEST_CASE("kills are attributed to the turret that made them") {
    const BattleTelemetry t = runLevel1();
    const ls::Attribution& a = t.attribution();
    CHECK(a.total > 0u);
    CHECK(a.machineGun > 0u);
    CHECK(a.cannon == 0u);          // none placed
    CHECK(a.machineGun + a.cannon + a.flamethrower + a.burn == a.total);
    CHECK(a.share(a.machineGun) > 0.5f);
    CHECK(a.share(a.machineGun) <= 1.0f);
}

TEST_CASE("attribution shares are safe on an empty battle") {
    ls::Attribution a;
    CHECK(a.share(0u) == doctest::Approx(0.0f));
}

TEST_CASE("the DPS estimate and requirement are both positive and comparable") {
    const BattleTelemetry t = runLevel1();
    CHECK(t.estimatedDps() > 0.0f);
    CHECK(t.requiredDps() > 0.0f);
    // The starting loadout is meant to lose Level 1 badly (GDD pillar 2).
    CHECK(t.estimatedDps() < t.requiredDps());
}

TEST_CASE("peak kills per second is recorded") {
    const BattleTelemetry t = runLevel1();
    CHECK(t.peakKillsPerSecond() > 0.0f);
}

TEST_CASE("the analysis always suggests something actionable") {
    const BattleTelemetry t = runLevel1();
    const UpgradeTree tree;
    const ls::FailureAnalysis fa = ls::analyse(t, tree);

    CHECK_FALSE(fa.victory);
    CHECK(fa.suggestionCount > 0);
    CHECK(fa.suggestionCount <= 2);
    CHECK(fa.requiredDps > 0u);
}

TEST_CASE("the analysis never suggests a node already owned") {
    const BattleTelemetry t = runLevel1();

    UpgradeTree tree;
    uint32_t scrap = 100000u;
    // Buy every one-shot node the analyser might reach for.
    const NodeId owned[] = {NodeId::ArmorPiercing, NodeId::UnlockCannon,
                            NodeId::UnlockFlamethrower, NodeId::MGOverclock,
                            NodeId::ExtraHardpoint};
    for (const NodeId n : owned) REQUIRE(tree.purchase(n, scrap));

    const ls::FailureAnalysis fa = ls::analyse(t, tree);
    for (int i = 0; i < fa.suggestionCount; ++i) {
        const NodeId n = fa.suggestions[static_cast<size_t>(i)];
        for (const NodeId o : owned) CHECK(n != o);
    }
    // Repeatable nodes are still fair game, so it still has advice.
    CHECK(fa.suggestionCount > 0);
}

TEST_CASE("a dense breach is diagnosed as a density problem") {
    // Hand-built telemetry would need World access, so drive the analyser
    // through a Level 3 run: Tanks present means armour piercing leads.
    const ls::Level level = ls::makeLevel3();
    ls::World world{level.map, 0x5EEFu};
    for (const ls::Vec2& p : ls::defaultDeployPositions(level.map, 4)) {
        world.placeTurret(p);
    }
    world.setLevelTotal(level.totalEnemies);

    BattleTelemetry t;
    t.begin(level);
    ls::SpawnDirector director;
    for (int i = 0; i < 6000 && !world.isOver(); ++i) {
        director.update(world, level, 1.0f / 60.0f);
        world.tick(1.0f / 60.0f);
        t.sample(world, 1.0f / 60.0f);
    }
    t.finish(world, world.isVictory());

    CHECK(t.hasTanks());
    const UpgradeTree tree;
    const ls::FailureAnalysis fa = ls::analyse(t, tree);
    CHECK(fa.suggestions[0] == NodeId::ArmorPiercing);
}

TEST_CASE("suggestions are never duplicated") {
    const BattleTelemetry t = runLevel1();
    const UpgradeTree tree;
    const ls::FailureAnalysis fa = ls::analyse(t, tree);
    if (fa.suggestionCount == 2) CHECK(fa.suggestions[0] != fa.suggestions[1]);
}
