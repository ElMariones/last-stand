#include <chrono>
#include <cstdio>
#include <string>
#include <raylib.h>

#include <algorithm>
#include <cstring>
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

// Drives the session to whatever screen a capture wants, so a screenshot of
// the upgrade tree does not depend on someone pressing keys fast enough.
void setUpShot(ls::Session& session, const char* screen, int level,
               bool keepTutorial) {
    const auto is = [&](const char* name) {
        return std::strcmp(screen, name) == 0;
    };
    // A capture session has no save, so the tutorial is always on step one.
    // Screenshots should show the game unless the coaching is the subject.
    if (!keepTutorial) session.skipTutorial();
    if (is("title")) return;

    session.goMenu();
    if (is("menu")) return;
    if (is("options")) { session.goOptions(); return; }
    if (is("stats")) { session.goStats(); return; }
    if (is("levels")) { session.goLevelSelect(); return; }

    // Unchecked because a capture session has no save, so every sector past
    // the first is locked and selectLevel would refuse. Reviewing the later
    // sectors' art is exactly what this mode is for.
    session.selectLevelUnchecked(level);
    if (is("prepare")) return;

    session.startBattle();
    if (is("battle")) {
        // Far enough in that there is a horde on screen and things are dying.
        for (int i = 0; i < 1400; ++i) {
            session.updateBattle(1.0f / 60.0f);
            session.updatePresentation(1.0f / 60.0f);
        }
        return;
    }

    for (int i = 0; i < 20000 && session.phase() == ls::Phase::Battle; ++i) {
        session.updateBattle(1.0f / 60.0f);
    }
    session.skipReveal();
    if (is("tree")) session.openTree();
}

// Recomputes everything that depends on the window's current size. Called
// every frame, so dragging the window edge reflows the game live rather than
// leaving the battlefield drawn at some size the window no longer is.
ls::Viewport syncToWindow(const ls::Settings& settings) {
    const float w = static_cast<float>(GetScreenWidth());
    const float h = static_cast<float>(GetScreenHeight());
    ls::ui::setScale(ls::uiScaleForWindow(w, h) * settings.ui());
    return ls::fitViewport(1280.0f, 720.0f, w, h);
}

