#pragma once
#include <cstdint>

namespace ls {

class Session;

// The first run, taught by doing. Not a wall of text before the game starts -
// GDD 13.1 forbids modal dialogs, and nobody reads a manual anyway. Each step
// asks for one action, and advances when the player performs it, so the
// tutorial is indistinguishable from playing badly-signposted and then
// well-signposted.
//
// It teaches the loop rather than the controls: place, deploy, lose, spend,
// retry, win, choose. The last step is the important one - a branching
// campaign is worthless if the player never learns the map is a choice.
enum class TutorialStep : uint8_t {
    // A sector loads with the player's turrets already on the field, so the
    // first thing to teach is not "place one" - there is nothing in the crate
    // to place - it is that where they stand is the player's call.
    MoveTurret = 0,
    Deploy,
    Watch,
    Speed,
    LosingPays,
    Spend,
    TryAgain,
    Claim,
    Choose,
    Done,
};

class Tutorial {
public:
    // Steps only ever advance. A player who deploys before being told to has
    // learned it, so the tutorial skips ahead rather than asking for
    // something already done.
    void observe(const Session& session, float frameSeconds);

    void skip() { step_ = TutorialStep::Done; }
    void restart() { step_ = TutorialStep::MoveTurret; dwell_ = 0.0f; }

    TutorialStep step() const { return step_; }
    bool active() const { return step_ != TutorialStep::Done; }

    // The instruction, and the reason for it. The reason is the part that
    // makes it stick.
    const char* headline() const;
    const char* detail() const;

private:
    void advanceTo(TutorialStep next);
    // One pass of the step machine; observe() runs it until it settles.
    void evaluate(const Session& session);

    TutorialStep step_  = TutorialStep::MoveTurret;
    float        dwell_ = 0.0f;   // seconds on the current step
};

}  // namespace ls
