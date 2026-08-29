#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "gameplay/Level.h"
#include "gameplay/Progression.h"
#include "gameplay/SpawnDirector.h"
#include "gameplay/UpgradeTree.h"
#include "math/Vec2.h"
#include "persist/SaveGame.h"
#include "sim/World.h"

namespace ls {

enum class Phase { Prepare, Battle, Report, Tree };

// The app-level orchestrator: owns the save, the upgrade tree, the current
// level, and the battle, and drives the run -> report -> upgrade -> retry
// cycle (GDD 3). raylib-free — main.cpp converts input and the renderer draws
// a snapshot of this state.
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

    // Level select (0..2).
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

private:
    void resetWorld();
    void syncWorldTurrets();
    void rebuildEffects();
    void defaultLoadout();
    void finishBattle();

    std::string    savePath_;
    SaveData       saveData_;
    UpgradeTree    tree_;
    uint32_t       scrap_ = 0u;
    std::array<uint32_t, kSaveLevels> bestKills_{};
    std::array<uint32_t, kSaveLevels> clearCounts_{};

    Effects effects_;
    int     levelIndex_ = 0;
    Level   level_ = makeLevel1();

    std::vector<Turret>    loadout_;   // the build, persistent across retries
    std::unique_ptr<World> world_;
    SpawnDirector          director_;
    Phase                  phase_ = Phase::Prepare;

    TurretKind selectedKind_ = TurretKind::MachineGun;
    int        timeScale_    = 1;
    float      airstrikeCd_  = 0.0f;
    float      overchargeCd_ = 0.0f;

    bool         hasResult_ = false;
    BattleResult result_;
    Payout       payout_;
};

}  // namespace ls
