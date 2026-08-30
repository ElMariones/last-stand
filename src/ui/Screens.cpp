#include "ui/Screens.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "render/Icons.h"
#include "render/Theme.h"

namespace ls::ui {

namespace {

constexpr float kRowBase = 44.0f;
float row() { return px(kRowBase); }

// Shown on the title screen. A build a tester can name is a build you can get
// a useful bug report about.
constexpr const char* kVersion = "vertical slice · build 6";

const char* const kNodeNames[kNodeCount] = {
    "Damage",         "Fire Rate",      "Range",           "Base HP",
    "Base Regen",     "Economy",        "Splash",          "Burn",
    "Cannon",         "Flamethrower",   "Extra Hardpoint", "DENSEST",
    "MG Overclock",   "MG Ricochet",    "MG Bullet Storm", "Explosive Shells",
    "Knockback",      "Cluster Shot",   "Lingering Flames","Ignite",
    "Firestorm",      "Airstrike",      "Overcharge",      "Armor Piercing",
};

const char* const kNodeDesc[kNodeCount] = {
    "+20% damage",     "+15% fire rate", "+12% range",     "+300 base HP",
    "+2 HP/sec",       "+20% scrap",     "+15% splash",    "+20% burn",
    "unlock Cannon",   "unlock Flame",   "+1 hardpoint",   "unlock DENSEST",
    "2x rate, 0.7x dmg","+1 bounce",     "20th shot spreads","splash +50%",
    "knockback +150%", "4 sub-blasts",   "burn lasts 2x",  "burn spreads",
    "burn damage 2x",  "unlock Airstrike","unlock Overcharge","+50% vs Tank",
};

float screenW() { return static_cast<float>(GetScreenWidth()); }
float screenH() { return static_cast<float>(GetScreenHeight()); }

// A centred column, the layout every menu screen uses. Width is scaled but
// clamped to the window, so a large UI scale on a small window still fits.
Rectangle column(float width, float top, float height) {
    const float w = std::min(px(width), screenW() - px(32.0f));
    return Rectangle{(screenW() - w) * 0.5f, top, w, height};
}

void navigate(Focus& focus) {
    if (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP)) focus.move(-1);
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN)) focus.move(1);
}

bool pressedBack() {
    return IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE);
}

void formatTime(float seconds, char* out, size_t n) {
    const int total = static_cast<int>(seconds);
    std::snprintf(out, n, "%02d:%02d", total / 60, total % 60);
}

}  // namespace

void scrim(float alpha) {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  theme::withAlpha(Color{8, 7, 9, 255}, alpha));
}

// ----------------------------------------------------------------- title ---

Result drawTitle(State& state, const Session& session, float dt) {
    (void)session;
    state.titleClock += dt;
    scrim(0.72f);

    const float cx = screenW() * 0.5f;
    const float top = screenH() * 0.26f;

    // A slow breathing glow behind the wordmark, so the title screen is not
    // a still image over a moving battle.
    const float pulse = 0.5f + 0.5f * std::sin(state.titleClock * 1.2f);
    DrawRectangle(0, static_cast<int>(top - px(18.0f)), GetScreenWidth(),
                  static_cast<int>(px(96.0f)),
                  theme::withAlpha(theme::kColdDeep, 0.15f + pulse * 0.10f));

    textCentered("LAST STAND", cx, top, sz(theme::kDisplay), theme::kInk);
    textCentered("you don't need to survive forever.", cx, top + px(78.0f),
                 sz(theme::kSmall), theme::kInkDim);
    textCentered("you just need to get stronger faster than they do.", cx,
                 top + px(98.0f), sz(theme::kSmall), theme::kInkDim);

    const float blink = 0.45f + 0.55f * std::sin(state.titleClock * 3.0f);
    textCentered("PRESS ENTER", cx, screenH() * 0.72f, sz(theme::kBody),
                 theme::withAlpha(theme::kCold, blink));
    textCentered(kVersion, cx, screenH() - px(34.0f), sz(theme::kMicro),
                 theme::kInkFaint);

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        return {Action::Continue, 0};
    }
    return {};
}

// ------------------------------------------------------------------ menu ---

Result drawMenu(State& state, const Session& session) {
    scrim(0.82f);
    const bool resumable = session.hasProgress();
    const int itemCount = resumable ? 5 : 3;
    state.menu.begin(itemCount);
    navigate(state.menu);

    const float cx = screenW() * 0.5f;
    textCentered("LAST STAND", cx, screenH() * 0.16f, sz(theme::kTitle),
                 theme::kInk);

    char line[128];
    if (resumable) {
        std::snprintf(line, sizeof(line), "%u SCRAP  ·  best %u  ·  %u cleared",
                      session.scrap(), session.bestKillsFor(0),
                      session.clearCountFor(0) + session.clearCountFor(1) +
                          session.clearCountFor(2));
    } else {
        std::snprintf(line, sizeof(line), "a new commander");
    }
    textCentered(line, cx, screenH() * 0.16f + px(52.0f), sz(theme::kSmall),
                 resumable ? theme::kScrap : theme::kInkFaint);

    const Rectangle box =
        column(360.0f, screenH() * 0.38f, row() * static_cast<float>(itemCount));
    const auto at = [&](int i) {
        return Rectangle{box.x, box.y + row() * static_cast<float>(i), box.width,
                         row()};
    };

    Result result;
    int item = 0;
    if (resumable) {
        if (button(at(item), "CONTINUE", state.menu, item)) {
            result = {Action::Continue, 0};
        }
        ++item;
    }

    // Two-step rather than a dialog: the label becomes the warning, and a
    // second press confirms. Moving away disarms it.
    const bool armed = state.newGameArmed;
    if (button(at(item), armed ? "ERASE EVERYTHING?" : "NEW GAME", state.menu,
               item)) {
        if (armed) {
            state.newGameArmed = false;
            result = {Action::NewGame, 0};
        } else if (resumable) {
            state.newGameArmed = true;
        } else {
            result = {Action::NewGame, 0};
        }
    }
    if (armed && !state.menu.isFocused(item)) state.newGameArmed = false;
    if (armed) {
        textCentered("this deletes your Scrap, tree and records",
                     cx, at(item).y + row() - px(4.0f), sz(theme::kMicro),
                     theme::kDanger);
    }
    ++item;

    if (resumable) {
        if (button(at(item), "SERVICE RECORD", state.menu, item)) {
            result = {Action::Stats, 0};
        }
        ++item;
    }
    if (button(at(item), "OPTIONS", state.menu, item)) {
        result = {Action::Options, 0};
    }
    ++item;
    if (button(at(item), "QUIT", state.menu, item)) {
        result = {Action::Quit, 0};
    }

    textCentered("arrows or mouse to choose  ·  enter to confirm", cx,
                 screenH() - px(56.0f), sz(theme::kMicro), theme::kInkFaint);
    return result;
}

