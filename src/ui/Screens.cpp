#include "ui/Screens.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "render/Theme.h"

namespace ls::ui {

namespace {

constexpr float kRow = 44.0f;

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

const char* const kKindName[3] = {"MACHINE GUN", "CANNON", "FLAMETHROWER"};

float screenW() { return static_cast<float>(GetScreenWidth()); }
float screenH() { return static_cast<float>(GetScreenHeight()); }

// A centred column, the layout every menu screen uses.
Rectangle column(float width, float top, float height) {
    return Rectangle{(screenW() - width) * 0.5f, top, width, height};
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
    const float top = screenH() * 0.28f;

    // A slow breathing glow behind the wordmark, so the title screen is not
    // a still image over a moving battle.
    const float pulse = 0.5f + 0.5f * std::sin(state.titleClock * 1.2f);
    DrawRectangle(0, static_cast<int>(top - 18.0f), GetScreenWidth(), 96,
                  theme::withAlpha(theme::kColdDeep, 0.15f + pulse * 0.10f));

    textCentered("LAST STAND", cx, top, theme::kDisplay, theme::kInk);
    textCentered("you don't need to survive forever.", cx,
                 top + 78.0f, theme::kSmall, theme::kInkDim);
    textCentered("you just need to get stronger faster than they do.", cx,
                 top + 98.0f, theme::kSmall, theme::kInkDim);

    const float blink = 0.45f + 0.55f * std::sin(state.titleClock * 3.0f);
    textCentered("PRESS ENTER", cx, screenH() * 0.72f, theme::kBody,
                 theme::withAlpha(theme::kCold, blink));

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        return {Action::Play, 0};
    }
    return {};
}

// ------------------------------------------------------------------ menu ---

Result drawMenu(State& state, const Session& session) {
    scrim(0.82f);
    state.menu.begin(3);
    navigate(state.menu);

    const float cx = screenW() * 0.5f;
    textCentered("LAST STAND", cx, screenH() * 0.18f, theme::kTitle,
                 theme::kInk);

    char line[96];
    std::snprintf(line, sizeof(line), "SCRAP %u", session.scrap());
    textCentered(line, cx, screenH() * 0.18f + 52.0f, theme::kSmall,
                 theme::kScrap);

    const Rectangle box = column(360.0f, screenH() * 0.40f, kRow * 3.0f);
    Result result;
    if (button(Rectangle{box.x, box.y, box.width, kRow}, "DEPLOY",
               state.menu, 0)) {
        result = {Action::Play, 0};
    }
    if (button(Rectangle{box.x, box.y + kRow, box.width, kRow}, "OPTIONS",
               state.menu, 1)) {
        result = {Action::Options, 0};
    }
    if (button(Rectangle{box.x, box.y + kRow * 2.0f, box.width, kRow}, "QUIT",
               state.menu, 2)) {
        result = {Action::Quit, 0};
    }

    textCentered("arrows or mouse to choose  ·  enter to confirm", cx,
                 screenH() - 56.0f, theme::kMicro, theme::kInkFaint);
    return result;
}

// --------------------------------------------------------------- options ---

Result drawOptions(State& state, Session& session) {
    scrim(0.88f);
    state.options.begin(9);
    navigate(state.options);

    Settings& s = session.settings();
    const Rectangle box = column(620.0f, screenH() * 0.14f, kRow * 10.5f);
    panelTitled(box, "OPTIONS");

    float y = box.y + theme::kUnit * 6.0f;
    const Rectangle row{box.x + theme::kUnit, y, box.width - theme::kUnit * 2.0f,
                        kRow};
    const auto at = [&](int i) {
        return Rectangle{row.x, row.y + kRow * static_cast<float>(i), row.width,
                         kRow};
    };

    bool changed = false;
    changed |= slider(at(0), "MASTER VOLUME", s.masterVolume, 0, 100, 5,
                      state.options, 0);
    changed |= slider(at(1), "EFFECTS VOLUME", s.sfxVolume, 0, 100, 5,
                      state.options, 1);
    changed |= slider(at(2), "MUSIC VOLUME", s.musicVolume, 0, 100, 5,
                      state.options, 2);
    changed |= slider(at(3), "SCREEN SHAKE", s.shakeScale, 0, 200, 10,
                      state.options, 3);
    changed |= slider(at(4), "DEFAULT SPEED", s.defaultTimeScale, 1, 4, 1,
                      state.options, 4);
    changed |= toggle(at(5), "HITSTOP", s.hitstop, state.options, 5);
    changed |= toggle(at(6), "DAMAGE NUMBERS", s.damageNumbers, state.options, 6);
    changed |= toggle(at(7), "LEVEL OF DETAIL", s.levelOfDetail, state.options, 7);
    changed |= toggle(at(8), "DEBUG OVERLAY", s.debugOverlay, state.options, 8);

    // DEFAULT SPEED only means anything at 1, 2 or 4; snap 3 to the nearest.
    if (s.defaultTimeScale == 3) s.defaultTimeScale = 4;

    text("esc to go back  ·  changes save immediately",
         box.x + theme::kGutter, box.y + box.height - theme::kUnit * 3.0f,
         theme::kMicro, theme::kInkFaint);

    if (changed) session.applySettings();
    if (pressedBack()) return {Action::Back, 0};
    return {};
}

// ---------------------------------------------------------- level select ---

Result drawLevelSelect(State& state, const Session& session) {
    scrim(0.85f);
    state.levels.begin(3);
    navigate(state.levels);

    const Rectangle box = column(680.0f, screenH() * 0.18f, kRow * 7.0f);
    panelTitled(box, "SELECT SECTOR");

    struct Row { const char* name; const char* blurb; uint32_t power; };
    const Row rows[3] = {
        {"1 · THE OUTSKIRTS", "100 grunts. A single lane. Where it starts.", 10u},
        {"2 · REFINERY GATE", "250, with runners. Rear-only coverage dies here.", 25u},
        {"3 · THE NARROWS",   "600, with tanks. AoE positioning beats raw DPS.", 60u},
    };

    Result result;
    for (int i = 0; i < 3; ++i) {
        const Rectangle r{box.x + theme::kUnit,
                          box.y + theme::kUnit * 6.0f + kRow * 1.4f *
                                                            static_cast<float>(i),
                          box.width - theme::kUnit * 2.0f, kRow * 1.3f};
        const bool isFocused = state.levels.isFocused(i);
        if (hovered(r)) state.levels.index = i;

        if (isFocused) {
            DrawRectangleRec(r, theme::withAlpha(theme::kCold, 0.10f));
            DrawRectangleV(Vector2{r.x, r.y}, Vector2{3.0f, r.height},
                           theme::kCold);
        }
        text(rows[i].name, r.x + theme::kGutter, r.y + 8.0f, theme::kBody,
             isFocused ? theme::kInk : theme::kInkDim);
        text(rows[i].blurb, r.x + theme::kGutter, r.y + 32.0f, theme::kMicro,
             theme::kInkFaint);

        char best[64];
        std::snprintf(best, sizeof(best), "best %u",
                      session.bestKills());
        (void)best;
        char power[48];
        std::snprintf(power, sizeof(power), "power %u", rows[i].power);
        textRight(power, r.x + r.width - theme::kGutter, r.y + 14.0f,
                  theme::kSmall, theme::kInkFaint);

        if ((isFocused && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))) ||
            (hovered(r) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))) {
            result = {Action::SelectLevel, i};
        }
    }

    text("esc to go back", box.x + theme::kGutter,
         box.y + box.height - theme::kUnit * 3.0f, theme::kMicro,
         theme::kInkFaint);
    if (pressedBack()) return {Action::Back, 0};
    return result;
}

