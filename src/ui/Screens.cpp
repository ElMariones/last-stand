#include "ui/Screens.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

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
    const int itemCount = resumable ? 4 : 3;
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

    // DEFAULT SPEED only means anything at 1, 2 or 4; snap 3 to the nearest.
    if (s.defaultTimeScale == 3) s.defaultTimeScale = 4;

    text("esc or the X to go back  ·  changes save immediately",
         box.x + px(theme::kGutter),
         box.y + box.height - px(theme::kUnit * 3.0f), sz(theme::kMicro),
         theme::kInkFaint);

    if (changed) session.applySettings();
    if (closeButton(box) || pressedBack()) return {Action::Back, 0};
    if (display) return {Action::ApplyDisplay, 0};
    return {};
}

// ---------------------------------------------------------- level select ---

Result drawLevelSelect(State& state, const Session& session) {
    scrim(0.85f);
    state.levels.begin(3);
    navigate(state.levels);

    const Rectangle box = column(700.0f, screenH() * 0.16f, row() * 7.5f);
    panelTitled(box, "SELECT SECTOR");

    struct Row { const char* name; const char* blurb; uint32_t power; };
    const Row rows[3] = {
        {"1 · THE OUTSKIRTS", "100 grunts, one lane. Where it starts.", 10u},
        {"2 · REFINERY GATE", "250, with runners. Rear-only coverage dies here.", 25u},
        {"3 · THE NARROWS",   "600, with tanks. Positioning beats raw damage.", 60u},
    };

    Result result;
    for (int i = 0; i < 3; ++i) {
        const Rectangle r{box.x + px(theme::kUnit),
                          box.y + px(theme::kUnit * 6.0f) +
                              row() * 1.45f * static_cast<float>(i),
                          box.width - px(theme::kUnit * 2.0f), row() * 1.35f};
        const bool isFocused = state.levels.isFocused(i);
        if (hovered(r)) state.levels.index = i;

        if (isFocused) {
            DrawRectangleRec(r, theme::withAlpha(theme::kCold, 0.10f));
            DrawRectangleV(Vector2{r.x, r.y}, Vector2{px(3.0f), r.height},
                           theme::kCold);
        }
        const bool cleared = session.clearCountFor(i) > 0u;
        text(rows[i].name, r.x + px(theme::kGutter), r.y + px(8.0f),
             sz(theme::kBody), isFocused ? theme::kInk : theme::kInkDim);
        text(rows[i].blurb, r.x + px(theme::kGutter), r.y + px(32.0f),
             sz(theme::kMicro), theme::kInkFaint);

        char stat[80];
        if (cleared) {
            std::snprintf(stat, sizeof(stat), "CLEARED x%u",
                          session.clearCountFor(i));
            textRight(stat, r.x + r.width - px(theme::kGutter), r.y + px(8.0f),
                      sz(theme::kSmall), theme::kGood);
        } else {
            std::snprintf(stat, sizeof(stat), "power %u", rows[i].power);
            textRight(stat, r.x + r.width - px(theme::kGutter), r.y + px(8.0f),
                      sz(theme::kSmall), theme::kInkFaint);
        }
        std::snprintf(stat, sizeof(stat), "best %u", session.bestKillsFor(i));
        textRight(stat, r.x + r.width - px(theme::kGutter), r.y + px(32.0f),
                  sz(theme::kMicro), theme::kInkDim);

        if ((isFocused && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))) ||
            (hovered(r) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))) {
            result = {Action::SelectLevel, i};
        }
    }

    text("esc to go back", box.x + px(theme::kGutter),
         box.y + box.height - px(theme::kUnit * 3.0f), sz(theme::kMicro),
         theme::kInkFaint);
    if (closeButton(box) || pressedBack()) return {Action::Back, 0};
    return result;
}

// --------------------------------------------------------------- prepare ---

// The hardpoint markers. Empty slots are dashed rings with a number; occupied
// ones get a filled ring. Without these the player has no way to know how many
// positions they have or which are free — and clicking felt like nothing was
// happening, because every slot arrived pre-filled with the only turret they
// had unlocked.
void drawHardpointOverlay(State& state, const Session& session) {
    const Vector2 mouse = GetMousePosition();
    state.hoveredHardpoint =
        session.hardpointNear(Vec2{mouse.x, mouse.y}, 28.0f);

    char badge[8];
    for (int i = 0; i < session.hardpointCount(); ++i) {
        const Vec2 hp = session.hardpointAt(i);
        const Vector2 c{hp.x, hp.y};
        const bool occupied = session.turretAtHardpoint(i) >= 0;
        const bool hot = state.hoveredHardpoint == i;

        const Color ring = occupied ? theme::kCold
                                    : (hot ? theme::kInk : theme::kInkFaint);
        DrawCircleLinesV(c, 20.0f, theme::withAlpha(ring, hot ? 1.0f : 0.75f));
        if (hot) DrawCircleLinesV(c, 24.0f, theme::withAlpha(ring, 0.4f));

        if (!occupied) {
            // An empty slot reads as an invitation: a plus, not a blank.
            DrawLineEx(Vector2{c.x - 6.0f, c.y}, Vector2{c.x + 6.0f, c.y}, 2.0f,
                       ring);
            DrawLineEx(Vector2{c.x, c.y - 6.0f}, Vector2{c.x, c.y + 6.0f}, 2.0f,
                       ring);
        }
        std::snprintf(badge, sizeof(badge), "%d", i + 1);
        DrawText(badge, static_cast<int>(c.x - 22.0f),
                 static_cast<int>(c.y - 30.0f), theme::kMicro,
                 theme::withAlpha(ring, 0.9f));
    }

    if (state.hoveredHardpoint >= 0) {
        const bool occupied =
            session.turretAtHardpoint(state.hoveredHardpoint) >= 0;
        const char* hint = occupied ? "click to replace  ·  right-click to clear"
                                    : "click to deploy";
        DrawText(hint, static_cast<int>(mouse.x) + 18,
                 static_cast<int>(mouse.y) + 8, theme::kMicro, theme::kInkDim);
    }
}