// --------------------------------------------------------------- options ---

Result drawOptions(State& state, Session& session) {
    scrim(0.88f);
    state.options.begin(13);
    navigate(state.options);

    Settings& s = session.settings();
    const Rectangle box = column(660.0f, screenH() * 0.08f, row() * 15.0f);
    panelTitled(box, "OPTIONS");

    const Rectangle rowRect{box.x + px(theme::kUnit),
                            box.y + px(theme::kUnit * 6.0f),
                            box.width - px(theme::kUnit * 2.0f), row()};
    const auto at = [&](int i) {
        return Rectangle{rowRect.x, rowRect.y + row() * static_cast<float>(i),
                         rowRect.width, row()};
    };
    const auto section = [&](const char* label, int i) {
        text(label, rowRect.x + px(theme::kGutter),
             at(i).y + row() * 0.5f - static_cast<float>(sz(theme::kMicro)) * 0.5f,
             sz(theme::kMicro), theme::kCold);
    };

    bool changed = false;
    bool display = false;

    section("AUDIO", 0);
    changed |= slider(at(1), "MASTER VOLUME", s.masterVolume, 0, 100, 5,
                      state.options, 1);
    changed |= slider(at(2), "EFFECTS", s.sfxVolume, 0, 100, 5, state.options, 2);
    changed |= slider(at(3), "MUSIC", s.musicVolume, 0, 100, 5, state.options, 3);

    section("DISPLAY", 4);
    // The size row shows the resolution rather than an index, because an
    // index is not a thing anybody wants to read.
    {
        const Rectangle r = at(5);
        int shown = state.windowSizeIndex;
        // The slider's own readout would be an index, which is not a thing
        // anybody wants to read, so it is suppressed and the resolution is
        // drawn in its place.
        if (sliderQuiet(r, "WINDOW SIZE", shown, 0, windowSizeCount() - 1, 1,
                        state.options, 5, s.fullscreen)) {
            state.windowSizeIndex = shown;
            windowSizeAt(shown, s.windowWidth, s.windowHeight);
            changed = true;
            display = true;
        }
        int w = 0;
        int h = 0;
        windowSizeAt(state.windowSizeIndex, w, h);
        char value[32];
        std::snprintf(value, sizeof(value), "%d x %d", w, h);
        textRight(value, r.x + r.width - px(theme::kGutter),
                  r.y + r.height * 0.5f -
                      static_cast<float>(sz(theme::kSmall)) * 0.5f,
                  sz(theme::kSmall),
                  s.fullscreen ? theme::kInkFaint : theme::kInkDim);
    }
    if (toggle(at(6), "FULLSCREEN", s.fullscreen, state.options, 6)) {
        changed = true;
        display = true;
    }
    if (slider(at(7), "INTERFACE SIZE", s.uiScale, 75, 150, 5, state.options, 7)) {
        changed = true;
        display = true;
    }

    section("GAMEPLAY", 8);
    changed |= slider(at(9), "SCREEN SHAKE", s.shakeScale, 0, 200, 10,
                      state.options, 9);
    changed |= slider(at(10), "DEFAULT SPEED", s.defaultTimeScale, 1, 4, 1,
                      state.options, 10);
    changed |= toggle(at(11), "DAMAGE NUMBERS", s.damageNumbers, state.options, 11);
    changed |= toggle(at(12), "PERFORMANCE OVERLAY", s.debugOverlay,
                      state.options, 12);

    text("esc or the X to go back  ·  changes save immediately",
         box.x + px(theme::kGutter),
         box.y + box.height - px(theme::kUnit * 3.0f), sz(theme::kMicro),
         theme::kInkFaint);

    if (changed) session.applySettings();
    if (closeButton(box) || pressedBack()) return {Action::Back, 0};
    if (display) return {Action::ApplyDisplay, 0};
    return {};
}

// ------------------------------------------------------------ sector map ---

namespace {

// The campaign laid out as a route rather than a list. Positions are in map
// space; the screen pans over them, so the board can be bigger than the
// window and still feel like one place.
struct SectorNode { float x; float y; };
constexpr SectorNode kSectorLayout[8] = {
    {180.0f, 520.0f}, {420.0f, 400.0f}, {690.0f, 470.0f}, {940.0f, 320.0f},
    {1200.0f, 430.0f}, {1470.0f, 300.0f}, {1740.0f, 440.0f}, {2020.0f, 300.0f},
};
constexpr float kMapWidth = 2200.0f;

const char* const kSectorBlurb[8] = {
    "One lane, one chokepoint. Where it starts.",
    "Two lanes converging. Rear-only coverage dies here.",
    "An open approach into a hard funnel.",
    "Two paths that never meet. Split your guns.",
    "One long switchback. Everything passes you twice.",
    "Four entrances. Nothing is defended by facing one way.",
    "Three chokepoints in series. Compress, release, compress.",
    "No cover, three sides. Purely how fast you can kill.",
};

}  // namespace

