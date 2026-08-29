#include <chrono>
#include <cstdio>
#include <memory>
#include <raylib.h>

#include "app/Bench.h"
#include "app/Cli.h"
#include "core/FixedTimestep.h"
#include "render/Renderer.h"
#include "sim/World.h"

namespace {

std::unique_ptr<ls::World> makeWorld(uint64_t seed, uint32_t spawn) {
    auto w = std::make_unique<ls::World>(ls::makeM1Map(), seed);
    for (const ls::Vec2& hp : w->map().hardpoints) {
        w->placeTurret(hp);
    }
    w->spawnWave(spawn);
    return w;
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
    SetTargetFPS(0);   // vsync governs pacing; never cap with a sleep

    auto world = makeWorld(options.seed, options.spawn);
    ls::FixedTimestep timestep{60.0, 0.25};
    ls::Renderer renderer;
    ls::DebugFlags flags;

    double lastTickMs = 0.0;

    // --shot mode: drive the sim at a fixed dt (ignoring real frame time so
    // the capture is reproducible), render, then screenshot and exit.
    const bool shotMode = options.shotTicks > 0u;
    if (shotMode) {
        flags.showFlowField = true;
    }

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_F)) flags.showFlowField = !flags.showFlowField;
        if (IsKeyPressed(KEY_G)) flags.showGrid = !flags.showGrid;
        if (IsKeyPressed(KEY_T)) flags.showTurretRange = !flags.showTurretRange;
        if (IsKeyPressed(KEY_SPACE)) world->spawnWave(100u);
        if (IsKeyPressed(KEY_R)) world = makeWorld(options.seed, options.spawn);

        const double frameSeconds =
            shotMode ? (1.0 / 60.0) : static_cast<double>(GetFrameTime());
        const int ticks = timestep.advance(frameSeconds);

        if (ticks > 0) {
            const auto t0 = std::chrono::steady_clock::now();
            for (int i = 0; i < ticks; ++i) {
                world->tick(static_cast<float>(timestep.tickSeconds()));
            }
            const auto t1 = std::chrono::steady_clock::now();
            const double totalMs =
                std::chrono::duration<double, std::milli>(t1 - t0).count();
            lastTickMs = totalMs / static_cast<double>(ticks);
        }

        renderer.draw(*world,
                      static_cast<float>(timestep.alpha()),
                      flags,
                      frameSeconds * 1000.0,
                      lastTickMs);

        if (shotMode && timestep.totalTicks() >= options.shotTicks) {
            TakeScreenshot("shot.png");
            break;
        }
    }

    CloseWindow();
    return 0;
}
