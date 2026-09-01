#include "app/Tutorial.h"

#include "app/Session.h"

namespace ls {

namespace {

// How long the two "just look at it" steps hold before moving on. They are
// the only steps with no action to wait for, so they are the only ones on a
// timer.
constexpr float kWatchSeconds = 7.0f;
constexpr float kSpeedSeconds = 8.0f;

}  // namespace

void Tutorial::advanceTo(TutorialStep next) {
    if (static_cast<uint8_t>(next) <= static_cast<uint8_t>(step_)) return;
    step_ = next;
    dwell_ = 0.0f;
}

void Tutorial::observe(const Session& session, float frameSeconds) {
    if (step_ == TutorialStep::Done) return;
    dwell_ += frameSeconds;

    // Re-evaluate until the step stops moving. A player who deploys
    // immediately has satisfied three steps at once, and showing them one
    // stale hint per frame for three frames is a flicker, not guidance.
    // Bounded by the step count, so it can never spin.
    for (int guard = 0; guard <= static_cast<int>(TutorialStep::Done); ++guard) {
        const TutorialStep before = step_;
        evaluate(session);
        if (step_ == before) return;
    }
}

void Tutorial::evaluate(const Session& session) {

    const Phase phase = session.phase();

    switch (step_) {
        case TutorialStep::MoveTurret:
            // Dragging one, or buying and siting a new one, both count: the
            // lesson is that the player chooses, not which verb they used.
            // Deploying straight past it counts too - see advanceTo.
            if (session.placementsMade() > 0u || phase == Phase::Battle) {
                advanceTo(TutorialStep::Deploy);
            }
            break;

        case TutorialStep::Deploy:
            if (phase == Phase::Battle) advanceTo(TutorialStep::Watch);
            break;

        case TutorialStep::Watch:
            if (phase == Phase::Report) advanceTo(TutorialStep::LosingPays);
            else if (dwell_ >= kWatchSeconds) advanceTo(TutorialStep::Speed);
            break;

        case TutorialStep::Speed:
            if (phase == Phase::Report) advanceTo(TutorialStep::LosingPays);
            else if (session.timeScale() > 1 || dwell_ >= kSpeedSeconds) {
                advanceTo(TutorialStep::Spend);
            }
            break;

        case TutorialStep::LosingPays:
            // A first battle that was somehow WON skips the whole economy
            // lesson - there is nothing to be consoled about, and the sector
            // it opened is the more interesting thing to talk about.
            if (session.hasResult() && session.result().victory) {
                advanceTo(TutorialStep::Claim);
            } else if (phase == Phase::Tree) {
                advanceTo(TutorialStep::Spend);
            }
            break;

        case TutorialStep::Spend:
            if (session.tree().totalSpent() > 0u) {
                advanceTo(TutorialStep::TryAgain);
            }
            break;

        case TutorialStep::TryAgain:
            if (phase == Phase::Battle) advanceTo(TutorialStep::Claim);
            break;

        case TutorialStep::Claim:
            // Only once a sector has actually been held. Until then this step
            // waits quietly rather than nagging through a string of losses.
            if (phase == Phase::Report && session.hasResult() &&
                session.result().victory) {
                if (session.canAdvance()) advanceTo(TutorialStep::Choose);
                else advanceTo(TutorialStep::Done);
            }
            break;

        case TutorialStep::Choose:
            // The last thing worth teaching: the map is a choice, not a
            // corridor. Done when the player picks something from it.
            if (phase == Phase::Prepare) advanceTo(TutorialStep::Done);
            break;

        case TutorialStep::Done:
            break;
    }
}

const char* Tutorial::headline() const {
    switch (step_) {
        case TutorialStep::MoveTurret:  return "Drag a turret somewhere better";
        case TutorialStep::Deploy:      return "SPACE to deploy";
        case TutorialStep::Watch:       return "They are walking to your base";
        case TutorialStep::Speed:       return "S runs it faster";
        case TutorialStep::LosingPays:  return "You lost, and got paid anyway";
        case TutorialStep::Spend:       return "Spend it - upgrades are permanent";
        case TutorialStep::TryAgain:    return "R runs the sector again";
        case TutorialStep::Claim:       return "Sector held";
        case TutorialStep::Choose:      return "Choose where to go next";
        case TutorialStep::Done:        return "";
    }
    return "";
}

const char* Tutorial::detail() const {
    switch (step_) {
        case TutorialStep::MoveTurret:
            return "Anywhere walkable, and you can move it again whenever you "
                   "like. A turret only shoots what walks into its range, so "
                   "where they stand is the whole tactic.";
        case TutorialStep::Deploy:
            return "You can go in with turrets still in the crate. Nobody will "
                   "stop you.";
        case TutorialStep::Watch:
            return "Everything that reaches it takes a bite out of it. Hold "
                   "them off long enough and the invasion runs out.";
        case TutorialStep::Speed:
            return "1x, 2x, 4x. The simulation is identical either way - it is "
                   "your time being saved, not the game's.";
        case TutorialStep::LosingPays:
            return "Every kill is Scrap whether you hold the sector or not. "
                   "Losing is how the first few runs are funded. Press U.";
        case TutorialStep::Spend:
            return "It is kept forever, across every sector and every retry. "
                   "The board also tells you which two nodes it thinks you need.";
        case TutorialStep::TryAgain:
            return "Same invasion, same order, every time - so what changed is "
                   "you.";
        case TutorialStep::Claim:
            return "That opened new ground. Press M for the sector map.";
        case TutorialStep::Choose:
            return "The campaign branches. Several sectors are open now and "
                   "you can take them in any order - or come back to a hard "
                   "one later, stronger.";
        case TutorialStep::Done:
            return "";
    }
    return "";
}

}  // namespace ls