Result drawLevelSelect(State& state, const Session& session) {
    // Heavier than the other overlays: the route has to read as a map, and a
    // battle glowing through it just looks like dirt on the screen.
    scrim(0.965f);

    const float mapTop = screenH() * 0.16f;
    const float mapH = screenH() * 0.56f;
    const Rectangle board{0.0f, mapTop, screenW(), mapH};

    // Centre on the furthest sector the player has opened, once, so a
    // returning player arrives looking at where they got to.
    if (!state.mapCentred) {
        const int furthest = session.furthestUnlockedLevel();
        state.mapPan.x = screenW() * 0.5f -
                         px(kSectorLayout[static_cast<size_t>(furthest)].x);
        state.mapCentred = true;
    }

    // Drag to pan. Clamped so the route cannot be dragged off the screen
    // entirely, which is the usual way a pannable board gets lost.
    const Vector2 mouse = GetMousePosition();
    if (CheckCollisionPointRec(mouse, board)) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            state.mapDragging = true;
            state.mapGrab = Vec2{mouse.x - state.mapPan.x, 0.0f};
        }
        const float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) state.mapPan.x += wheel * px(60.0f);
    }
    if (state.mapDragging) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            state.mapPan.x = mouse.x - state.mapGrab.x;
        } else {
            state.mapDragging = false;
        }
    }
    const float span = px(kMapWidth);
    state.mapPan.x = std::clamp(state.mapPan.x, screenW() - span - px(120.0f),
                                px(120.0f));

    const auto at = [&](int i) {
        const SectorNode& n = kSectorLayout[static_cast<size_t>(i)];
        return Vector2{state.mapPan.x + px(n.x),
                       board.y + board.height * 0.5f + px(n.y - 400.0f) * 0.55f};
    };

    state.levels.begin(kLevelCount);
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT)) {
        state.levels.move(-1);
    }
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) {
        state.levels.move(1);
    }

    textCentered("SECTOR MAP", screenW() * 0.5f, screenH() * 0.07f,
                 sz(theme::kTitle), theme::kInk);

    // The route: solid where the player has been, dashed-dim ahead of them.
    for (int i = 0; i + 1 < kLevelCount; ++i) {
        const bool travelled = session.isLevelUnlocked(i + 1);
        DrawLineEx(at(i), at(i + 1), travelled ? 2.5f : 1.5f,
                   travelled ? theme::withAlpha(theme::kColdDim, 0.9f)
                             : theme::withAlpha(theme::kColdDeep, 0.9f));
    }

    Result result;
    for (int i = 0; i < kLevelCount; ++i) {
        const Vector2 c = at(i);
        const bool unlocked = session.isLevelUnlocked(i);
        const bool cleared = session.clearCountFor(i) > 0u;
        const bool current = session.levelIndex() == i;
        const float r = px(26.0f);

        const Rectangle hit{c.x - r, c.y - r, r * 2.0f, r * 2.0f};
        if (CheckCollisionPointRec(mouse, hit) && !state.mapDragging) {
            state.levels.index = i;
        }
        const bool focused = state.levels.isFocused(i);

        // Cleared sectors are filled, open ones are outlined, locked ones are
        // barely there. Status is readable at a glance without reading a word.
        Color ink = theme::kInkFaint;
        if (cleared) ink = theme::kGood;
        else if (unlocked) ink = theme::kCold;

        if (focused) {
            DrawCircleV(c, r + px(7.0f), theme::withAlpha(ink, 0.16f));
            DrawCircleLinesV(c, r + px(7.0f), theme::withAlpha(ink, 0.7f));
        }
        if (cleared) {
            DrawCircleV(c, r, theme::withAlpha(ink, 0.30f));
        }
        DrawCircleLinesV(c, r, ink);
        if (current) DrawCircleLinesV(c, r - px(5.0f), theme::withAlpha(ink, 0.8f));

        char label[8];
        std::snprintf(label, sizeof(label), "%d", i + 1);
        textCentered(label, c.x, c.y - static_cast<float>(sz(theme::kBody)) * 0.5f,
                     sz(theme::kBody), unlocked ? theme::kInk : theme::kInkFaint);

        if (!unlocked) {
            // A padlock drawn from two rectangles: no glyph, no asset.
            const float lw = px(9.0f);
            const float lh = px(7.0f);
            DrawRectangleV(Vector2{c.x - lw * 0.5f, c.y + r * 0.45f},
                           Vector2{lw, lh}, theme::kInkFaint);
            DrawCircleLinesV(Vector2{c.x, c.y + r * 0.45f}, lw * 0.42f,
                             theme::kInkFaint);
        }

        textCentered(ls::levelName(i), c.x, c.y + r + px(8.0f),
                     sz(theme::kMicro),
                     unlocked ? theme::kInkDim : theme::kInkFaint);

        if (unlocked && CheckCollisionPointRec(mouse, hit) &&
            IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !state.mapDragging) {
            result = {Action::SelectLevel, i};
        }
    }

    // The detail card for whatever is focused, pinned below the board so it
    // does not move as the map pans.
    const int sel = std::clamp(state.levels.index, 0, kLevelCount - 1);
    const Rectangle card = column(660.0f, board.y + board.height + px(12.0f),
                                  row() * 2.3f);
    panel(card);
    char line[160];
    std::snprintf(line, sizeof(line), "SECTOR %d  ·  %s", sel + 1,
                  ls::levelName(sel));
    text(line, card.x + px(theme::kGutter), card.y + px(12.0f),
         sz(theme::kBody), theme::kInk);
    text(kSectorBlurb[static_cast<size_t>(sel)], card.x + px(theme::kGutter),
         card.y + px(38.0f), sz(theme::kMicro), theme::kInkFaint);

    if (session.isLevelUnlocked(sel)) {
        std::snprintf(line, sizeof(line), "power %u  ·  best %u  ·  cleared %ux",
                      ls::levelRecommendedPower(sel), session.bestKillsFor(sel),
                      session.clearCountFor(sel));
        text(line, card.x + px(theme::kGutter), card.y + px(62.0f),
             sz(theme::kMicro), theme::kInkDim);
        textRight("ENTER  deploy", card.x + card.width - px(theme::kGutter),
                  card.y + px(38.0f), sz(theme::kSmall), theme::kGood);
    } else {
        std::snprintf(line, sizeof(line), "locked  ·  hold sector %d to open it",
                      sel);
        text(line, card.x + px(theme::kGutter), card.y + px(62.0f),
             sz(theme::kMicro), theme::kDanger);
    }

    textCentered("drag or scroll to pan  ·  arrows to choose  ·  esc to go back",
                 screenW() * 0.5f, screenH() - px(40.0f), sz(theme::kMicro),
                 theme::kInkFaint);

    if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) &&
        session.isLevelUnlocked(sel)) {
        result = {Action::SelectLevel, sel};
    }
    if (pressedBack()) return {Action::Back, 0};
    return result;
}

// ----------------------------------------------------------------- stats ---

