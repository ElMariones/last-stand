#include <chrono>
#include <cstdio>
#include <string>
#include <raylib.h>

#include <algorithm>
#include <vector>

#include "app/Balance.h"
#include "app/Bench.h"
#include "app/Cli.h"
#include "app/Session.h"
#include "audio/AudioEngine.h"
#include "core/FixedTimestep.h"
#include "gameplay/Level.h"
#include "gameplay/SpawnDirector.h"
#include "render/Renderer.h"
#include "render/Theme.h"
#include "ui/Screens.h"

namespace {

const char* kSavePath = "laststand.save";

// Times the render path alone: the simulation runs so the horde is real and
// moving, but only renderer.draw() is on the clock. Opens a window because
// there is no way to price draw submission without one; vsync is off so the
// number is the work, not the display's refresh.
int runRenderBench(const ls::Options& options) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "LAST STAND - render bench");
    if (!IsWindowReady()) {
        std::printf(
            "no window: --render-bench needs a desktop session.\n"
            "The headless LOD census in --bench reports submission cost "
            "without one.\n");
        return 1;
    }
    SetTargetFPS(0);

    ls::Level level;
    level.name = "render-bench";
    level.map = ls::makeM1Map();
    level.schedule = {ls::SpawnEvent{0.0f, options.renderBench}};
    level.totalEnemies = options.renderBench;

    ls::World world{level.map, options.seed};
    for (const ls::Vec2& hp : level.map.hardpoints) world.placeTurret(hp);
    world.setLevelTotal(level.totalEnemies);
    world.base().maxHealth = 1.0e9f;
    world.base().health = world.base().maxHealth;

    ls::SpawnDirector director;
    ls::Renderer renderer;
    ls::DebugFlags flags;
    ls::RenderSettings renderSettings;
    renderSettings.lod = !options.noLod;
    renderSettings.batched = !options.noBatch;
    ls::RenderSettings settings;
    settings.lod = !options.noLod;
    settings.batched = !options.noBatch;

    std::vector<double> samples;
    samples.reserve(static_cast<size_t>(options.renderFrames));
    ls::RenderStats last;

    const float dt = 1.0f / 60.0f;
    for (uint64_t frame = 0; frame < options.renderFrames &&
                             !WindowShouldClose(); ++frame) {
        director.update(world, level, dt);
        world.tick(dt);

        BeginDrawing();
        ClearBackground(Color{12, 10, 10, 255});
        const auto t0 = std::chrono::steady_clock::now();
        renderer.draw(world, 0.0f, flags, settings);
        const auto t1 = std::chrono::steady_clock::now();
        EndDrawing();

        // The first few frames pay for shader/pipeline warm-up.
        if (frame >= 30u) {
            samples.push_back(
                std::chrono::duration<double, std::milli>(t1 - t0).count());
        }
        last = renderer.stats();
    }
    CloseWindow();

    if (samples.empty()) {
        std::printf("no frames sampled\n");
        return 1;
    }
    std::sort(samples.begin(), samples.end());
    double sum = 0.0;
    for (const double v : samples) sum += v;

    std::printf("mode           %s%s\n", options.noLod ? "no-lod " : "lod ",
                options.noBatch ? "no-batch" : "batched");
    std::printf("frames         %zu\n", samples.size());
    std::printf("entities       %u\n", last.enemies);
    std::printf("drawn          %u\n", last.drawn);
    std::printf("triangles      %u\n", last.triangles);
    std::printf("batches        %u\n", last.batches);
    std::printf("tier_full      %u\n", last.tierCount[0]);
    std::printf("tier_silhouette %u\n", last.tierCount[1]);
    std::printf("tier_shape     %u\n", last.tierCount[2]);
    std::printf("draw_mean_ms   %.4f\n",
                sum / static_cast<double>(samples.size()));
    std::printf("draw_p99_ms    %.4f\n",
                samples[static_cast<size_t>(
                    static_cast<double>(samples.size() - 1) * 0.99)]);
    return 0;
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
    if (options.balanceRuns > 0) {
        ls::printBalance(ls::runBalance(options.balanceRuns, options.balanceLevel));
        return 0;
    }
    if (options.bench || options.noRender) {
        const ls::BenchResult r = ls::runBench(options);
        ls::printBench(r);
        return 0;
    }
    if (options.renderBench > 0u) return runRenderBench(options);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "LAST STAND");
    if (!IsWindowReady()) {
        std::printf("no window: this build needs a desktop session to run "
                    "the game. Try --bench for the headless simulation.\n");
        return 1;
    }
    SetTargetFPS(0);
    SetExitKey(KEY_NULL);   // ESC is the pause/back key, not the quit key

    ls::Session session(kSavePath);
    ls::AudioEngine audio;
    audio.init();
    audio.applySettings(session.settings());

    ls::FixedTimestep timestep{60.0, 0.25};
    ls::Renderer renderer;
    ls::DebugFlags flags;
    ls::ui::State ui;

    ls::Phase prevPhase = session.phase();
    double lastTickMs = 0.0;
    bool quitRequested = false;

    const bool shotMode = options.shotTicks > 0u;
    if (shotMode) {
        session.goMenu();
        session.selectLevel(0);
        session.startBattle();
    }

    while (!WindowShouldClose() && !quitRequested) {
        const float frameSeconds =
            shotMode ? static_cast<float>(timestep.tickSeconds())
                     : GetFrameTime();
        const ls::Phase phase = session.phase();
        const bool inBattle = phase == ls::Phase::Battle;

        // --- developer toggles ---------------------------------------------
        if (IsKeyPressed(KEY_F)) flags.showFlowField = !flags.showFlowField;
        if (IsKeyPressed(KEY_G)) flags.showGrid = !flags.showGrid;
        if (IsKeyPressed(KEY_T)) flags.showTurretRange = !flags.showTurretRange;

        // --- battle-only input ---------------------------------------------
        if (inBattle) {
            if (IsKeyPressed(KEY_S)) session.cycleTimeScale();
            if (IsKeyPressed(KEY_A)) session.fireAirstrike();
            if (IsKeyPressed(KEY_O)) {
                const Vector2 m = GetMousePosition();
                session.overchargeAt(ls::Vec2{m.x, m.y});
            }
        }
        if (phase == ls::Phase::Prepare &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            GetMousePosition().y < static_cast<float>(GetScreenHeight()) - 84.0f) {
            const Vector2 m = GetMousePosition();
            session.setTurretAt(ls::Vec2{m.x, m.y}, 14.0f, 14.0f);
        }

        // --- simulation -----------------------------------------------------
        // Hitstop withholds ticks rather than scaling dt, so the fixed
        // timestep — and therefore determinism — is untouched by juice.
        if (inBattle && !session.frozen()) {
            if (prevPhase != ls::Phase::Battle) timestep.reset();
            const double scale = static_cast<double>(session.timeScale());
            const double advance =
                shotMode ? timestep.tickSeconds()
                         : static_cast<double>(frameSeconds) * scale;
            const int ticks = timestep.advance(advance);
            if (ticks > 0) {
                const auto t0 = std::chrono::steady_clock::now();
                for (int i = 0; i < ticks; ++i) {
                    session.updateBattle(static_cast<float>(timestep.tickSeconds()));
                }
                const auto t1 = std::chrono::steady_clock::now();
                lastTickMs = std::chrono::duration<double, std::milli>(t1 - t0)
                                 .count() / static_cast<double>(ticks);
            }
        }
        prevPhase = session.phase();

        session.updatePresentation(frameSeconds);
        renderer.tickAnimationClock(frameSeconds);
        session.setScrapAnchor(
            ls::Vec2{static_cast<float>(GetScreenWidth()) - 40.0f, 28.0f});

        // --- audio ----------------------------------------------------------
        const ls::FrameEvents ev = session.takeEvents();
        audio.update(frameSeconds, ev.kills,
                     ev.gunShots + ev.cannonShots + ev.flameShots, inBattle);
        if (ev.arrivals > 0u) audio.play(ls::SoundId::BaseHit);
        if (ev.airstrike) audio.play(ls::SoundId::Airstrike);
        if (ev.overcharge) audio.play(ls::SoundId::Overcharge);
        if (ev.cannonShots > 0u) audio.play(ls::SoundId::Cannon);
        if (ev.flameShots > 0u) audio.play(ls::SoundId::Flame);
        if (ev.gunShots > 0u) audio.play(ls::SoundId::Gunshot);
        if (ev.kills > 0u) audio.play(ls::SoundId::Death);
        if (ev.battleEnded) {
            audio.stopBeds();
            audio.play(ev.victory ? ls::SoundId::Victory : ls::SoundId::Defeat);
        }

        // --- render ---------------------------------------------------------
        BeginDrawing();
        ClearBackground(ls::theme::kVoid);

        ls::RenderSettings renderSettings;
        renderSettings.lod = session.settings().levelOfDetail && !options.noLod;
        renderSettings.batched = !options.noBatch;

        ls::FxScene fx;
        fx.particles = &session.particles();
        fx.corpses = &session.corpses();
        fx.numbers = &session.damageNumbers();
        fx.showNumbers = session.settings().damageNumbers;
        fx.shake = session.juice().offset(timestep.totalTicks() +
                                          static_cast<uint64_t>(GetFPS()));

        if (session.world() != nullptr) {
            renderer.draw(*session.world(),
                          static_cast<float>(timestep.alpha()), flags,
                          renderSettings, fx);
        }

        // --- screens ---------------------------------------------------------
        ls::ui::beginFrame(ui.feedback);
        ls::ui::Result action;
        switch (session.phase()) {
            case ls::Phase::Title:
                action = ls::ui::drawTitle(ui, session, frameSeconds);
                break;
            case ls::Phase::Menu:
                action = ls::ui::drawMenu(ui, session);
                break;
            case ls::Phase::Options:
                action = ls::ui::drawOptions(ui, session);
                break;
            case ls::Phase::LevelSelect:
                action = ls::ui::drawLevelSelect(ui, session);
                break;
            case ls::Phase::Prepare:
                action = ls::ui::drawPrepareHud(ui, session);
                break;
            case ls::Phase::Battle:
                action = ls::ui::drawBattleHud(ui, session);
                break;
            case ls::Phase::Pause:
                action = ls::ui::drawPause(ui, session);
                break;
            case ls::Phase::Report:
                action = ls::ui::drawReport(ui, session);
                break;
            case ls::Phase::Tree:
                action = ls::ui::drawTree(ui, session);
                break;
        }

        if (session.settings().debugOverlay && session.world() != nullptr) {
            renderer.drawDebugOverlay(*session.world(),
                                      static_cast<double>(frameSeconds) * 1000.0,
                                      lastTickMs);
        }
        EndDrawing();

        // --- act on the screen's intent --------------------------------------
        if (ui.feedback.moved) audio.play(ls::SoundId::UiMove);
        if (ui.feedback.accepted) audio.play(ls::SoundId::UiSelect);

        // Any key completes the report's count-up instantly (GDD 13.1).
        if (session.phase() == ls::Phase::Report && GetKeyPressed() != 0) {
            session.skipReveal();
        }

        switch (action.action) {
            case ls::ui::Action::None: break;
            case ls::ui::Action::Play:
                if (session.phase() == ls::Phase::Title) session.goMenu();
                else session.goLevelSelect();
                break;
            case ls::ui::Action::Options: session.goOptions(); break;
            case ls::ui::Action::Quit: quitRequested = true; break;
            case ls::ui::Action::Back:
                audio.play(ls::SoundId::UiBack);
                if (session.phase() == ls::Phase::Options) session.resume();
                else if (session.phase() == ls::Phase::Tree) session.backToReport();
                else session.goMenu();
                break;
            case ls::ui::Action::SelectLevel:
                if (action.value < 0) session.goLevelSelect();
                else session.selectLevel(action.value);
                break;
            case ls::ui::Action::StartBattle: session.startBattle(); break;
            case ls::ui::Action::CycleKind: session.cycleKind(); break;
            case ls::ui::Action::CycleTargeting: session.cycleTargeting(); break;
            case ls::ui::Action::Retry: session.retry(); break;
            case ls::ui::Action::OpenTree: session.openTree(); break;
            case ls::ui::Action::BackToPrepare: session.backToPrepare(); break;
            case ls::ui::Action::Buy:
                session.buy(static_cast<ls::NodeId>(action.value));
                break;
            case ls::ui::Action::Respec: session.respec(); break;
            case ls::ui::Action::Resume:
                if (session.phase() == ls::Phase::Battle) session.pause();
                else session.resume();
                break;
            case ls::ui::Action::Abandon: session.abandonBattle(); break;
            case ls::ui::Action::ToMenu: session.goMenu(); break;
        }

        if (shotMode && timestep.totalTicks() >= options.shotTicks) {
            TakeScreenshot("shot.png");
            break;
        }
    }

    session.saveNow();
    audio.shutdown();
    CloseWindow();
    return 0;
}
