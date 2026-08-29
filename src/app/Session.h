#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <string>

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
    const Level&     level() const { return level_; }
    const World*     world() const { return world_.get(); }

    bool     hasResult() const { return hasResult_; }
    const BattleResult& result() const { return result_; }
    const Payout&       payout() const { return payout_; }

    // Early-game turret placement (Prepare): toggle a turret at the hardpoint
    // nearest `worldPos` within `halfW`×`halfH`, if any. Returns true on a hit.
    bool toggleTurretAt(Vec2 worldPos, float halfW, float halfH);

    void startBattle();            // Prepare -> Battle
    void updateBattle(float dt);   // ticks battle; transitions to Report when over
    void retry();                  // Report/Tree -> fresh battle, immediately running
    void openTree();               // Report -> Tree
    void backToReport();           // Tree -> Report
    void backToPrepare();          // Report/Tree -> Prepare (re-tune loadout)
    void buy(NodeId node);         // Tree: spend scrap
    void respec();                 // Tree: refund everything

    void saveNow() const;

    // Whether the level has been won at least once (drives the replay table).
    uint32_t bestKills() const { return bestKills_[0]; }

private:
    void resetWorld();             // fresh world + default turrets + bonuses
    void applyBonuses();
    void finishBattle();

    std::string    savePath_;
    SaveData       saveData_;
    UpgradeTree    tree_;
    uint32_t       scrap_ = 0u;
    std::array<uint32_t, kSaveLevels> bestKills_{};
    std::array<uint32_t, kSaveLevels> clearCounts_{};

    Level                 level_ = makeLevel1();
    std::unique_ptr<World> world_;
    SpawnDirector         director_;
    Phase                 phase_ = Phase::Prepare;

    bool         hasResult_ = false;
    BattleResult result_;
    Payout       payout_;
};

}  // namespace ls