Result drawStats(State& state, const Session& session) {
    scrim(0.88f);
    state.stats.begin(1);

    const ls::Stats& st = session.stats();
    const Rectangle box = column(640.0f, screenH() * 0.12f, row() * 12.0f);
    panelTitled(box, "SERVICE RECORD");

    float y = box.y + px(theme::kUnit * 7.0f);
    char value[64];

    // Two columns: the label reads as a question, the number answers it.
    const auto line2 = [&](const char* label, const char* v, Color ink) {
        text(label, box.x + px(theme::kGutter), y, sz(theme::kSmall),
             theme::kInkDim);
        textRight(v, box.x + box.width - px(theme::kGutter), y,
                  sz(theme::kSmall), ink);
        y += px(30.0f);
    };
    const auto section = [&](const char* label) {
        y += px(6.0f);
        text(label, box.x + px(theme::kGutter), y, sz(theme::kMicro),
             theme::kCold);
        y += px(24.0f);
    };

    section("CAREER");
    std::snprintf(value, sizeof(value), "%u", st.runs);
    line2("Runs", value, theme::kInk);
    std::snprintf(value, sizeof(value), "%u  (%.0f%%)", st.victories,
                  static_cast<double>(st.winRate() * 100.0f));
    line2("Sectors held", value, st.victories > 0u ? theme::kGood : theme::kInkDim);
    // Seconds early on, minutes soon after, hours eventually: "0h 00m" after
    // a first forty-second run reads as a broken counter.
    const uint32_t secs = st.secondsPlayed;
    if (secs < 60u) {
        std::snprintf(value, sizeof(value), "%us", secs);
    } else if (secs < 3600u) {
        std::snprintf(value, sizeof(value), "%um %02us", secs / 60u, secs % 60u);
    } else {
        std::snprintf(value, sizeof(value), "%uh %02um", secs / 3600u,
                      (secs % 3600u) / 60u);
    }
    line2("Time in the field", value, theme::kInk);

    section("DESTRUCTION");
    std::snprintf(value, sizeof(value), "%u", st.kills);
    line2("Total kills", value, theme::kInk);
    std::snprintf(value, sizeof(value), "%u", st.bestRunKills);
    line2("Best single run", value, theme::kScrap);
    if (st.runs > 0u) {
        std::snprintf(value, sizeof(value), "%u",
                      st.kills / std::max(1u, st.runs));
        line2("Average per run", value, theme::kInkDim);
    }

    section("LOGISTICS");
    std::snprintf(value, sizeof(value), "%u", st.scrapEarned);
    line2("Scrap earned", value, theme::kScrap);
    std::snprintf(value, sizeof(value), "%u", st.nodesBought);
    line2("Upgrades bought", value, theme::kInkDim);
    std::snprintf(value, sizeof(value), "%u", st.turretsBought);
    line2("Turrets bought", value, theme::kInkDim);

    if (st.runs == 0u) {
        textCentered("nothing to report yet", box.x + box.width * 0.5f,
                     box.y + box.height - px(46.0f), sz(theme::kMicro),
                     theme::kInkFaint);
    }

    if (closeButton(box) || pressedBack()) return {Action::Back, 0};
    return {};
}

// --------------------------------------------------------------- prepare ---

// The placement layer: suggested emplacements, the range ring of whatever the
// cursor is on, and a ghost of what a click would put down and whether it can
// go there. Turrets are not bound to the emplacements — they are a hint, not
// a grid — so the overlay's job is to make free placement legible rather than
// to mark slots.
void drawHardpointOverlay(State& state, const Session& session,
                          const Viewport& viewport) {
    const Vector2 mouseScreen = GetMousePosition();
    const Vec2 mouse =
        viewport.screenToWorld(Vec2{mouseScreen.x, mouseScreen.y});
    // Every placed turret gets a ring, and the one under the cursor shows the
    // range it actually covers — the number that makes positioning a decision.
    const int hoverTurret = session.turretIndexAt(mouse, 16.0f);
    for (size_t i = 0; i < session.loadout().size(); ++i) {
        const Turret& t = session.loadout()[i];
        const Vec2 p = viewport.worldToScreen(t.position);
        const bool hot = (static_cast<int>(i) == hoverTurret) ||
                         (static_cast<int>(i) == state.dragIndex);
        DrawCircleLinesV(Vector2{p.x, p.y}, viewport.scaled(15.0f),
                         theme::withAlpha(theme::kCold, hot ? 1.0f : 0.5f));
        if (hot) {
            DrawCircleLinesV(Vector2{p.x, p.y}, viewport.scaled(t.range),
                             theme::withAlpha(theme::kCold, 0.35f));
        }
    }

    // The ghost: what a click would deploy, and whether it is allowed to.
    const bool dragging = state.dragIndex >= 0;
    const bool hasSpare = session.available(session.selectedKind()) > 0u;
    if (dragging || (hoverTurret < 0 && hasSpare)) {
        const bool ok = session.canPlaceAt(mouse, state.dragIndex);
        const Color tint = ok ? theme::kGood : theme::kDanger;
        const Vec2 p = viewport.worldToScreen(mouse);
        DrawCircleLinesV(Vector2{p.x, p.y}, viewport.scaled(15.0f),
                         theme::withAlpha(tint, 0.9f));
        const float range = dragging
                                ? session.loadout()[static_cast<size_t>(
                                      state.dragIndex)].range
                                : 160.0f;
        DrawCircleLinesV(Vector2{p.x, p.y}, viewport.scaled(range),
                         theme::withAlpha(tint, 0.25f));
    }

    const char* hint = nullptr;
    if (dragging) {
        hint = session.canPlaceAt(mouse, state.dragIndex) ? "release to place"
                                                          : "cannot go there";
    } else if (hoverTurret >= 0) {
        hint = "drag to move  ·  right-click to recall";
    } else if (hasSpare) {
        hint = session.canPlaceAt(mouse) ? "click to deploy" : "cannot go there";
    } else {
        hint = "no spare turrets - buy one below";
    }
    DrawText(hint, static_cast<int>(mouseScreen.x) + 18,
             static_cast<int>(mouseScreen.y) + 8, sz(theme::kMicro),
             theme::kInkDim);
}