// --------------------------------------------------------------- prepare ---

Result drawPrepareHud(State& state, const Session& session) {
    (void)state;
    const Rectangle bar{0.0f, screenH() - 84.0f, screenW(), 84.0f};
    DrawRectangleRec(bar, theme::withAlpha(Color{12, 11, 14, 255}, 0.94f));
    rule(0.0f, bar.y, screenW(), 0.8f);

    char line[160];
    std::snprintf(line, sizeof(line), "SECTOR %d · %s",
                  session.levelIndex() + 1, session.level().name.c_str());
    text(line, theme::kGutter, bar.y + 14.0f, theme::kBody, theme::kInk);

    std::snprintf(line, sizeof(line), "%u incoming   ·   best %u",
                  session.level().totalEnemies, session.bestKills());
    text(line, theme::kGutter, bar.y + 42.0f, theme::kMicro, theme::kInkDim);

    std::snprintf(line, sizeof(line), "PLACING  %s",
                  kKindName[static_cast<int>(session.selectedKind())]);
    textCentered(line, screenW() * 0.5f, bar.y + 20.0f, theme::kBody,
                 theme::kCold);
    textCentered("click a hardpoint  ·  TAB kind  ·  M targeting",
                 screenW() * 0.5f, bar.y + 48.0f, theme::kMicro,
                 theme::kInkFaint);

    textRight("SPACE  DEPLOY", screenW() - theme::kGutter, bar.y + 20.0f,
              theme::kBody, theme::kGood);
    textRight("L level select  ·  esc menu", screenW() - theme::kGutter,
              bar.y + 48.0f, theme::kMicro, theme::kInkFaint);

    if (IsKeyPressed(KEY_L)) return {Action::SelectLevel, -1};
    if (pressedBack()) return {Action::ToMenu, 0};
    if (IsKeyPressed(KEY_TAB)) return {Action::CycleKind, 0};
    if (IsKeyPressed(KEY_M)) return {Action::CycleTargeting, 0};
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
        return {Action::StartBattle, 0};
    }
    return {};
}

