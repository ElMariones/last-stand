#pragma once
#include <raylib.h>

#include "app/Session.h"
#include "ui/Widgets.h"

namespace ls::ui {

// What a screen wants the app to do. Screens never mutate the Session
// directly: they draw, they read input, and they return an intent. That keeps
// every transition in one switch in main.cpp instead of scattered through the
// drawing code.
enum class Action {
    None,
    Continue,
    NewGame,
    Options,
    Quit,
    Back,
    SelectLevel,     // value = level index, or -1 to open the level select
    StartBattle,
    SelectKind,      // value = TurretKind
    CycleTargeting,
    FillHardpoints,
    ClearHardpoints,
    Retry,
    Restart,
    OpenTree,
    BackToPrepare,
    Buy,             // value = node index
    Respec,
    Resume,
    Abandon,
    ToMenu,
    ApplyDisplay,    // window size / fullscreen / ui scale changed
};

struct Result {
    Action action = Action::None;
    int    value  = 0;
};

// Focus indices per screen, plus the transition state. Owned by the app so it
// survives across frames; screens are otherwise stateless.
struct State {
    Focus menu;
    Focus options;
    Focus levels;
    Focus report;
    Focus tree;
    Focus pause;

    int   treeScroll = 0;
    float fade = 0.0f;         // 1 = fully covered, for screen transitions
    float titleClock = 0.0f;
    // NEW GAME erases everything, so it asks twice rather than opening a
    // modal dialog — GDD 13.1 forbids those outright.
    bool  newGameArmed = false;
    int   windowSizeIndex = 0;
    int   hoveredHardpoint = -1;
    WidgetFeedback feedback;
};

// Every screen takes the whole Session because they are all views onto it,
// and returns at most one intent per frame.
Result drawTitle(State& state, const Session& session, float dt);
Result drawMenu(State& state, const Session& session);
Result drawOptions(State& state, Session& session);
Result drawLevelSelect(State& state, const Session& session);
Result drawPrepareHud(State& state, const Session& session);
// The hardpoint markers, drawn over the battlefield in Prepare.
void   drawHardpointOverlay(State& state, const Session& session);
Result drawBattleHud(State& state, const Session& session);
Result drawPause(State& state, const Session& session);
Result drawReport(State& state, const Session& session);
Result drawTree(State& state, const Session& session);

// The dim wash every full-screen overlay sits on, so the live battle behind
// the menus reads as background rather than competition.
void scrim(float alpha);

}  // namespace ls::ui