Result drawPrepareHud(State& state, const Session& session,
                      const Viewport& viewport) {
    const float barH = px(104.0f);
    const Rectangle bar{0.0f, screenH() - barH, screenW(), barH};
    DrawRectangleRec(bar, theme::withAlpha(Color{12, 11, 14, 255}, 0.95f));
    rule(0.0f, bar.y, screenW(), 0.8f);

    char line[192];
    std::snprintf(line, sizeof(line), "SECTOR %d · %s",
                  session.levelIndex() + 1, session.level().name.c_str());
    text(line, px(theme::kGutter), bar.y + px(12.0f), sz(theme::kBody),
         theme::kInk);
    std::snprintf(line, sizeof(line), "%u incoming   ·   best %u",
                  session.level().totalEnemies, session.bestKills());
    text(line, px(theme::kGutter), bar.y + px(38.0f), sz(theme::kMicro),
         theme::kInkDim);

    // The arsenal, not a slot count: how many of each kind you own and how
    // many are still in the crate.
    const int deployed = session.turretCount();
    std::snprintf(line, sizeof(line), "%d DEPLOYED  ·  %u in reserve", deployed,
                  session.available(TurretKind::MachineGun) +
                      session.available(TurretKind::Cannon) +
                      session.available(TurretKind::Flamethrower));
    text(line, px(theme::kGutter), bar.y + px(62.0f), sz(theme::kSmall),
         deployed == 0 ? theme::kDanger : theme::kGood);

    struct Kind { TurretKind kind; const char* name; const char* stat; };
    const Kind kinds[3] = {
        {TurretKind::MachineGun,   "1 MACHINE GUN", "fast, single target"},
        {TurretKind::Cannon,       "2 CANNON",      "slow, heavy splash"},
        {TurretKind::Flamethrower, "3 FLAMETHROWER","cone, burns over time"},
    };
    Result result;
    float x = screenW() * 0.26f;
    for (const Kind& k : kinds) {
        const bool unlocked = session.isKindUnlocked(k.kind);
        const bool selected = session.selectedKind() == k.kind;
        const uint32_t spare = session.available(k.kind);
        const Rectangle r{x, bar.y + px(10.0f), px(210.0f), px(60.0f)};
        DrawRectangleRec(r, theme::withAlpha(Color{20, 19, 23, 255},
                                             selected ? 1.0f : 0.55f));
        DrawRectangleLinesEx(r, 1.0f,
                             selected ? theme::kCold
                                      : theme::withAlpha(theme::kColdDeep, 0.9f));
        // The turret itself, drawn at icon scale from the same code the
        // battlefield uses, so the card and the thing it deploys match.
        const Vector2 icon{r.x + px(26.0f), r.y + r.height * 0.5f};
        if (unlocked) {
            drawTurretIcon(k.kind, icon, ui::scale() * 0.9f);
        } else {
            DrawCircleLinesV(icon, px(11.0f), theme::kInkFaint);
        }

        const float textX = r.x + px(50.0f);
        text(k.name, textX, r.y + px(6.0f), sz(theme::kMicro),
             !unlocked ? theme::kInkFaint
                       : (selected ? theme::kInk : theme::kInkDim));

        if (unlocked) {
            char count[48];
            std::snprintf(count, sizeof(count), "%u ready / %u owned", spare,
                          session.owned(k.kind));
            text(count, textX, r.y + px(24.0f), sz(theme::kMicro),
                 spare > 0u ? theme::kGood : theme::kInkFaint);

            // Buying is right here rather than in the tree: the decision
            // "another gun or a better gun" should be one screen, not two.
            const Rectangle buy{r.x + r.width - px(78.0f), r.y + px(34.0f),
                                px(70.0f), px(20.0f)};
            const bool afford = session.canAffordTurret(k.kind);
            DrawRectangleLinesEx(buy, 1.0f,
                                 afford ? theme::kScrap
                                        : theme::withAlpha(theme::kColdDeep, 0.9f));
            char price[32];
            std::snprintf(price, sizeof(price), "BUY %u",
                          session.turretPrice(k.kind));
            textCentered(price, buy.x + buy.width * 0.5f, buy.y + px(3.0f),
                         sz(theme::kMicro),
                         afford ? theme::kScrap : theme::kInkFaint);
            if (afford && hovered(buy) &&
                IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                result = {Action::BuyTurret, static_cast<int>(k.kind)};
            }
            text(k.stat, textX, r.y + px(40.0f), sz(theme::kMicro),
                 theme::kInkFaint);
        } else {
            text("LOCKED - unlock it in the tree", textX, r.y + px(26.0f),
                 sz(theme::kMicro), theme::kInkFaint);
        }

        if (unlocked && hovered(r) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            result.action == Action::None) {
            result = {Action::SelectKind, static_cast<int>(k.kind)};
        }
        x += px(218.0f);
    }

    // DEPLOY is always available, even with empty slots — the player is
    // allowed to try a sector understaffed, and telling them "no" mid-flow is
    // worse than letting them find out.
    const Rectangle deploy{screenW() - px(200.0f), bar.y + px(14.0f),
                           px(180.0f), px(40.0f)};
    const bool ready = deployed > 0;
    DrawRectangleRec(deploy, theme::withAlpha(ready ? theme::kColdDeep
                                                    : Color{40, 24, 24, 255},
                                              0.9f));
    DrawRectangleLinesEx(deploy, 1.5f, ready ? theme::kGood : theme::kDanger);
    textCentered(ready ? "DEPLOY   [SPACE]" : "DEPLOY ANYWAY",
                 deploy.x + deploy.width * 0.5f,
                 deploy.y + deploy.height * 0.5f -
                     static_cast<float>(sz(theme::kSmall)) * 0.5f,
                 sz(theme::kSmall), ready ? theme::kGood : theme::kDanger);
    if (hovered(deploy) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        result = {Action::StartBattle, 0};
    }

    textRight("drag to move  ·  F auto-deploy  ·  C recall all  ·  M targeting",
              screenW() - px(theme::kGutter), bar.y + px(58.0f),
              sz(theme::kMicro), theme::kInkFaint);
    textRight("L sectors  ·  esc menu", screenW() - px(theme::kGutter),
              bar.y + px(76.0f), sz(theme::kMicro), theme::kInkFaint);

    drawHardpointOverlay(state, session, viewport);

    if (IsKeyPressed(KEY_ONE)) return {Action::SelectKind, 0};
    if (IsKeyPressed(KEY_TWO)) return {Action::SelectKind, 1};
    if (IsKeyPressed(KEY_THREE)) return {Action::SelectKind, 2};
    if (IsKeyPressed(KEY_F)) return {Action::FillHardpoints, 0};   // auto-deploy
    if (IsKeyPressed(KEY_C)) return {Action::ClearHardpoints, 0};
    if (IsKeyPressed(KEY_L)) return {Action::SelectLevel, -1};
    if (pressedBack()) return {Action::ToMenu, 0};
    if (IsKeyPressed(KEY_M)) return {Action::CycleTargeting, 0};
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
        return {Action::StartBattle, 0};
    }
    return result;
}

// ---------------------------------------------------------------- battle ---