// ---------------------------------------------------------------- battle ---

Result drawBattleHud(State& state, const Session& session) {
    (void)state;
    const World* w = session.world();
    if (w == nullptr) return {};

    // Base health, top centre, big enough to be read out of the corner of an
    // eye while the battlefield has the player's attention.
    const float barW = 360.0f;
    const Rectangle hp{(screenW() - barW) * 0.5f, 18.0f, barW, 10.0f};
    const float frac = (w->base().maxHealth > 0.0f)
                           ? w->base().health / w->base().maxHealth
                           : 0.0f;
    bar(hp, frac, frac > 0.35f ? theme::kCold : theme::kDanger,
        theme::withAlpha(theme::kColdDeep, 0.8f));

    char line[160];
    std::snprintf(line, sizeof(line), "BASE  %.0f%%", frac * 100.0f);
    textCentered(line, screenW() * 0.5f, hp.y + 16.0f, theme::kMicro,
                 frac > 0.35f ? theme::kInkDim : theme::kDanger);

    // Kills and scrap, top right. The Scrap figure is where the arcs fly to.
    std::snprintf(line, sizeof(line), "%u", session.scrap());
    textRight(line, screenW() - theme::kGutter, 16.0f, theme::kHeading,
              theme::kScrap);
    textRight("SCRAP", screenW() - theme::kGutter, 46.0f, theme::kMicro,
              theme::kInkFaint);

    std::snprintf(line, sizeof(line), "%u / %u", w->totalKills(),
                  session.level().totalEnemies);
    text(line, theme::kGutter, 16.0f, theme::kHeading, theme::kInk);
    text("DESTROYED", theme::kGutter, 46.0f, theme::kMicro, theme::kInkFaint);

    // Abilities and time control, bottom left: a cooldown you can see is a
    // decision; a cooldown you have to remember is a chore.
    float x = theme::kGutter;
    const float y = screenH() - 46.0f;
    const auto pill = [&](const char* key, const char* name, bool unlocked,
                          float cd, float full) {
        const Rectangle r{x, y, 132.0f, 30.0f};
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
        std::snprintf(buf, sizeof(buf), "%s  %s", key, name);
        text(buf, r.x + 10.0f, r.y + 8.0f, theme::kMicro,
             !unlocked ? theme::kInkFaint
                       : (ready ? theme::kInk : theme::kInkDim));
        x += r.width + theme::kUnit;
    };
    pill("A", "AIRSTRIKE", session.airstrikeReady() ||
                               session.airstrikeCooldown() > 0.0f,
         session.airstrikeCooldown(), 25.0f);
    pill("O", "OVERCHARGE", session.overchargeReady() ||
                                session.overchargeCooldown() > 0.0f,
         session.overchargeCooldown(), 15.0f);

    std::snprintf(line, sizeof(line), "S   %dx", session.timeScale());
    textRight(line, screenW() - theme::kGutter, y + 8.0f, theme::kSmall,
              session.timeScale() > 1 ? theme::kCold : theme::kInkDim);
    textRight("esc pause", screenW() - theme::kGutter, y - 14.0f, theme::kMicro,
              theme::kInkFaint);

    if (pressedBack()) return {Action::Resume, 0};   // esc opens the pause menu
    return {};
}