// Pushes the display settings into raylib. Borderless windowed rather than
// exclusive fullscreen: ToggleFullscreen changes the monitor's video mode,
// which on macOS leaves the window at the old size for a frame or three and
// sometimes never comes back at all. Borderless is instant, reversible and
// what nearly every player means by "fullscreen".
void applyDisplaySettings(const ls::Settings& settings) {
    const bool borderless = IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
    if (settings.fullscreen != borderless) {
        ToggleBorderlessWindowed();
        return;   // the size below is meaningless while borderless
    }
    if (!settings.fullscreen && (GetScreenWidth() != settings.windowWidth ||
                                 GetScreenHeight() != settings.windowHeight)) {
        SetWindowSize(settings.windowWidth, settings.windowHeight);
        SetWindowPosition(
            (GetMonitorWidth(GetCurrentMonitor()) - settings.windowWidth) / 2,
            (GetMonitorHeight(GetCurrentMonitor()) - settings.windowHeight) / 2);
    }
}

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
    for (const ls::Vec2& p : ls::defaultDeployPositions(level.map, 4)) {
        world.placeTurret(p);
    }
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
        renderer.draw(world, ls::fitViewport(1280.0f, 720.0f,
                                             static_cast<float>(GetScreenWidth()),
                                             static_cast<float>(GetScreenHeight())),
                      0.0f, flags, settings);
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
    if (options.matrix) {
        // A spread wide enough to show both ends: a player who bought almost
        // nothing, and one who has been farming.
        const int budgets[7] = {0, 400, 1500, 5000, 15000, 40000, 80000};
        ls::printMatrix(ls::runMatrix(budgets, 7));
        return 0;
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
    // Below this the report panel cannot fit, so the window will not go there.
    SetWindowMinSize(1024, 600);

    ls::Session session(kSavePath);
    if (options.windowWidth > 0 && options.windowHeight > 0) {
        session.settings().windowWidth = options.windowWidth;
        session.settings().windowHeight = options.windowHeight;
        session.settings().fullscreen = false;
    }
    applyDisplaySettings(session.settings());
    ls::Viewport viewport = syncToWindow(session.settings());
    ls::AudioEngine audio;
    audio.init();
    audio.applySettings(session.settings());

    ls::FixedTimestep timestep{60.0, 0.25};
    ls::Renderer renderer;
    ls::DebugFlags flags;
    ls::ui::State ui;
    ui.windowSizeIndex = ls::nearestWindowSize(session.settings().windowWidth,
                                               session.settings().windowHeight);

    ls::Phase prevPhase = session.phase();
    double lastTickMs = 0.0;
    bool quitRequested = false;
    uint64_t shotFrames = 0u;

    const bool shotMode = options.shotTicks > 0u;
    if (shotMode) {
        setUpShot(session, options.shotScreen, options.shotLevel,
                  options.shotTutorial);
    }

    while (!WindowShouldClose() && !quitRequested) {
        const float frameSeconds =
            shotMode ? static_cast<float>(timestep.tickSeconds())
                     : GetFrameTime();
        const ls::Phase phase = session.phase();
        const bool inBattle = phase == ls::Phase::Battle;

        // The window can change size from the options screen, from the OS, or
        // from the player dragging a corner. All three land here.
        viewport = syncToWindow(session.settings());
        if (IsWindowResized() && !session.settings().fullscreen) {
            session.settings().windowWidth = GetScreenWidth();
            session.settings().windowHeight = GetScreenHeight();
            ui.windowSizeIndex = ls::nearestWindowSize(
                session.settings().windowWidth, session.settings().windowHeight);
        }

        // --- developer toggles ---------------------------------------------
        if (IsKeyPressed(KEY_F)) flags.showFlowField = !flags.showFlowField;
        if (IsKeyPressed(KEY_G)) flags.showGrid = !flags.showGrid;
        if (IsKeyPressed(KEY_T)) flags.showTurretRange = !flags.showTurretRange;

        // --- battle-only input ---------------------------------------------
        if (inBattle) {
            if (IsKeyPressed(KEY_S)) session.cycleTimeScale();
            // Direct speed selection, because four presses to reach 4x is a chore.
            if (IsKeyPressed(KEY_ONE)) session.setTimeScale(1);
            if (IsKeyPressed(KEY_TWO)) session.setTimeScale(2);
            if (IsKeyPressed(KEY_THREE)) session.setTimeScale(3);
            if (IsKeyPressed(KEY_FOUR)) session.setTimeScale(4);
            if (IsKeyPressed(KEY_A)) session.fireAirstrike();
            if (IsKeyPressed(KEY_O)) {
                const Vector2 m = GetMousePosition();
                session.overchargeAt(
                    viewport.screenToWorld(ls::Vec2{m.x, m.y}));
            }
        }
        if (phase == ls::Phase::Prepare) {
            const Vector2 screenMouse = GetMousePosition();
            const ls::Vec2 m =
                viewport.screenToWorld(ls::Vec2{screenMouse.x, screenMouse.y});
            const float hudTop =
                static_cast<float>(GetScreenHeight()) - ls::ui::px(104.0f);
            // Drag to reposition, click open ground to deploy, right-click a
            // turret to put it back in the crate. A generous grab radius:
            // hunting for a 14px hitbox with a mouse is not a game mechanic.
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                screenMouse.y < hudTop) {
                const int hit = session.turretIndexAt(m, 16.0f);
                if (hit >= 0) {
                    ui.dragIndex = hit;
                } else {
                    session.placeTurretAt(m);
                }
            }
            if (ui.dragIndex >= 0) {
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                    session.moveTurret(ui.dragIndex, m);
                } else {
                    ui.dragIndex = -1;
                }
            }
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) &&
                screenMouse.y < hudTop) {
                session.removeTurretAt(m, 16.0f);
            }
        } else {
            ui.dragIndex = -1;
        }

        // --- simulation -----------------------------------------------------
        // Hitstop withholds ticks rather than scaling dt, so the fixed
        // timestep — and therefore determinism — is untouched by juice.
        // The title screen runs its own backdrop off the same fixed
        // timestep, so the claim that the menu background is a live battle is
        // actually true.
        if (phase == ls::Phase::Title) {
            const double advance = shotMode ? timestep.tickSeconds()
                                            : static_cast<double>(frameSeconds);
            const int ticks = timestep.advance(advance);
            for (int i = 0; i < ticks; ++i) {
                session.tickBackdrop(static_cast<float>(timestep.tickSeconds()));
            }
        }

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
        // The Scrap counter lives in screen space; the arcs fly in world
        // space, so the anchor has to come back through the viewport.
        session.setScrapAnchor(viewport.screenToWorld(
            ls::Vec2{static_cast<float>(GetScreenWidth()) - ls::ui::px(40.0f),
                     ls::ui::px(28.0f)}));

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
            renderer.draw(*session.world(), viewport,
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
            case ls::Phase::Stats:
                action = ls::ui::drawStats(ui, session);
                break;
            case ls::Phase::Prepare:
                action = ls::ui::drawPrepareHud(ui, session, viewport);
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

        // Drawn last so it sits over whatever screen is up, and only where
        // it is worth coaching - the title and menus are self-explanatory.
        {
            const ls::Phase p = session.phase();
            const bool coachable =
                p == ls::Phase::Prepare || p == ls::Phase::Battle ||
                p == ls::Phase::Report || p == ls::Phase::Tree ||
                p == ls::Phase::LevelSelect;
            if (coachable) {
                const ls::ui::Result t = ls::ui::drawTutorial(session);
                if (t.action != ls::ui::Action::None) action = t;
            }
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
            case ls::ui::Action::Continue:
                if (session.phase() == ls::Phase::Title) session.goMenu();
                else session.goLevelSelect();
                break;
            case ls::ui::Action::NewGame:
                session.newGame();
                audio.applySettings(session.settings());
                break;
            case ls::ui::Action::Options: session.goOptions(); break;
            case ls::ui::Action::Stats: session.goStats(); break;
            case ls::ui::Action::Quit: quitRequested = true; break;
            case ls::ui::Action::ApplyDisplay:
                applyDisplaySettings(session.settings());
                audio.applySettings(session.settings());
                break;
            case ls::ui::Action::Back:
                audio.play(ls::SoundId::UiBack);
                if (session.phase() == ls::Phase::Options ||
                    session.phase() == ls::Phase::Stats) {
                    session.resume();
                }
                else if (session.phase() == ls::Phase::Tree) session.backToReport();
                else session.goMenu();
                break;
            case ls::ui::Action::SelectLevel:
                if (action.value < 0) {
                    session.goLevelSelect();
                    ui.mapCentred = false;
                } else {
                    session.selectLevel(action.value);
                }
                break;
            case ls::ui::Action::StartBattle: session.startBattle(); break;
            case ls::ui::Action::SelectKind:
                session.selectKind(static_cast<ls::TurretKind>(action.value));
                break;
            case ls::ui::Action::BuyTurret:
                session.buyTurret(static_cast<ls::TurretKind>(action.value));
                break;
            case ls::ui::Action::CycleTargeting: session.cycleTargeting(); break;
            case ls::ui::Action::CycleSpeed: session.cycleTimeScale(); break;
            case ls::ui::Action::FillHardpoints: session.autoDeploy(); break;
            case ls::ui::Action::ClearHardpoints:
                session.clearLoadout();
                ui.dragIndex = -1;
                break;
            case ls::ui::Action::Retry: session.retry(); break;
            case ls::ui::Action::ToMap:
                // Reachable from every screen, including mid-battle via the
                // pause menu. Re-centre the board each time so it opens on
                // where the player actually is.
                if (session.phase() == ls::Phase::Pause) session.abandonBattle();
                session.goLevelSelect();
                ui.mapCentred = false;
                break;
            case ls::ui::Action::Restart:
                session.resume();
                session.retry();
                break;
            case ls::ui::Action::OpenTree:
                session.openTree();
                // Land the cursor on the node the report just recommended,
                // so the advice survives the screen change.
                if (session.failure().suggestionCount > 0) {
                    ui.tree.index = static_cast<int>(
                        session.failure().suggestions[0]);
                    ui.treeScroll = 0;
                }
                break;
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
            case ls::ui::Action::SkipTutorial: session.skipTutorial(); break;
            case ls::ui::Action::RestartTutorial:
                session.restartTutorial();
                break;
            case ls::ui::Action::ToMenu: session.goMenu(); break;
        }

        if (shotMode) {
            ++shotFrames;
            if (shotFrames >= options.shotTicks) {
                TakeScreenshot(options.shotPath);
                break;
            }
        }
    }

    session.saveNow();
    audio.shutdown();
    CloseWindow();
    return 0;
}