Result drawBattleHud(State& state, const Session& session) {
    (void)state;
    const World* w = session.world();
    if (w == nullptr) return {};

    // Base health, top centre, big enough to read out of the corner of an eye.
    const float barW = px(360.0f);
    const Rectangle hp{(screenW() - barW) * 0.5f, px(18.0f), barW, px(10.0f)};
    const float frac = (w->base().maxHealth > 0.0f)
                           ? w->base().health / w->base().maxHealth
                           : 0.0f;
    bar(hp, frac, frac > 0.35f ? theme::kCold : theme::kDanger,
        theme::withAlpha(theme::kColdDeep, 0.8f));

    char line[192];
    std::snprintf(line, sizeof(line), "BASE  %.0f%%", frac * 100.0f);
    textCentered(line, screenW() * 0.5f, hp.y + px(16.0f), sz(theme::kMicro),
                 frac > 0.35f ? theme::kInkDim : theme::kDanger);

    // How much of the invasion has arrived: the player's sense of "how much
    // longer" and of whether the peak is still coming.
    const Rectangle prog{hp.x, hp.y + px(34.0f), barW, px(4.0f)};
    const float spawned =
        (session.level().totalEnemies > 0u)
            ? static_cast<float>(w->spawned()) /
                  static_cast<float>(session.level().totalEnemies)
            : 0.0f;
    bar(prog, spawned, theme::withAlpha(theme::kInkFaint, 0.9f),
        theme::withAlpha(theme::kColdDeep, 0.5f));
    char clock[16];
    formatTime(session.telemetry().elapsedSeconds(), clock, sizeof(clock));
    std::snprintf(line, sizeof(line), "%s  ·  %.0f%% of the wave has landed",
                  clock, spawned * 100.0f);
    textCentered(line, screenW() * 0.5f, prog.y + px(8.0f), sz(theme::kMicro),
                 theme::kInkFaint);

    std::snprintf(line, sizeof(line), "%u", session.scrap());
    textRight(line, screenW() - px(theme::kGutter), px(16.0f),
              sz(theme::kHeading), theme::kScrap);
    textRight("SCRAP", screenW() - px(theme::kGutter), px(48.0f),
              sz(theme::kMicro), theme::kInkFaint);

    std::snprintf(line, sizeof(line), "%u / %u", w->totalKills(),
                  session.level().totalEnemies);
    text(line, px(theme::kGutter), px(16.0f), sz(theme::kHeading), theme::kInk);
    std::snprintf(line, sizeof(line), "DESTROYED  ·  best %u",
                  session.bestKills());
    text(line, px(theme::kGutter), px(48.0f), sz(theme::kMicro),
         theme::kInkFaint);

    float x = px(theme::kGutter);
    const float y = screenH() - px(46.0f);
    const auto pill = [&](const char* key, const char* name, bool unlocked,
                          float cd, float full) {
        const Rectangle r{x, y, px(136.0f), px(30.0f)};
        const bool ready = unlocked && cd <= 0.0f;
        DrawRectangleRec(r, theme::withAlpha(Color{18, 17, 20, 255}, 0.92f));
        if (unlocked && cd > 0.0f) {
            const float k = 1.0f - std::clamp(cd / full, 0.0f, 1.0f);
            DrawRectangleRec(Rectangle{r.x, r.y, r.width * k, r.height},
                             theme::withAlpha(theme::kColdDeep, 0.9f));
        }
        DrawRectangleLinesEx(r, 1.0f,
                             ready ? theme::kCold
                                   : theme::withAlpha(theme::kColdDeep, 0.9f));
        char buf[64];
        if (unlocked && cd > 0.0f) {
            std::snprintf(buf, sizeof(buf), "%s  %s  %.0fs", key, name,
                          static_cast<double>(cd));
        } else {
            std::snprintf(buf, sizeof(buf), "%s  %s", key, name);
        }
        text(buf, r.x + px(10.0f), r.y + px(8.0f), sz(theme::kMicro),
             !unlocked ? theme::kInkFaint
                       : (ready ? theme::kInk : theme::kInkDim));
        x += r.width + px(theme::kUnit);
    };
    pill("A", "AIRSTRIKE",
         session.airstrikeReady() || session.airstrikeCooldown() > 0.0f,
         session.airstrikeCooldown(), 25.0f);
    pill("O", "OVERCHARGE",
         session.overchargeReady() || session.overchargeCooldown() > 0.0f,
         session.overchargeCooldown(), 15.0f);

    // Time control as four chevrons that light up, not a number to decode.
    // Clickable, because "press S four times" is not a speed control.
    const float chevW = px(26.0f);
    const Rectangle speed{screenW() - px(theme::kGutter) - chevW * 4.0f - px(46.0f),
                          y, chevW * 4.0f + px(46.0f), px(30.0f)};
    DrawRectangleRec(speed, theme::withAlpha(Color{18, 17, 20, 255}, 0.92f));
    DrawRectangleLinesEx(speed, 1.0f,
                         session.timeScale() > 1
                             ? theme::kCold
                             : theme::withAlpha(theme::kColdDeep, 0.9f));
    text("S", speed.x + px(10.0f), speed.y + px(8.0f), sz(theme::kMicro),
         theme::kInkFaint);
    for (int i = 0; i < 4; ++i) {
        const bool lit = i < session.timeScale();
        text(">", speed.x + px(30.0f) + chevW * static_cast<float>(i),
             speed.y + px(6.0f), sz(theme::kBody),
             lit ? theme::kCold : theme::withAlpha(theme::kColdDeep, 1.0f));
    }
    if (hovered(speed) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        return {Action::CycleSpeed, 0};
    }
    textRight("esc pause", screenW() - px(theme::kGutter), y - px(14.0f),
              sz(theme::kMicro), theme::kInkFaint);

    if (pressedBack()) return {Action::Resume, 0};   // esc opens the pause menu
    return {};
}

// ----------------------------------------------------------------- pause ---

Result drawPause(State& state, const Session& session) {
    (void)session;
    scrim(0.78f);
    state.pause.begin(4);
    navigate(state.pause);

    const float cx = screenW() * 0.5f;
    textCentered("PAUSED", cx, screenH() * 0.26f, sz(theme::kTitle),
                 theme::kInk);

    const Rectangle box = column(320.0f, screenH() * 0.40f, row() * 4.0f);
    const auto at = [&](int i) {
        return Rectangle{box.x, box.y + row() * static_cast<float>(i), box.width,
                         row()};
    };

    Result result;
    if (button(at(0), "RESUME", state.pause, 0)) result = {Action::Resume, 0};
    if (button(at(1), "RESTART SECTOR", state.pause, 1)) {
        result = {Action::Restart, 0};
    }
    if (button(at(2), "OPTIONS", state.pause, 2)) result = {Action::Options, 0};
    if (button(at(3), "ABANDON", state.pause, 3)) result = {Action::Abandon, 0};

    textCentered("abandoning pays nothing", cx, box.y + row() * 4.4f,
                 sz(theme::kMicro), theme::kInkFaint);
    if (pressedBack()) return {Action::Resume, 0};
    return result;
}

// ---------------------------------------------------------------- report ---

