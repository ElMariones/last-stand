#include <chrono>
#include <cstdio>
#include <raylib.h>

#include "app/Bench.h"
#include "app/Cli.h"
#include "app/Session.h"
#include "core/FixedTimestep.h"
#include "render/Renderer.h"

namespace {

const char* kSavePath = "laststand.save";

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

void drawTree(const ls::Session& s) {
    static const char* kNames[ls::kNodeCount] = {
        "Damage", "Fire Rate", "Range", "Base HP", "Base Regen", "Economy"};
    static const char* kDesc[ls::kNodeCount] = {
        "x1.20 dmg", "x1.15 rate", "x1.12 range",
        "+300 HP", "+2 HP/s", "x1.20 scrap"};

    const int x = 300;
    int y = 100;
    char line[256];

    std::snprintf(line, sizeof(line), "SCRAP  %u", s.scrap());
    DrawText(line, x, y, 32, Color{255, 220, 120, 255});
    y += 48;

    for (size_t i = 0; i < ls::kNodeCount; ++i) {
        const auto node = static_cast<ls::NodeId>(i);
        const uint32_t lvl = s.tree().level(node);
        const uint32_t cost = s.tree().cost(node);
        const bool afford = s.tree().canAfford(node, s.scrap());

        std::snprintf(line, sizeof(line), "[%d] %-11s Lv%u  %s  cost %u",
                      static_cast<int>(i + 1), kNames[i], lvl, kDesc[i], cost);
        DrawText(line, x, y, 20,
                 afford ? Color{200, 230, 255, 255}
                        : Color{90, 95, 105, 255});
        y += 26;
    }

    y += 12;
    DrawText("[X] respec all    [R] retry    [ESC] report", x, y, 18,
             Color{140, 150, 165, 255});
}

}  // namespace

int main(int argc, char** argv) {
    const ls::Options options = ls::parseArgs(argc, argv);

    if (options.help) {
        std::printf("%s", ls::usageText());
        return 0;
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

    // --shot: drive the sim at a fixed dt (no real frame time, so the capture
    // is reproducible), render, screenshot, then exit.
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
                    session.toggleTurretAt(ls::Vec2{m.x, m.y}, 12.0f, 12.0f);
                }
                if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
                    session.startBattle();
                }
                break;
            case ls::Phase::Battle:
                break;
            case ls::Phase::Report:
                if (IsKeyPressed(KEY_R)) session.retry();
                if (IsKeyPressed(KEY_U)) session.openTree();
                if (IsKeyPressed(KEY_P)) session.backToPrepare();
                break;
            case ls::Phase::Tree:
                if (IsKeyPressed(KEY_R)) session.retry();
                if (IsKeyPressed(KEY_X)) session.respec();
                if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_ESCAPE)) {
                    session.backToReport();
                }
                for (int i = 0; i < static_cast<int>(ls::kNodeCount); ++i) {
                    if (IsKeyPressed(KEY_ONE + i)) {
                        session.buy(static_cast<ls::NodeId>(i));
                    }
                }
                break;
        }

        // --- simulation ----------------------------------------------------
        // Drop any stale accumulator when a battle begins, so a long pause or
        // hitch on the previous frame can't burst a dozen catch-up ticks and
        // make the opening of a battle lurch.
        if (session.phase() == ls::Phase::Battle) {
            if (prevPhase != ls::Phase::Battle) timestep.reset();

            const double frameSeconds =
                shotMode ? timestep.tickSeconds()
                         : static_cast<double>(GetFrameTime());
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
        char hint[128];
        std::snprintf(hint, sizeof(hint),
                      "phase %d   scrap %u   best %u",
                      static_cast<int>(session.phase()), session.scrap(),
                      session.bestKills());
        DrawText(hint, 12, 700, 16, Color{140, 150, 165, 255});

        if (!battlePhases) {
            const Vector2 s = {static_cast<float>(GetScreenWidth()),
                               static_cast<float>(GetScreenHeight())};
            DrawRectangleV(Vector2{0, 0}, s, Color{12, 10, 10, 230});
            if (session.phase() == ls::Phase::Report) drawReport(session);
            else if (session.phase() == ls::Phase::Tree) drawTree(session);
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
