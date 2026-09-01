#pragma once
#include <raylib.h>

#include "app/Session.h"
#include "render/Viewport.h"
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
    Stats,
    Quit,
    Back,
    SelectLevel,     // value = level index, or -1 to open the level select
    StartBattle,
    SelectKind,      // value = TurretKind
    BuyTurret,       // value = TurretKind
    CycleTargeting,
    CycleSpeed,
    FillHardpoints,
    ClearHardpoints,
    Retry,
    Restart,
    ToMap,           // open the sector map from wherever we are
    SkipTutorial,
    RestartTutorial,
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
    Focus stats;

    int   treeScroll = 0;
    float fade = 0.0f;         // 1 = fully covered, for screen transitions
    float titleClock = 0.0f;
    // NEW GAME erases everything, so it asks twice rather than opening a
    // modal dialog — GDD 13.1 forbids those outright.
    bool  newGameArmed = false;
    int   windowSizeIndex = 0;
    int   hoveredHardpoint = -1;
    // Which placed turret the mouse is dragging, or -1. Owned here because it
    // spans frames and the screens themselves are stateless.
    int   dragIndex = -1;
    // The sector map pans by dragging; this is its scroll offset and grab.
    Vec2  mapPan{0.0f, 0.0f};
    Vec2  mapGrab{0.0f, 0.0f};
    bool  mapDragging = false;
    bool  mapCentred = false;
    WidgetFeedback feedback;
};

// Every screen takes the whole Session because they are all views onto it,
// and returns at most one intent per frame.
Result drawTitle(State& state, const Session& session, float dt);
Result drawMenu(State& state, const Session& session);
Result drawOptions(State& state, Session& session);
Result drawLevelSelect(State& state, const Session& session);
Result drawStats(State& state, const Session& session);
Result drawPrepareHud(State& state, const Session& session,
                      const Viewport& viewport);
// The turret markers, drawn over the battlefield in Prepare.
void   drawHardpointOverlay(State& state, const Session& session,
                            const Viewport& viewport);
Result drawBattleHud(State& state, const Session& session);
Result drawPause(State& state, const Session& session);
Result drawReport(State& state, const Session& session);
Result drawTree(State& state, const Session& session);

// The tutorial's coach line, drawn on top of whatever screen is up. Returns
// SkipTutorial if the player dismissed it. Draws nothing once the tutorial is
// finished, so the caller can always call it.
Result drawTutorial(const Session& session);

// The dim wash every full-screen overlay sits on, so the live battle behind
// the menus reads as background rather than competition.
void scrim(float alpha);

}  // namespace ls::ui
