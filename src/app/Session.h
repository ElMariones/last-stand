#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "fx/Corpses.h"
#include "fx/DamageNumbers.h"
#include "fx/Juice.h"
#include "fx/Particles.h"
#include "gameplay/Level.h"
#include "gameplay/Progression.h"
#include "gameplay/Settings.h"
#include "gameplay/SpawnDirector.h"
#include "gameplay/Telemetry.h"
#include "gameplay/UpgradeTree.h"
#include "math/Vec2.h"
#include "persist/SaveGame.h"
#include "sim/World.h"

namespace ls {

// GDD 13.3's screen graph. Every arrow is reversible except Battle -> Report.
enum class Phase {
    Title,
    Menu,
    Options,
    LevelSelect,
    Prepare,
    Battle,
    Pause,
    Report,
    Tree,
};

// What happened since the caller last looked. Drained once per frame by the
// app, which turns it into sound. Session stays free of raylib this way, and
// the audio engine never has to reach into the simulation.
struct FrameEvents {
    uint32_t kills      = 0u;
    uint32_t arrivals   = 0u;
    uint32_t gunShots   = 0u;
    uint32_t cannonShots = 0u;
    uint32_t flameShots = 0u;
    bool     airstrike  = false;
    bool     overcharge = false;
    bool     battleEnded = false;
    bool     victory    = false;
};

// The app-level orchestrator: owns the save, the settings, the upgrade tree,
// the current level, the battle, its telemetry and all the presentation
// state that is not rendering. raylib-free — main.cpp converts input, the
// renderer draws a snapshot, the audio engine listens to FrameEvents.
class Session {
public:
    explicit Session(const char* savePath);

    Phase    phase() const { return phase_; }
    uint32_t scrap() const { return scrap_; }
    const UpgradeTree& tree() const { return tree_; }
    const Level& level() const { return level_; }
    int   levelIndex() const { return levelIndex_; }
    const World* world() const { return world_.get(); }

    bool hasResult() const { return hasResult_; }
    const BattleResult& result() const { return result_; }
    const Payout&       payout() const { return payout_; }
    const FailureAnalysis& failure() const { return failure_; }
    const BattleTelemetry& telemetry() const { return telemetry_; }

    // --- settings ----------------------------------------------------------
    const Settings& settings() const { return settings_; }
    Settings&       settings() { return settings_; }
    // Re-reads the settings into everything that derives from them and saves.
    void applySettings();

    // --- screen navigation -------------------------------------------------
    void goMenu();
    void goOptions();
    void goLevelSelect();
    void pause();
    void resume();
    void abandonBattle();          // Pause -> Menu, no payout
    void selectLevel(int idx);

    // Prepare: turret placement and targeting.
    TurretKind selectedKind() const { return selectedKind_; }
    void cycleKind();
    void cycleTargeting();
    void setTurretAt(Vec2 worldPos, float halfW, float halfH);

    // Time controls (1x/2x/4x).
    int  timeScale() const { return timeScale_; }
    void cycleTimeScale();

    // Abilities.
    bool airstrikeReady() const { return effects_.airstrike && airstrikeCd_ <= 0.0f; }
    bool overchargeReady() const { return effects_.overcharge && overchargeCd_ <= 0.0f; }
    float airstrikeCooldown() const { return airstrikeCd_; }
    float overchargeCooldown() const { return overchargeCd_; }
    void fireAirstrike();
    void overchargeAt(Vec2 worldPos);

    void startBattle();            // Prepare -> Battle
    void updateBattle(float dt);   // ticks battle; transitions to Report when over
    void retry();                  // Report/Tree -> fresh battle, immediately running
    void openTree();               // Report -> Tree
    void backToReport();           // Tree -> Report
    void backToPrepare();          // Report/Tree -> Prepare (re-tune loadout)
    void buy(NodeId node);         // Tree: spend scrap
    void respec();                 // Tree: refund everything

    void saveNow() const;

    uint32_t bestKills() const { return bestKills_[static_cast<size_t>(levelIndex_)]; }

    // --- presentation ------------------------------------------------------
    // Advances everything that runs at frame rate rather than tick rate.
    void updatePresentation(float frameSeconds);

    const ParticlePool&  particles() const { return particles_; }
    const CorpseRing&    corpses() const { return corpses_; }
    const DamageNumbers& damageNumbers() const { return numbers_; }
    const Juice&         juice() const { return juice_; }
    Juice&               juice() { return juice_; }

    // True while hitstop is holding the frame. The caller withholds ticks.
    bool frozen() const { return juice_.frozen(); }

    // The Scrap counter's screen position, so kill arcs know where to fly.
    void setScrapAnchor(Vec2 p) { scrapAnchor_ = p; }

    // 0..1 reveal for the Battle Report's count-up. Any key completes it
    // instantly (GDD 13.1: everything is skippable).
    float reportReveal() const { return reportReveal_; }
    void  skipReveal() { reportReveal_ = 1.0f; }

    FrameEvents takeEvents();

private:
    void resetWorld();
    void syncWorldTurrets();
    void rebuildEffects();
    void rebuildLoadout();
    void defaultLoadout();
    void finishBattle();
    bool kindUnlocked(TurretKind kind) const;
    void emitBattleFx(uint32_t killsBefore, uint32_t arrivalsBefore);
    void resetPresentation();

    std::string    savePath_;
    SaveData       saveData_;
    UpgradeTree    tree_;
    Settings       settings_;
    uint32_t       scrap_ = 0u;
    std::array<uint32_t, kSaveLevels> bestKills_{};
    std::array<uint32_t, kSaveLevels> clearCounts_{};

    Effects effects_;
    int     levelIndex_ = 0;
    Level   level_ = makeLevel1();

    std::vector<Turret>    loadout_;   // the build, persistent across retries
    std::unique_ptr<World> world_;
    SpawnDirector          director_;
    Phase                  phase_ = Phase::Title;
    Phase                  returnPhase_ = Phase::Menu;   // where Options goes back to

    TurretKind selectedKind_ = TurretKind::MachineGun;
    int        timeScale_    = 1;
    float      airstrikeCd_  = 0.0f;
    float      overchargeCd_ = 0.0f;

    bool         hasResult_ = false;
    BattleResult result_;
    Payout       payout_;
    BattleTelemetry telemetry_;
    FailureAnalysis failure_;

    ParticlePool  particles_;
    CorpseRing    corpses_;
    DamageNumbers numbers_;
    Juice         juice_;
    Pcg32         fxRng_{0xF00Du};      // presentation only; never the sim's
    Vec2          scrapAnchor_{1180.0f, 24.0f};
    float         reportReveal_ = 0.0f;
    FrameEvents   events_;
    std::array<uint64_t, 3> lastShots_{};   // shots per turret kind, last tick
};

}  // namespace ls
