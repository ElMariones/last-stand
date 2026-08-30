#include <chrono>
#include <cstdio>
#include <string>
#include <raylib.h>

#include "app/Bench.h"
#include "app/Cli.h"
#include "app/Session.h"
#include "core/FixedTimestep.h"
#include "render/Renderer.h"

namespace {

const char* kSavePath = "laststand.save";

const char* kNodeNames[ls::kNodeCount] = {
    "Damage",         "Fire Rate",     "Range",          "Base HP",
    "Base Regen",     "Economy",       "Splash",         "Burn",
    "Cannon",         "Flamethrower",  "Extra Hardpoint","DENSEST",
    "MG Overclock",   "MG Ricochet",   "MG Bullet Storm","Explosive Shells",
    "Knockback",      "Cluster Shot",  "Lingering Flames","Ignite",
    "Firestorm",      "Airstrike",     "Overcharge",     "Armor Piercing",
};

const char* kNodeDesc[ls::kNodeCount] = {
    "+20% dmg/lv",    "+15% rate/lv",  "+12% range/lv", "+300 HP/lv",
    "+2 HP/s/lv",     "+20% scrap/lv", "+15% splash/lv","+20% burn/lv",
    "unlock Cannon",  "unlock Flame",  "+1 hardpoint",  "unlock DENSEST",
    "MG 2x/0.7x",     "MG +1 bounce",  "MG 20th spread","splash +50%",
    "knockback +150%","4 sub-blasts",  "burn lasts 2x", "burn spreads",
    "burn 2x",        "unlock strike", "unlock overchg", "+50% vs Tank",
};

const char* kKindName[3] = {"Machine Gun", "Cannon", "Flamethrower"};

void drawReport(const ls::Session& s) {
    const ls::Payout& p = s.payout();
    const ls::BattleResult& r = s.result();
    const int cx = 300;
    int y = 140;

    DrawText(r.victory ? "VICTORY" : "DEFEATED", cx, y, 48,
             r.victory ? Color{150, 220, 255, 255}
                       : Color{255, 90, 70, 255});
    y += 56;

    char line[256];
    std::snprintf(line, sizeof(line), "%u / %u destroyed", r.kills,
                  r.totalEnemies);
    DrawText(line, cx, y, 26, Color{220, 230, 245, 255});
    y += 34;

    std::snprintf(line, sizeof(line), "+ %u SCRAP", p.scrap);
    DrawText(line, cx, y, 30, Color{255, 220, 120, 255});
    y += 36;

    std::snprintf(line, sizeof(line), "kill %u   depth %u   best %u",
                  p.killScrap, p.depthScrap, p.bestBonus);
    DrawText(line, cx, y, 18, Color{180, 195, 210, 255});
    y += 28;

    if (p.newBest) {
        std::snprintf(line, sizeof(line), "NEW BEST  (was %u)", r.previousBest);
        DrawText(line, cx, y, 20, Color{150, 255, 170, 255});
        y += 30;
    }

    DrawText("[R] retry    [U] upgrade    [P] prepare", cx, y + 20, 18,
             Color{140, 150, 165, 255});
}

void drawTree(const ls::Session& s, int selected) {
    const int x = 260;
    int y = 70;
    char line[256];

    std::snprintf(line, sizeof(line), "SCRAP  %u", s.scrap());
    DrawText(line, x, y, 30, Color{255, 220, 120, 255});
    y += 40;

    for (size_t i = 0; i < ls::kNodeCount; ++i) {
        const auto node = static_cast<ls::NodeId>(i);
        const uint32_t lvl = s.tree().level(node);
        const uint32_t cost = s.tree().cost(node);
        const bool afford = s.tree().canAfford(node, s.scrap());
        const bool oneShot = !ls::isRepeatable(node) && lvl > 0u;
        const std::string lvlText =
            oneShot ? "OWNED" : (lvl > 0u ? "Lv" + std::to_string(lvl) : "");

        char buf[256];
        std::snprintf(buf, sizeof(buf), "%s %-16s %-6s cost %-6u %s",
                      (static_cast<int>(i) == selected) ? ">" : " ",
                      kNodeNames[i], lvlText.c_str(), cost, kNodeDesc[i]);
        DrawText(buf, x, y, 17,
                 afford ? Color{200, 230, 255, 255}
                        : Color{90, 95, 105, 255});
        y += 22;
    }

    DrawText("[UP]/[DOWN] select   [ENTER] buy   [X] respec   [R] retry   [ESC] report",
             x, 680, 16, Color{140, 150, 165, 255});
}

}  // namespace