Result drawReport(State& state, const Session& session) {
    scrim(0.90f);
    const BattleResult& r = session.result();
    const Payout& p = session.payout();
    const FailureAnalysis& fa = session.failure();
    const BattleTelemetry& t = session.telemetry();
    const Attribution& attr = t.attribution();

    // Every number counts up, and any key completes it instantly
    // (GDD 13.1: everything is skippable).
    const float reveal = session.reportReveal();
    const auto counted = [&](uint32_t value) {
        return static_cast<uint32_t>(static_cast<float>(value) * reveal);
    };

    const Rectangle box = column(780.0f, px(28.0f), screenH() - px(56.0f));
    panel(box);

    const float cx = box.x + box.width * 0.5f;
    float y = box.y + 26.0f;

    textCentered(r.victory ? "SECTOR HELD" : "OVERRUN", cx, y, sz(theme::kTitle),
                 r.victory ? theme::kCold : theme::kDanger);
    y += 54.0f;

    char line[192];
    std::snprintf(line, sizeof(line), "%u / %u destroyed", counted(r.kills),
                  r.totalEnemies);
    textCentered(line, cx, y, sz(theme::kHeading), theme::kInk);
    y += 32.0f;

    char clock[16];
    formatTime(t.elapsedSeconds(), clock, sizeof(clock));
    std::snprintf(line, sizeof(line), "%s survived  ·  peak %.1f kills/sec",
                  clock, static_cast<double>(t.peakKillsPerSecond()));
    textCentered(line, cx, y, sz(theme::kSmall), theme::kInkDim);
    y += 34.0f;

    // The payout, boxed, because it is the reason the player is on this
    // screen at all.
    const Rectangle payoutBox{cx - 170.0f, y, 340.0f, 62.0f};
    DrawRectangleRec(payoutBox, theme::withAlpha(theme::kColdDeep, 0.35f));
    DrawRectangleLinesEx(payoutBox, 1.0f, theme::withAlpha(theme::kScrap, 0.5f));
    std::snprintf(line, sizeof(line), "+ %u SCRAP", counted(p.scrap));
    textCentered(line, cx, y + 10.0f, sz(theme::kTitle), theme::kScrap);
    std::snprintf(line, sizeof(line), "kills %u   depth %u   best %u   x%.2f",
                  p.killScrap, p.depthScrap, p.bestBonus,
                  static_cast<double>(p.multiplier));
    textCentered(line, cx, y + 44.0f, sz(theme::kMicro), theme::kInkDim);
    y += 74.0f;

    if (session.canAdvance()) {
        std::snprintf(line, sizeof(line), "SECTOR %d UNLOCKED   ·   %s",
                      session.levelIndex() + 2,
                      ls::levelName(session.levelIndex() + 1));
        textCentered(line, cx, y, sz(theme::kBody), theme::kGood);
    } else if (p.newBest) {
        std::snprintf(line, sizeof(line), "NEW BEST   ·   was %u",
                      r.previousBest);
        textCentered(line, cx, y, sz(theme::kBody), theme::kGood);
    }
    y += 34.0f;

    rule(box.x + px(theme::kGutter), y, box.width - px(theme::kGutter) * 2.0f, 0.7f);
    y += 16.0f;

    // Failure Analysis: where, when, and by how much (GDD 13.2).
    text(fa.victory ? "AFTER ACTION" : "FAILURE ANALYSIS",
         box.x + px(theme::kGutter), y, sz(theme::kSmall), theme::kCold);
    y += 26.0f;

    if (t.breached()) {
        char breach[16];
        formatTime(fa.breachTime, breach, sizeof(breach));
        std::snprintf(line, sizeof(line), "Breach at %s, %s", fa.lane, breach);
    } else {
        std::snprintf(line, sizeof(line), "No breach. The line held.");
    }
    text(line, box.x + px(theme::kGutter), y, sz(theme::kSmall), theme::kInk);
    y += 24.0f;

    const auto stat = [&](const char* label, const char* value, Color ink) {
        text(label, box.x + px(theme::kGutter), y, sz(theme::kMicro), theme::kInkFaint);
        text(value, box.x + px(theme::kGutter) + 240.0f, y, sz(theme::kMicro), ink);
        y += 20.0f;
    };
    char value[64];
    std::snprintf(value, sizeof(value), "%u", fa.peakDensity);
    stat("Peak density there", value, theme::kInkDim);
    std::snprintf(value, sizeof(value), "%u", fa.yourDps);
    stat("Your DPS", value, theme::kInkDim);
    std::snprintf(value, sizeof(value), "%u", fa.requiredDps);
    stat("Estimated requirement", value,
         fa.requiredDps > fa.yourDps ? theme::kDanger : theme::kGood);

    y += 6.0f;
    for (int i = 0; i < fa.suggestionCount; ++i) {
        const size_t n = static_cast<size_t>(fa.suggestions[static_cast<size_t>(i)]);
        std::snprintf(line, sizeof(line), "> BUY  %-18s %s", kNodeNames[n],
                      kNodeDesc[n]);
        text(line, box.x + px(theme::kGutter), y, sz(theme::kSmall), theme::kScrap);
        y += 22.0f;
    }

    y += 10.0f;
    rule(box.x + px(theme::kGutter), y, box.width - px(theme::kGutter) * 2.0f, 0.7f);
    y += 16.0f;

    // Damage attribution. A turret sitting at 4% across ten runs is a turret
    // that needs a buff, and this says so without a spreadsheet.
    struct Src { const char* name; uint32_t kills; Color color; };
    const Src sources[4] = {
        {"Machine Gun",  attr.machineGun,   theme::kCold},
        {"Cannon",       attr.cannon,       theme::kColdDim},
        {"Flamethrower", attr.flamethrower, theme::kFireMid},
        {"Burn",         attr.burn,         theme::kFireDeep},
    };
    for (const Src& src : sources) {
        text(src.name, box.x + px(theme::kGutter), y, sz(theme::kMicro),
             theme::kInkDim);
        const Rectangle track{box.x + px(theme::kGutter) + 140.0f, y + 4.0f, 300.0f,
                              8.0f};
        bar(track, attr.share(src.kills) * reveal, src.color,
            theme::withAlpha(theme::kColdDeep, 0.6f));
        std::snprintf(value, sizeof(value), "%3.0f%%",
                      static_cast<double>(attr.share(src.kills) * 100.0f));
        text(value, track.x + track.width + 14.0f, y, sz(theme::kMicro),
             theme::kInkDim);
        y += 20.0f;
    }

    // The retry path is the most optimised path in the game: always the same
    // place, always one key, never behind a confirmation (GDD 13.1). It is
    // pinned to the bottom of the panel rather than flowing after the
    // content, so it never moves and never collides with a long analysis.
    const float footer = box.y + box.height - px(56.0f);
    rule(box.x + px(theme::kGutter), footer - 12.0f,
         box.width - px(theme::kGutter) * 2.0f, 0.7f);
    // Winning opens the next sector, and the report says so instead of
    // leaving the player to go looking. RETRY keeps its position either way,
    // because it is the path the whole game is optimised around (GDD 13.1).
    const bool advance = session.canAdvance();
    const int buttons = advance ? 4 : 3;
    state.report.begin(buttons);
    navigate(state.report);

    Result result;
    const float bw = (box.width - px(theme::kGutter) * 2.0f) /
                     static_cast<float>(buttons);
    const auto at = [&](int i) {
        return Rectangle{box.x + px(theme::kGutter) + bw * static_cast<float>(i),
                         footer, bw, px(40.0f)};
    };

    int item = 0;
    if (advance) {
        char label[64];
        std::snprintf(label, sizeof(label), "N  SECTOR %d",
                      session.levelIndex() + 2);
        // Drawn as the one thing on the screen with a filled frame.
        DrawRectangleRec(at(item), theme::withAlpha(theme::kGood, 0.16f));
        DrawRectangleLinesEx(at(item), 1.5f, theme::kGood);
        if (button(at(item), label, state.report, item)) {
            result = {Action::Advance, 0};
        }
        ++item;
    }
    if (button(at(item), "R  RETRY", state.report, item)) {
        result = {Action::Retry, 0};
    }
    ++item;
    if (button(at(item), "U  UPGRADE", state.report, item)) {
        result = {Action::OpenTree, 0};
    }
    ++item;
    if (button(at(item), "P  PREPARE", state.report, item)) {
        result = {Action::BackToPrepare, 0};
    }

    if (advance && (IsKeyPressed(KEY_N) || IsKeyPressed(KEY_ENTER))) {
        result = {Action::Advance, 0};
    }
    if (IsKeyPressed(KEY_R)) result = {Action::Retry, 0};
    if (IsKeyPressed(KEY_U)) result = {Action::OpenTree, 0};
    if (IsKeyPressed(KEY_P)) result = {Action::BackToPrepare, 0};
    return result;
}