// ----------------------------------------------------------------- pause ---

Result drawPause(State& state, const Session& session) {
    (void)session;
    scrim(0.78f);
    state.pause.begin(3);
    navigate(state.pause);

    const float cx = screenW() * 0.5f;
    textCentered("PAUSED", cx, screenH() * 0.28f, theme::kTitle, theme::kInk);

    const Rectangle box = column(320.0f, screenH() * 0.44f, kRow * 3.0f);
    Result result;
    if (button(Rectangle{box.x, box.y, box.width, kRow}, "RESUME", state.pause, 0)) {
        result = {Action::Resume, 0};
    }
    if (button(Rectangle{box.x, box.y + kRow, box.width, kRow}, "OPTIONS",
               state.pause, 1)) {
        result = {Action::Options, 0};
    }
    if (button(Rectangle{box.x, box.y + kRow * 2.0f, box.width, kRow},
               "ABANDON", state.pause, 2)) {
        result = {Action::Abandon, 0};
    }

    textCentered("abandoning pays nothing", cx, box.y + kRow * 3.4f,
                 theme::kMicro, theme::kInkFaint);
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

    const Rectangle box = column(760.0f, 48.0f, screenH() - 130.0f);
    panel(box);

    const float cx = box.x + box.width * 0.5f;
    float y = box.y + 26.0f;

    textCentered(r.victory ? "SECTOR HELD" : "OVERRUN", cx, y, theme::kTitle,
                 r.victory ? theme::kCold : theme::kDanger);
    y += 54.0f;

    char line[192];
    std::snprintf(line, sizeof(line), "%u / %u destroyed", counted(r.kills),
                  r.totalEnemies);
    textCentered(line, cx, y, theme::kHeading, theme::kInk);
    y += 32.0f;

    char clock[16];
    formatTime(t.elapsedSeconds(), clock, sizeof(clock));
    std::snprintf(line, sizeof(line), "%s survived  ·  peak %.1f kills/sec",
                  clock, static_cast<double>(t.peakKillsPerSecond()));
    textCentered(line, cx, y, theme::kSmall, theme::kInkDim);
    y += 34.0f;

    // The payout, boxed, because it is the reason the player is on this
    // screen at all.
    const Rectangle payoutBox{cx - 170.0f, y, 340.0f, 62.0f};
    DrawRectangleRec(payoutBox, theme::withAlpha(theme::kColdDeep, 0.35f));
    DrawRectangleLinesEx(payoutBox, 1.0f, theme::withAlpha(theme::kScrap, 0.5f));
    std::snprintf(line, sizeof(line), "+ %u SCRAP", counted(p.scrap));
    textCentered(line, cx, y + 10.0f, theme::kTitle, theme::kScrap);
    std::snprintf(line, sizeof(line), "kills %u   depth %u   best %u   x%.2f",
                  p.killScrap, p.depthScrap, p.bestBonus,
                  static_cast<double>(p.multiplier));
    textCentered(line, cx, y + 44.0f, theme::kMicro, theme::kInkDim);
    y += 74.0f;

    if (p.newBest) {
        std::snprintf(line, sizeof(line), "NEW BEST   ·   was %u",
                      r.previousBest);
        textCentered(line, cx, y, theme::kBody, theme::kGood);
    }
    y += 34.0f;

    rule(box.x + theme::kGutter, y, box.width - theme::kGutter * 2.0f, 0.7f);
    y += 16.0f;

    // Failure Analysis: where, when, and by how much (GDD 13.2).
    text(fa.victory ? "AFTER ACTION" : "FAILURE ANALYSIS",
         box.x + theme::kGutter, y, theme::kSmall, theme::kCold);
    y += 26.0f;

    if (t.breached()) {
        char breach[16];
        formatTime(fa.breachTime, breach, sizeof(breach));
        std::snprintf(line, sizeof(line), "Breach at %s, %s", fa.lane, breach);
    } else {
        std::snprintf(line, sizeof(line), "No breach. The line held.");
    }
    text(line, box.x + theme::kGutter, y, theme::kSmall, theme::kInk);
    y += 24.0f;

    const auto stat = [&](const char* label, const char* value, Color ink) {
        text(label, box.x + theme::kGutter, y, theme::kMicro, theme::kInkFaint);
        text(value, box.x + theme::kGutter + 240.0f, y, theme::kMicro, ink);
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
        std::snprintf(line, sizeof(line), "> suggested:  %s  (%s)",
                      kNodeNames[n], kNodeDesc[n]);
        text(line, box.x + theme::kGutter, y, theme::kSmall, theme::kScrap);
        y += 22.0f;
    }

    y += 10.0f;
    rule(box.x + theme::kGutter, y, box.width - theme::kGutter * 2.0f, 0.7f);
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
        text(src.name, box.x + theme::kGutter, y, theme::kMicro,
             theme::kInkDim);
        const Rectangle track{box.x + theme::kGutter + 140.0f, y + 4.0f, 300.0f,
                              8.0f};
        bar(track, attr.share(src.kills) * reveal, src.color,
            theme::withAlpha(theme::kColdDeep, 0.6f));
        std::snprintf(value, sizeof(value), "%3.0f%%",
                      static_cast<double>(attr.share(src.kills) * 100.0f));
        text(value, track.x + track.width + 14.0f, y, theme::kMicro,
             theme::kInkDim);
        y += 20.0f;
    }

    // The retry path is the most optimised path in the game: always the same
    // place, always one key, never behind a confirmation (GDD 13.1).
    const float footer = box.y + box.height - 56.0f;
    rule(box.x + theme::kGutter, footer - 12.0f,
         box.width - theme::kGutter * 2.0f, 0.7f);
    state.report.begin(3);
    navigate(state.report);

    Result result;
    const float bw = (box.width - theme::kGutter * 2.0f) / 3.0f;
    if (button(Rectangle{box.x + theme::kGutter, footer, bw, 40.0f},
               "R  RETRY", state.report, 0)) {
        result = {Action::Retry, 0};
    }
    if (button(Rectangle{box.x + theme::kGutter + bw, footer, bw, 40.0f},
               "U  UPGRADE", state.report, 1)) {
        result = {Action::OpenTree, 0};
    }
    if (button(Rectangle{box.x + theme::kGutter + bw * 2.0f, footer, bw, 40.0f},
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
    text(line, box.x + theme::kGutter, box.y + 16.0f, theme::kTitle,
         theme::kScrap);
    text("SCRAP", box.x + theme::kGutter, box.y + 58.0f, theme::kMicro,
         theme::kInkFaint);
    textRight("UPGRADES", box.x + box.width - theme::kGutter, box.y + 26.0f,
              theme::kHeading, theme::kInkDim);
    rule(box.x + theme::kGutter, box.y + 84.0f,
         box.width - theme::kGutter * 2.0f, 0.7f);

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

        const Rectangle r{box.x + theme::kUnit,
                          box.y + 96.0f + rowH * static_cast<float>(v),
                          box.width - theme::kUnit * 2.0f, rowH};
        if (treeRow(r, kNodeNames[static_cast<size_t>(i)], levelText,
                    kNodeDesc[static_cast<size_t>(i)], cost, afford, owned,
                    state.tree, i)) {
            result = {Action::Buy, i};
        }
    }

    const float footer = box.y + box.height - 34.0f;
    text("enter buy  ·  X respec  ·  R retry  ·  esc back",
         box.x + theme::kGutter, footer, theme::kMicro, theme::kInkFaint);

    if (IsKeyPressed(KEY_X)) result = {Action::Respec, 0};
    if (IsKeyPressed(KEY_R)) result = {Action::Retry, 0};
    if (pressedBack()) result = {Action::Back, 0};
    return result;
}

}  // namespace ls::ui