int main(int argc, char** argv) {
    const ls::Options options = ls::parseArgs(argc, argv);

    if (options.help) {
        std::printf("%s", ls::usageText());
        return 0;
    }

    if (options.sweepPath != nullptr) {
        return ls::runSweep(options) ? 0 : 1;
    }

    if (options.bench || options.noRender) {
        const ls::BenchResult r = ls::runBench(options);
        ls::printBench(r);
        return 0;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "LAST STAND");
    SetTargetFPS(0);

    ls::Session session(kSavePath);
    ls::FixedTimestep timestep{60.0, 0.25};
    ls::Renderer renderer;
    ls::DebugFlags flags;

    ls::Phase prevPhase = ls::Phase::Prepare;
    double lastTickMs = 0.0;
    int selectedNode = 0;

    const bool shotMode = options.shotTicks > 0u;
    if (shotMode) session.startBattle();

    while (!WindowShouldClose()) {
        // --- input ----------------------------------------------------------
        if (IsKeyPressed(KEY_F)) flags.showFlowField = !flags.showFlowField;
        if (IsKeyPressed(KEY_G)) flags.showGrid = !flags.showGrid;
        if (IsKeyPressed(KEY_T)) flags.showTurretRange = !flags.showTurretRange;

        switch (session.phase()) {
            case ls::Phase::Prepare:
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    const Vector2 m = GetMousePosition();
                    session.setTurretAt(ls::Vec2{m.x, m.y}, 12.0f, 12.0f);
                }
                if (IsKeyPressed(KEY_TAB)) session.cycleKind();
                if (IsKeyPressed(KEY_M)) session.cycleTargeting();
                if (IsKeyPressed(KEY_ONE)) session.selectLevel(0);
                if (IsKeyPressed(KEY_TWO)) session.selectLevel(1);
                if (IsKeyPressed(KEY_THREE)) session.selectLevel(2);
                if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
                    session.startBattle();
                }
                break;
            case ls::Phase::Battle:
                if (IsKeyPressed(KEY_S)) session.cycleTimeScale();
                if (IsKeyPressed(KEY_A)) session.fireAirstrike();
                if (IsKeyPressed(KEY_O)) {
                    const Vector2 m = GetMousePosition();
                    session.overchargeAt(ls::Vec2{m.x, m.y});
                }
                break;
            case ls::Phase::Report:
                if (IsKeyPressed(KEY_R)) session.retry();
                if (IsKeyPressed(KEY_U)) session.openTree();
                if (IsKeyPressed(KEY_P)) session.backToPrepare();
                break;
            case ls::Phase::Tree:
                if (IsKeyPressed(KEY_UP)) selectedNode = (selectedNode + 23) % 24;
                if (IsKeyPressed(KEY_DOWN)) selectedNode = (selectedNode + 1) % 24;
                if (IsKeyPressed(KEY_ENTER)) {
                    session.buy(static_cast<ls::NodeId>(selectedNode));
                }
                if (IsKeyPressed(KEY_X)) session.respec();
                if (IsKeyPressed(KEY_R)) session.retry();
                if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_ESCAPE)) {
                    session.backToReport();
                }
                break;
        }

        // --- simulation ----------------------------------------------------
        if (session.phase() == ls::Phase::Battle) {
            if (prevPhase != ls::Phase::Battle) timestep.reset();

            const double scale = static_cast<double>(session.timeScale());
            const double frameSeconds =
                shotMode ? timestep.tickSeconds()
                         : static_cast<double>(GetFrameTime()) * scale;
            const int ticks = timestep.advance(frameSeconds);
            if (ticks > 0) {
                const auto t0 = std::chrono::steady_clock::now();
                for (int i = 0; i < ticks; ++i) {
                    session.updateBattle(static_cast<float>(timestep.tickSeconds()));
                }
                const auto t1 = std::chrono::steady_clock::now();
                const double totalMs =
                    std::chrono::duration<double, std::milli>(t1 - t0).count();
                lastTickMs = totalMs / static_cast<double>(ticks);
            }
        }
        prevPhase = session.phase();

        // --- render ---------------------------------------------------------
        BeginDrawing();
        ClearBackground(Color{12, 10, 10, 255});

        if (session.world() != nullptr) {
            renderer.draw(*session.world(),
                          static_cast<float>(timestep.alpha()), flags,
                          static_cast<double>(GetFrameTime()) * 1000.0,
                          lastTickMs);
        }

        const bool battlePhases = session.phase() == ls::Phase::Battle ||
                                  session.phase() == ls::Phase::Prepare;
        char hint[192];
        if (battlePhases) {
            std::snprintf(hint, sizeof(hint),
                          "L%d %s  kind %s  speed %dx  scrap %u  best %u",
                          session.levelIndex() + 1, session.level().name.c_str(),
                          kKindName[static_cast<int>(session.selectedKind())],
                          session.timeScale(), session.scrap(), session.bestKills());
        } else {
            std::snprintf(hint, sizeof(hint), "scrap %u   best %u",
                          session.scrap(), session.bestKills());
        }
        DrawText(hint, 12, 700, 16, Color{140, 150, 165, 255});

        if (!battlePhases) {
            const Vector2 s = {static_cast<float>(GetScreenWidth()),
                               static_cast<float>(GetScreenHeight())};
            DrawRectangleV(Vector2{0, 0}, s, Color{12, 10, 10, 235});
            if (session.phase() == ls::Phase::Report) drawReport(session);
            else if (session.phase() == ls::Phase::Tree) drawTree(session, selectedNode);
        }

        EndDrawing();

        if (shotMode && timestep.totalTicks() >= options.shotTicks) {
            TakeScreenshot("shot.png");
            break;
        }
    }

    session.saveNow();
    CloseWindow();
    return 0;
}