// ------------------------------------------------------------------ tree ---

Result drawTree(State& state, const Session& session) {
    scrim(0.92f);
    state.tree.begin(static_cast<int>(kNodeCount));
    navigate(state.tree);

    const Rectangle box = column(820.0f, 40.0f, screenH() - 110.0f);
    panel(box);

    char line[128];
    std::snprintf(line, sizeof(line), "%u", session.scrap());
    text(line, box.x + px(theme::kGutter), box.y + 16.0f, sz(theme::kTitle),
         theme::kScrap);
    text("SCRAP", box.x + px(theme::kGutter), box.y + 58.0f, sz(theme::kMicro),
         theme::kInkFaint);
    textRight("UPGRADES", box.x + box.width - px(theme::kGutter), box.y + 26.0f,
              sz(theme::kHeading), theme::kInkDim);
    std::snprintf(line, sizeof(line), "%u invested",
                  session.tree().totalSpent());
    textRight(line, box.x + box.width - px(theme::kGutter), box.y + 58.0f,
              sz(theme::kMicro), theme::kInkFaint);
    rule(box.x + px(theme::kGutter), box.y + 84.0f,
         box.width - px(theme::kGutter) * 2.0f, 0.7f);

    const float rowH = px(26.0f);
    const float listTop = box.y + px(96.0f);
    const float listH = box.height - px(150.0f);
    const int visible = std::max(1, static_cast<int>(listH / rowH));

    // Keyboard focus drags the window along with it; the bar and the wheel
    // move it directly.
    state.treeScroll =
        std::clamp(state.treeScroll, state.tree.index - visible + 1,
                   state.tree.index);
    const Rectangle list{box.x, listTop, box.width, listH};
    const Rectangle track{box.x + box.width - px(10.0f), listTop, px(5.0f),
                          listH};
    scrollbar(track, list, state.treeScroll, visible,
              static_cast<int>(kNodeCount));
    state.treeScroll = std::clamp(
        state.treeScroll, 0,
        std::max(0, static_cast<int>(kNodeCount) - visible));

    Result result;
    for (int v = 0; v < visible; ++v) {
        const int i = state.treeScroll + v;
        if (i >= static_cast<int>(kNodeCount)) break;
        const auto node = static_cast<NodeId>(i);
        const uint32_t lvl = session.tree().level(node);
        const bool owned = !isRepeatable(node) && lvl > 0u;
        const uint32_t cost = session.tree().cost(node);
        const bool afford = !owned && session.tree().canAfford(node, session.scrap());

        char levelText[16] = "";
        if (isRepeatable(node) && lvl > 0u) {
            std::snprintf(levelText, sizeof(levelText), "Lv%u", lvl);
        }

        const Rectangle r{box.x + px(theme::kUnit),
                          listTop + rowH * static_cast<float>(v),
                          box.width - px(theme::kUnit) * 2.0f - px(14.0f), rowH};

        // Carry the Battle Report's advice onto the shelf. Telling the player
        // what to buy and then making them remember it across a screen change
        // is where most of the value of Failure Analysis would leak away.
        bool suggested = false;
        for (int k = 0; k < session.failure().suggestionCount; ++k) {
            if (session.failure().suggestions[static_cast<size_t>(k)] == node) {
                suggested = true;
            }
        }
        if (suggested && !owned) {
            DrawRectangleV(Vector2{r.x - px(6.0f), r.y + rowH * 0.25f},
                           Vector2{px(3.0f), rowH * 0.5f}, theme::kScrap);
        }

        if (treeRow(r, kNodeNames[static_cast<size_t>(i)], levelText,
                    kNodeDesc[static_cast<size_t>(i)], cost, afford, owned,
                    state.tree, i)) {
            result = {Action::Buy, i};
        }
    }

    const float footer = box.y + box.height - 34.0f;
    text("enter buy  ·  X respec (free)  ·  R retry  ·  esc back",
         box.x + px(theme::kGutter), footer, sz(theme::kMicro), theme::kInkFaint);
    if (state.treeScroll + visible < static_cast<int>(kNodeCount)) {
        textRight("scroll for more", box.x + box.width - px(theme::kGutter),
                  footer, sz(theme::kMicro), theme::kInkFaint);
    }

    if (IsKeyPressed(KEY_X)) result = {Action::Respec, 0};
    if (IsKeyPressed(KEY_R)) result = {Action::Retry, 0};
    if (closeButton(box) || pressedBack()) result = {Action::Back, 0};
    return result;
}

}  // namespace ls::ui
