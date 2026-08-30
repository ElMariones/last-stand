#include <doctest/doctest.h>

#include <algorithm>

#include "app/Session.h"

using ls::NodeId;
using ls::Phase;
using ls::Session;
using ls::TargetingMode;

namespace {

// A session with no save path touches no filesystem: load fails, saveNow is a
// no-op, so these tests are hermetic and order-independent. The game opens on
// the title screen from M6, so these walk it to Prepare — the state the loop
// tests are actually about.
Session freshSession() {
    Session s{nullptr};
    s.goMenu();
    s.selectLevel(0);
    return s;
}

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

TEST_CASE("the game opens on the title screen") {
    const Session s{nullptr};
    CHECK(s.phase() == Phase::Title);
    // ...with a world already running behind it, because the title screen's
    // background is a live battle.
    CHECK(s.world() != nullptr);
}

TEST_CASE("selecting a level lands in Prepare with the level 1 loadout") {
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

TEST_CASE("the screen graph is reversible everywhere except battle to report") {
    Session s{nullptr};
    REQUIRE(s.phase() == Phase::Title);

    s.goMenu();
    CHECK(s.phase() == Phase::Menu);

    s.goOptions();
    CHECK(s.phase() == Phase::Options);
    s.resume();                       // options returns whence it came
    CHECK(s.phase() == Phase::Menu);

    s.goLevelSelect();
    CHECK(s.phase() == Phase::LevelSelect);
    s.selectLevel(1);
    CHECK(s.phase() == Phase::Prepare);
    CHECK(s.levelIndex() == 1);

    s.startBattle();
    CHECK(s.phase() == Phase::Battle);

    s.pause();
    CHECK(s.phase() == Phase::Pause);
    // Options is reachable from the pause screen and returns to it.
    s.goOptions();
    s.resume();
    CHECK(s.phase() == Phase::Pause);
    s.resume();
    CHECK(s.phase() == Phase::Battle);
}

TEST_CASE("abandoning a battle pays nothing") {
    Session s = freshSession();
    s.startBattle();
    // Long enough for the horde to reach the guns; nothing dies in the first
    // ten seconds because it takes that long to walk into range.
    for (int i = 0; i < 1500 && s.phase() == Phase::Battle; ++i) {
        s.updateBattle(1.0f / 60.0f);
    }
    REQUIRE(s.world()->totalKills() > 0u);

    s.pause();
    s.abandonBattle();
    CHECK(s.phase() == Phase::Menu);
    CHECK(s.scrap() == 0u);          // quitting is not a strategy
    CHECK_FALSE(s.hasResult());
}

TEST_CASE("a finished battle produces telemetry and a diagnosis") {
    Session s = freshSession();
    REQUIRE(playOut(s) == Phase::Report);

    CHECK(s.telemetry().elapsedSeconds() > 0.0f);
    CHECK(s.telemetry().attribution().total == s.result().kills);
    CHECK(s.failure().suggestionCount > 0);
    CHECK(s.failure().requiredDps > 0u);
}

TEST_CASE("the report reveal completes on its own and can be skipped") {
    Session s = freshSession();
    REQUIRE(playOut(s) == Phase::Report);
    CHECK(s.reportReveal() == doctest::Approx(0.0f));

    s.updatePresentation(0.1f);
    CHECK(s.reportReveal() > 0.0f);
    CHECK(s.reportReveal() < 1.0f);

    s.skipReveal();
    CHECK(s.reportReveal() == doctest::Approx(1.0f));

    // ...and it never overshoots.
    for (int i = 0; i < 100; ++i) s.updatePresentation(0.1f);
    CHECK(s.reportReveal() == doctest::Approx(1.0f));
}

TEST_CASE("settings drive the juice they configure") {
    Session s = freshSession();
    s.settings().shakeScale = 0;
    s.settings().hitstop = false;
    s.applySettings();

    s.juice().onDetonation(20.0f);
    CHECK(s.juice().amplitude() == doctest::Approx(0.0f));
    CHECK_FALSE(s.frozen());

    s.settings().shakeScale = 100;
    s.settings().hitstop = true;
    s.applySettings();
    s.juice().onDetonation(20.0f);
    CHECK(s.juice().amplitude() > 0.0f);
    CHECK(s.frozen());
}

TEST_CASE("a battle emits the events the audio engine listens for") {
    Session s = freshSession();
    s.startBattle();
    ls::FrameEvents total;
    // The base holds for roughly 45 seconds now, so give the loop room to
    // actually reach the end of the battle.
    for (int i = 0; i < 6000 && s.phase() == Phase::Battle; ++i) {
        s.updateBattle(1.0f / 60.0f);
        const ls::FrameEvents ev = s.takeEvents();
        total.kills += ev.kills;
        total.arrivals += ev.arrivals;
        total.gunShots += ev.gunShots;
        if (ev.battleEnded) total.battleEnded = true;
    }
    CHECK(total.kills > 0u);
    CHECK(total.gunShots > 0u);
    CHECK(total.arrivals > 0u);
    CHECK(total.battleEnded);
    // Draining is destructive: a second read reports nothing.
    CHECK(s.takeEvents().kills == 0u);
}

TEST_CASE("corpses and particles appear where enemies die") {
    Session s = freshSession();
    s.startBattle();
    uint32_t peakParticles = 0u;
    uint32_t peakCorpses = 0u;
    for (int i = 0; i < 1800 && s.phase() == Phase::Battle; ++i) {
        s.updateBattle(1.0f / 60.0f);
        s.updatePresentation(1.0f / 60.0f);
        // Sparks live a third of a second, so the peak is what matters, not
        // whatever happens to be alive when the battle ends.
        peakParticles = std::max(peakParticles, s.particles().count());
        peakCorpses = std::max(peakCorpses, s.corpses().count());
    }
    CHECK(peakCorpses > 0u);
    CHECK(peakParticles > 0u);
    CHECK(peakParticles <= ls::ParticlePool::kCapacity);
}

TEST_CASE("clicking a hardpoint always visibly does something") {
    // The reported bug: every slot arrived holding a Machine Gun, the other
    // kinds were locked, and a click replaced a Machine Gun with an identical
    // Machine Gun. The player saw a dead mouse button.
    Session s = freshSession();
    REQUIRE(s.hardpointCount() == 4);
    REQUIRE(s.turretCount() == 4);

    const ls::Vec2 hp = s.hardpointAt(0);
    REQUIRE(s.turretAtHardpoint(0) >= 0);

    // Same kind on an occupied slot clears it...
    s.toggleTurretAt(hp, 28.0f);
    CHECK(s.turretAtHardpoint(0) < 0);
    CHECK(s.turretCount() == 3);

    // ...and clicking again puts it back.
    s.toggleTurretAt(hp, 28.0f);
    CHECK(s.turretAtHardpoint(0) >= 0);
    CHECK(s.turretCount() == 4);
}

TEST_CASE("a click that misses every hardpoint changes nothing") {
    Session s = freshSession();
    const int before = s.turretCount();
    s.toggleTurretAt(ls::Vec2{5.0f, 5.0f}, 28.0f);
    CHECK(s.turretCount() == before);
}

TEST_CASE("right-click clears a slot and does nothing to an empty one") {
    Session s = freshSession();
    const ls::Vec2 hp = s.hardpointAt(1);
    s.removeTurretAt(hp, 28.0f);
    CHECK(s.turretAtHardpoint(1) < 0);
    s.removeTurretAt(hp, 28.0f);          // idempotent
    CHECK(s.turretAtHardpoint(1) < 0);
}

TEST_CASE("fill and clear cover every slot") {
    Session s = freshSession();
    s.clearLoadout();
    CHECK(s.turretCount() == 0);

    s.fillEmptyHardpoints();
    CHECK(s.turretCount() == s.hardpointCount());
    for (int i = 0; i < s.hardpointCount(); ++i) {
        CHECK(s.turretAtHardpoint(i) >= 0);
    }
    // Filling an already-full board is not a way to exceed the slot count.
    s.fillEmptyHardpoints();
    CHECK(s.turretCount() == s.hardpointCount());
}

TEST_CASE("a battle can be started with no turrets at all") {
    // The player is allowed to walk into a sector understaffed. Refusing
    // mid-flow teaches less than letting them watch it fail.
    Session s = freshSession();
    s.clearLoadout();
    s.startBattle();
    CHECK(s.phase() == Phase::Battle);
    for (int i = 0; i < 6000 && s.phase() == Phase::Battle; ++i) {
        s.updateBattle(1.0f / 60.0f);
    }
    CHECK(s.phase() == Phase::Report);
    CHECK(s.result().kills == 0u);
}

TEST_CASE("a locked turret kind cannot be selected or placed") {
    Session s = freshSession();
    REQUIRE_FALSE(s.isKindUnlocked(ls::TurretKind::Cannon));

    s.selectKind(ls::TurretKind::Cannon);
    CHECK(s.selectedKind() == ls::TurretKind::MachineGun);

    s.clearLoadout();
    s.toggleTurretAt(s.hardpointAt(0), 28.0f);
    REQUIRE(s.turretAtHardpoint(0) >= 0);
    CHECK(s.world()->turrets().front().kind == ls::TurretKind::MachineGun);
}

TEST_CASE("NEW GAME erases progress but keeps the player's options") {
    Session s = freshSession();
    s.settings().masterVolume = 33;
    s.settings().uiScale = 125;
    s.applySettings();

    REQUIRE(playOut(s) == Phase::Report);
    REQUIRE(s.scrap() > 0u);
    REQUIRE(s.hasProgress());

    s.newGame();
    CHECK(s.scrap() == 0u);
    CHECK(s.bestKillsFor(0) == 0u);
    CHECK(s.tree().totalSpent() == 0u);
    CHECK_FALSE(s.hasProgress());
    CHECK(s.phase() == Phase::Prepare);
    // Nobody wants their volume reset because they restarted the campaign.
    CHECK(s.settings().masterVolume == 33);
    CHECK(s.settings().uiScale == 125);
}

TEST_CASE("a fresh save has nothing to continue") {
    const Session s{nullptr};
    CHECK_FALSE(s.hasProgress());
}

// ------------------------------------------------------- the arsenal ------

TEST_CASE("a new commander owns four machine guns and nothing else") {
    Session s = freshSession();
    CHECK(s.owned(ls::TurretKind::MachineGun) == 4u);
    CHECK(s.owned(ls::TurretKind::Cannon) == 0u);
    CHECK(s.owned(ls::TurretKind::Flamethrower) == 0u);
    // ...and they open already deployed, so a first-timer sees a defence.
    CHECK(s.turretCount() == 4);
    CHECK(s.available(ls::TurretKind::MachineGun) == 0u);
}

TEST_CASE("turrets go anywhere walkable, not only on the emplacements") {
    Session s = freshSession();
    s.clearLoadout();
    REQUIRE(s.available(ls::TurretKind::MachineGun) == 4u);

    // A patch of open ground nowhere near an authored emplacement.
    const ls::Vec2 open = s.level().map.grid.cellCenter(40, 30);
    REQUIRE(s.canPlaceAt(open));
    CHECK(s.placeTurretAt(open));
    CHECK(s.turretCount() == 1);
    CHECK(s.available(ls::TurretKind::MachineGun) == 3u);
}

TEST_CASE("placement refuses walls, the base and other turrets") {
    Session s = freshSession();
    s.clearLoadout();

    // Inside one of the wall blocks.
    CHECK(s.placementAt(s.level().map.grid.cellCenter(25, 8)) ==
          Session::Placement::OnWall);
    // On top of the base.
    CHECK(s.placementAt(s.level().map.baseCenter()) ==
          Session::Placement::TooCloseToBase);
    // Off the map entirely.
    CHECK(s.placementAt(ls::Vec2{-50.0f, -50.0f}) ==
          Session::Placement::OffMap);

    const ls::Vec2 spot = s.level().map.grid.cellCenter(40, 30);
    REQUIRE(s.placeTurretAt(spot));
    CHECK(s.placementAt(ls::Vec2{spot.x + 4.0f, spot.y}) ==
          Session::Placement::TooCloseToTurret);
}

TEST_CASE("you cannot deploy what you do not own") {
    Session s = freshSession();
    REQUIRE(s.available(ls::TurretKind::MachineGun) == 0u);   // all deployed
    const ls::Vec2 open = s.level().map.grid.cellCenter(40, 30);
    CHECK_FALSE(s.placeTurretAt(open));
    CHECK(s.turretCount() == 4);
}

TEST_CASE("dragging moves a turret, and an illegal drop is refused") {
    Session s = freshSession();
    const ls::Vec2 from = s.loadout().front().position;
    const ls::Vec2 to = s.level().map.grid.cellCenter(40, 30);

    REQUIRE(s.moveTurret(0, to));
    CHECK(s.loadout().front().position.x == doctest::Approx(to.x));

    // Into a wall: refused, and the turret stays where it was.
    CHECK_FALSE(s.moveTurret(0, s.level().map.grid.cellCenter(25, 8)));
    CHECK(s.loadout().front().position.x == doctest::Approx(to.x));
    CHECK(from.x != doctest::Approx(to.x));
}

TEST_CASE("recalling a turret puts it back in the crate") {
    Session s = freshSession();
    s.recallTurret(0);
    CHECK(s.turretCount() == 3);
    CHECK(s.available(ls::TurretKind::MachineGun) == 1u);
}

TEST_CASE("buying a turret costs Scrap and rises in price") {
    Session s = freshSession();
    REQUIRE(playOut(s) == Phase::Report);
    s.backToPrepare();

    // Give the run enough Scrap by grinding a few more.
    for (int i = 0; i < 12 && !s.canAffordTurret(ls::TurretKind::MachineGun); ++i) {
        s.startBattle();
        pump(s);
        s.backToPrepare();
    }
    REQUIRE(s.canAffordTurret(ls::TurretKind::MachineGun));

    const uint32_t price = s.turretPrice(ls::TurretKind::MachineGun);
    const uint32_t scrapBefore = s.scrap();
    REQUIRE(s.buyTurret(ls::TurretKind::MachineGun));

    CHECK(s.owned(ls::TurretKind::MachineGun) == 5u);
    CHECK(s.available(ls::TurretKind::MachineGun) == 1u);
    CHECK(s.scrap() == scrapBefore - price);
    CHECK(s.turretPrice(ls::TurretKind::MachineGun) > price);
    CHECK(s.stats().turretsBought == 1u);
}

TEST_CASE("a locked kind cannot be bought") {
    Session s = freshSession();
    CHECK_FALSE(s.canAffordTurret(ls::TurretKind::Cannon));
    CHECK_FALSE(s.buyTurret(ls::TurretKind::Cannon));
    CHECK(s.owned(ls::TurretKind::Cannon) == 0u);
}

TEST_CASE("auto-deploy puts the whole reserve on the field") {
    Session s = freshSession();
    s.clearLoadout();
    REQUIRE(s.turretCount() == 0);

    s.autoDeploy();
    CHECK(s.turretCount() == 4);
    CHECK(s.available(ls::TurretKind::MachineGun) == 0u);
    // Every one of them landed somewhere legal.
    for (int i = 0; i < static_cast<int>(s.loadout().size()); ++i) {
        CHECK(s.placementAt(s.loadout()[static_cast<size_t>(i)].position, i) ==
              Session::Placement::Ok);
    }
}

TEST_CASE("the speed control runs 1 to 4 and wraps") {
    Session s = freshSession();
    CHECK(s.timeScale() == 1);
    for (int expected : {2, 3, 4, 1}) {
        s.cycleTimeScale();
        CHECK(s.timeScale() == expected);
    }
    s.setTimeScale(3);
    CHECK(s.timeScale() == 3);
    s.setTimeScale(99);
    CHECK(s.timeScale() == 4);
    s.setTimeScale(-2);
    CHECK(s.timeScale() == 1);
}

TEST_CASE("lifetime stats accumulate across runs") {
    Session s = freshSession();
    CHECK(s.stats().runs == 0u);

    REQUIRE(playOut(s) == Phase::Report);
    const uint32_t firstKills = s.result().kills;
    CHECK(s.stats().runs == 1u);
    CHECK(s.stats().kills == firstKills);
    CHECK(s.stats().bestRunKills == firstKills);
    CHECK(s.stats().scrapEarned == s.payout().scrap);
    CHECK(s.stats().secondsPlayed > 0u);

    s.retry();
    pump(s);
    CHECK(s.stats().runs == 2u);
    CHECK(s.stats().kills == firstKills * 2u);
    CHECK(s.stats().winRate() >= 0.0f);
    CHECK(s.stats().winRate() <= 1.0f);
}
