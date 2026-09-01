#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "app/Tutorial.h"
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
    Stats,
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
    const Stats& stats() const { return stats_; }

    const Settings& settings() const { return settings_; }
    Settings&       settings() { return settings_; }
    // Re-reads the settings into everything that derives from them and saves.
    void applySettings();

    // --- screen navigation -------------------------------------------------
    void goMenu();
    void goOptions();
    void goLevelSelect();
    void goStats();
    void pause();
    void resume();
    void abandonBattle();          // Pause -> Menu, no payout
    void selectLevel(int idx);
    // The campaign is a graph (see gameplay/Level.h): a sector opens as soon
    // as ANY of its parent sectors has been held at least once, so tiers fan
    // out into alternatives rather than a single queue. Tier 0 is always open.
    bool isLevelUnlocked(int idx) const;
    int  furthestUnlockedLevel() const;
    // Where the campaign wants to send the player next: a sector this victory
    // just opened if there is one, otherwise the shallowest one still
    // standing. Returns the current level when there is nowhere new to go.
    int  suggestedNextLevel() const;
    // True when the battle just won opened something new. The report turns
    // that into its primary action — which is a trip to the sector map, not a
    // jump to one particular sector: the whole point of a branching campaign
    // is that the player picks, and a "NEXT" button quietly picks for them.
    bool canAdvance() const;
    // How many sectors this victory opened, so the report can say what was
    // actually won rather than naming the next index up.
    int  sectorsOpenedHere() const;

    // --- harness hooks -----------------------------------------------------
    // Used only by the offline tools - the balance harness, which drops a
    // hypothetical player onto an arbitrary sector with an arbitrary budget,
    // and the screenshot mode, which has no save and so would find every
    // sector but the first locked. Deliberately named so that a call site in
    // the game itself reads as obviously wrong.
    void grantScrap(uint32_t amount);
    void selectLevelUnchecked(int idx);
    // The upgrade screen normally opens off the back of a battle report. The
    // matrix has to outfit a hypothetical player who has not fought one, so
    // it needs a way in that does not fake a result.
    void openTreeDirect();

    // --- Prepare: turret placement -----------------------------------------
    TurretKind selectedKind() const { return selectedKind_; }
    void cycleKind();
    // Direct selection for the 1/2/3 hotkeys. Silently ignores a locked kind.
    void selectKind(TurretKind kind);
    bool isKindUnlocked(TurretKind kind) const { return kindUnlocked(kind); }
    void cycleTargeting();
    TargetingMode targetingMode() const;

    // --- the arsenal -------------------------------------------------------
    // Turrets are things you own, not slots you fill. Buy them, then put them
    // wherever you like: positioning IS the tactical decision, and a fixed
    // grid of emplacements was making that decision for the player.
    uint32_t owned(TurretKind kind) const;
    uint32_t placed(TurretKind kind) const;
    uint32_t available(TurretKind kind) const;
    uint32_t turretPrice(TurretKind kind) const;
    bool     canAffordTurret(TurretKind kind) const;
    bool     buyTurret(TurretKind kind);

    int  turretCount() const { return static_cast<int>(loadout_.size()); }
    const std::vector<Turret>& loadout() const { return loadout_; }

    // Anywhere walkable, clear of the base and of other turrets. Returns why
    // not, so the cursor can say so rather than just refusing.
    enum class Placement { Ok, OffMap, OnWall, TooCloseToBase, TooCloseToTurret };
    Placement placementAt(Vec2 worldPos, int ignoreIndex = -1) const;
    bool canPlaceAt(Vec2 worldPos, int ignoreIndex = -1) const {
        return placementAt(worldPos, ignoreIndex) == Placement::Ok;
    }

    int  turretIndexAt(Vec2 worldPos, float radius) const;
    bool placeTurretAt(Vec2 worldPos);          // spends one from the arsenal
    bool moveTurret(int index, Vec2 worldPos);  // drag; refuses invalid drops
    void recallTurret(int index);               // back into the arsenal
    void toggleTurretAt(Vec2 worldPos, float radius);
    void removeTurretAt(Vec2 worldPos, float radius);
    // Deploys everything still in reserve: the suggested emplacements first,
    // then outward from them. One keypress for players who would rather not
    // arrange anything, and the harness's placement policy.
    void autoDeploy();
    void clearLoadout();

    // Time controls (1x/2x/4x).
    int  timeScale() const { return timeScale_; }
    void cycleTimeScale();
    void setTimeScale(int scale);

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
    uint32_t bestKillsFor(int level) const;
    uint32_t clearCountFor(int level) const;

    // True when this save holds anything worth continuing. Drives whether the
    // menu offers CONTINUE or only NEW GAME.
    bool hasProgress() const;
    // Wipes Scrap, the tree and every record, keeping the player's options —
    // nobody wants their volume reset because they restarted the campaign.
    void newGame();

    // How many turrets the player has deliberately put down or dragged this
    // session. The tutorial watches it, because "you decide where these go"
    // is the one thing a new player has to be shown rather than told - and
    // the alternative, watching the turret count, is already non-zero the
    // moment a sector loads.
    uint32_t placementsMade() const { return placementsMade_; }

    // --- the first run -----------------------------------------------------
    const Tutorial& tutorial() const { return tutorial_; }
    // Dismissed by the player, or finished on its own. Either way it is
    // recorded in the save so it never appears again unaided.
    void skipTutorial();
    // Available from Options, because a player who skipped it on a whim
    // should not have to erase their campaign to get it back.
    void restartTutorial();

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
    Stats          stats_;
    uint32_t       scrap_ = 0u;
    std::array<uint32_t, kSaveLevels> bestKills_{};
    std::array<uint32_t, kSaveLevels> clearCounts_{};

    Effects effects_;
    int     levelIndex_ = 0;
    Level   level_ = makeLevel1();

    std::vector<Turret>    loadout_;   // the build, persistent across retries
    // How many of each kind the player owns, placed or not.
    std::array<uint32_t, 3> arsenal_{};
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

    uint32_t      placementsMade_ = 0u;
    Tutorial      tutorial_;
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
