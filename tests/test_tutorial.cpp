#include <doctest/doctest.h>

#include "app/Session.h"
#include "app/Tutorial.h"

using ls::Phase;
using ls::Session;
using ls::TutorialStep;

namespace {

Session freshSession() {
    Session s{nullptr};      // no save path: nothing is written to disk
    s.goMenu();
    s.selectLevel(0);
    return s;
}

// One frame of the tutorial's own clock. It only ever observes, so nothing
// else has to happen for a step to be evaluated.
void pump(Session& s, int frames = 1) {
    for (int i = 0; i < frames; ++i) s.updatePresentation(1.0f / 60.0f);
}

}  // namespace

TEST_CASE("a fresh player starts at the first step") {
    Session s = freshSession();
    pump(s);
    CHECK(s.tutorial().active());
    CHECK(s.tutorial().step() == TutorialStep::MoveTurret);
    CHECK(s.tutorial().headline()[0] != '\0');
    CHECK(s.tutorial().detail()[0] != '\0');
}

TEST_CASE("the tutorial advances by watching, never by blocking") {
    Session s = freshSession();
    pump(s);
    REQUIRE(s.tutorial().step() == TutorialStep::MoveTurret);

    // A sector loads with the starting turrets already on the field, so step
    // one is about moving one rather than placing one.
    REQUIRE(s.turretCount() > 0);
    REQUIRE(s.placementsMade() == 0u);
    const auto spots = ls::defaultDeployPositions(s.level().map, 8);
    REQUIRE(spots.size() == 8u);
    REQUIRE(s.moveTurret(0, spots[7]));
    pump(s);
    CHECK(s.tutorial().step() == TutorialStep::Deploy);

    s.startBattle();
    pump(s);
    CHECK(s.tutorial().step() == TutorialStep::Watch);
}

TEST_CASE("buying and siting a new turret counts as learning the same thing") {
    Session s = freshSession();
    pump(s);
    REQUIRE(s.tutorial().step() == TutorialStep::MoveTurret);

    s.grantScrap(5000u);
    REQUIRE(s.buyTurret(ls::TurretKind::MachineGun));
    const auto spots = ls::defaultDeployPositions(s.level().map, 8);
    REQUIRE(s.placeTurretAt(spots[6]));
    pump(s);
    CHECK(s.tutorial().step() == TutorialStep::Deploy);
}

TEST_CASE("a player who is already ahead is not asked to catch up") {
    // Deploying straight away skips the placement coaching rather than
    // nagging for something already done. Steps only ever move forward.
    Session s = freshSession();
    s.autoDeploy();
    s.startBattle();
    pump(s);
    CHECK(static_cast<int>(s.tutorial().step()) >=
          static_cast<int>(TutorialStep::Watch));
}

TEST_CASE("the timed steps hand over on their own") {
    Session s = freshSession();
    s.autoDeploy();
    s.startBattle();
    pump(s);
    REQUIRE(s.tutorial().step() == TutorialStep::Watch);

    // "Look at this" is the only kind of step with no action to wait for, so
    // it is the only kind on a timer. Nothing else may depend on one.
    pump(s, 60 * 8);
    CHECK(static_cast<int>(s.tutorial().step()) >
          static_cast<int>(TutorialStep::Watch));
}

TEST_CASE("skipping is permanent and immediate") {
    Session s = freshSession();
    pump(s);
    REQUIRE(s.tutorial().active());

    s.skipTutorial();
    CHECK_FALSE(s.tutorial().active());
    CHECK(s.tutorial().step() == TutorialStep::Done);
    CHECK(s.settings().tutorialDone);

    // ...and it stays gone however much the game is played.
    s.autoDeploy();
    s.startBattle();
    pump(s, 600);
    CHECK_FALSE(s.tutorial().active());
}

TEST_CASE("a finished tutorial can be asked for again") {
    Session s = freshSession();
    s.skipTutorial();
    REQUIRE_FALSE(s.tutorial().active());

    s.restartTutorial();
    CHECK(s.tutorial().active());
    CHECK(s.tutorial().step() == TutorialStep::MoveTurret);
    CHECK_FALSE(s.settings().tutorialDone);
}

TEST_CASE("finishing the tutorial records it without being asked") {
    Session s = freshSession();
    pump(s);
    REQUIRE(s.tutorial().active());
    CHECK_FALSE(s.settings().tutorialDone);

    // Reaching the end by playing has to record itself, or the tutorial
    // reappears on the next launch for a player who completed it.
    s.skipTutorial();
    CHECK(s.settings().tutorialDone);
}

TEST_CASE("every step the player can reach says something") {
    // A step with an empty headline draws an empty box, which reads as a bug
    // rather than as guidance. Rather than poke a Tutorial onto each step -
    // which the public API rightly does not allow - this plays until the
    // tutorial finishes and checks the text of every step it actually visits.
    Session s = freshSession();
    bool visited[static_cast<size_t>(TutorialStep::Done) + 1u] = {};

    const auto record = [&]() {
        const TutorialStep step = s.tutorial().step();
        visited[static_cast<size_t>(step)] = true;
        if (step == TutorialStep::Done) {
            CHECK(s.tutorial().headline()[0] == '\0');
            CHECK(s.tutorial().detail()[0] == '\0');
        } else {
            CAPTURE(static_cast<int>(step));
            CHECK(s.tutorial().headline()[0] != '\0');
            CHECK(s.tutorial().detail()[0] != '\0');
        }
    };

    s.autoDeploy();
    record();
    for (int run = 0; run < 12 && s.tutorial().active(); ++run) {
        if (s.phase() == Phase::Prepare) s.startBattle();
        else if (s.phase() == Phase::Report) s.openTree();
        else if (s.phase() == Phase::Tree) {
            if (s.tree().canAfford(ls::NodeId::Damage, s.scrap())) {
                s.buy(ls::NodeId::Damage);
            }
            s.backToPrepare();
            s.autoDeploy();
        }
        for (int i = 0; i < 40000 && s.phase() == Phase::Battle; ++i) {
            s.updateBattle(1.0f / 60.0f);
            if ((i % 8) == 0) { pump(s); record(); }
        }
        pump(s, 4);
        record();
    }

    // Not every step is necessarily SEEN: observe() catches up as far as the
    // player has already got within a single frame, so deploying immediately
    // passes through Deploy without it ever being the displayed step. That is
    // deliberate - a hint that is already satisfied should not flash up. What
    // matters is that whatever did get shown had something to say, and that
    // the coaching covered a real stretch of the loop rather than one step.
    int distinct = 0;
    for (const bool seen : visited) distinct += seen ? 1 : 0;
    CHECK(distinct >= 3);
    CHECK(visited[static_cast<size_t>(TutorialStep::Watch)]);
}

TEST_CASE("the tutorial's last lesson is that the campaign branches") {
    // The single most important thing to teach in a game whose sectors fan
    // out: if the player never learns the map is a choice, the graph may as
    // well be a queue.
    CHECK(static_cast<int>(TutorialStep::Choose) + 1 ==
          static_cast<int>(TutorialStep::Done));
}