Result drawPrepareHud(State& state, const Session& session) {
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

    // The number the player was missing: how many of their positions are
    // manned, and how many exist at all.
    const int placed = session.turretCount();
    const int slots = session.hardpointCount();
    std::snprintf(line, sizeof(line), "%d / %d DEPLOYED", placed, slots);
    text(line, px(theme::kGutter), bar.y + px(62.0f), sz(theme::kSmall),
         placed == 0 ? theme::kDanger
                     : (placed < slots ? theme::kScrap : theme::kGood));

    // Turret picker: all three kinds always visible, locked ones dimmed, so
    // the player can see what the tree is for.
    struct Kind { TurretKind kind; const char* name; const char* stat; };
    const Kind kinds[3] = {
        {TurretKind::MachineGun,   "1 MACHINE GUN", "fast, single target"},
        {TurretKind::Cannon,       "2 CANNON",      "slow, heavy splash"},
        {TurretKind::Flamethrower, "3 FLAMETHROWER","cone, burns over time"},
    };
    Result result;
    float x = screenW() * 0.30f;
    for (const Kind& k : kinds) {
        const bool unlocked = session.isKindUnlocked(k.kind);
        const bool selected = session.selectedKind() == k.kind;
        const Rectangle r{x, bar.y + px(14.0f), px(196.0f), px(52.0f)};
        DrawRectangleRec(r, theme::withAlpha(Color{20, 19, 23, 255},
                                             selected ? 1.0f : 0.55f));
        DrawRectangleLinesEx(r, 1.0f,
                             selected ? theme::kCold
                                      : theme::withAlpha(theme::kColdDeep, 0.9f));
        text(k.name, r.x + px(10.0f), r.y + px(8.0f), sz(theme::kMicro),
             !unlocked ? theme::kInkFaint
                       : (selected ? theme::kInk : theme::kInkDim));
        text(unlocked ? k.stat : "LOCKED - buy it in the tree",
             r.x + px(10.0f), r.y + px(28.0f), sz(theme::kMicro),
             unlocked ? theme::kInkFaint : theme::kInkFaint);
        if (unlocked && hovered(r) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            result = {Action::SelectKind, static_cast<int>(k.kind)};
        }
        x += px(204.0f);
    }

    // DEPLOY is always available, even with empty slots — the player is
    // allowed to try a sector understaffed, and telling them "no" mid-flow is
    // worse than letting them find out.
    const Rectangle deploy{screenW() - px(200.0f), bar.y + px(14.0f),
                           px(180.0f), px(40.0f)};
    const bool ready = placed > 0;
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

    textRight("F fill  ·  C clear  ·  M targeting  ·  L sectors  ·  esc menu",
              screenW() - px(theme::kGutter), bar.y + px(66.0f),
              sz(theme::kMicro), theme::kInkFaint);

    drawHardpointOverlay(state, session);

    if (IsKeyPressed(KEY_ONE)) return {Action::SelectKind, 0};
    if (IsKeyPressed(KEY_TWO)) return {Action::SelectKind, 1};
    if (IsKeyPressed(KEY_THREE)) return {Action::SelectKind, 2};
    if (IsKeyPressed(KEY_F)) return {Action::FillHardpoints, 0};
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

    std::snprintf(line, sizeof(line), "S   %dx", session.timeScale());
    textRight(line, screenW() - px(theme::kGutter), y + px(8.0f),
              sz(theme::kSmall),
              session.timeScale() > 1 ? theme::kCold : theme::kInkDim);
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

    if (p.newBest) {
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
    state.report.begin(3);
    navigate(state.report);

    Result result;
    const float bw = (box.width - px(theme::kGutter) * 2.0f) / 3.0f;
    if (button(Rectangle{box.x + px(theme::kGutter), footer, bw, 40.0f},
               "R  RETRY", state.report, 0)) {
        result = {Action::Retry, 0};
    }
    if (button(Rectangle{box.x + px(theme::kGutter) + bw, footer, bw, 40.0f},
               "U  UPGRADE", state.report, 1)) {
        result = {Action::OpenTree, 0};
    }
    if (button(Rectangle{box.x + px(theme::kGutter) + bw * 2.0f, footer, bw, 40.0f},
               "P  PREPARE", state.report, 2)) {
        result = {Action::BackToPrepare, 0};
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

    // Keep the focused row on screen without a scrollbar: the list is 24 rows
    // and the window shows most of them.
    const float rowH = 26.0f;
    const int visible =
        std::max(1, static_cast<int>((box.height - 150.0f) / rowH));
    state.treeScroll =
        std::clamp(state.treeScroll, state.tree.index - visible + 2,
                   state.tree.index);
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
                          box.y + 96.0f + rowH * static_cast<float>(v),
                          box.width - px(theme::kUnit) * 2.0f, rowH};

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
        textRight("more below", box.x + box.width - px(theme::kGutter), footer,
                  sz(theme::kMicro), theme::kInkFaint);
    }

    if (IsKeyPressed(KEY_X)) result = {Action::Respec, 0};
    if (IsKeyPressed(KEY_R)) result = {Action::Retry, 0};
    if (closeButton(box) || pressedBack()) result = {Action::Back, 0};
    return result;
}

}  // namespace ls::ui
