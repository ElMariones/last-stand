#pragma once
#include <array>
#include <cstdint>

#include "gameplay/Level.h"
#include "gameplay/UpgradeTree.h"
#include "sim/World.h"

namespace ls {

// Where a breach happened, in words the player can act on.
enum class Lane : uint8_t { North = 0, Centre, South };
const char* laneName(Lane lane);

// Kills credited to each source. Derived, not tracked: per-turret kills are
// already counted in the simulation, and burn kills are whatever is left over,
// because arrivals despawn without ever counting as a kill. Adding counters to
// World for a report panel would have meant re-blessing the golden hashes for
// a cosmetic feature.
struct Attribution {
    uint32_t machineGun   = 0u;
    uint32_t cannon       = 0u;
    uint32_t flamethrower = 0u;   // direct kills; the Flamethrower's are burn
    uint32_t burn         = 0u;
    uint32_t total        = 0u;

    float share(uint32_t part) const {
        return (total == 0u) ? 0.0f
                             : static_cast<float>(part) /
                                   static_cast<float>(total);
    }
};

// Samples a battle as it runs. Owned by the Session, fed from the World, and
// entirely outside sim/ — the simulation neither knows nor cares.
class BattleTelemetry {
public:
    static constexpr int   kMaxRows = 64;
    static constexpr float kSampleInterval = 0.1f;   // 10 Hz is plenty

    void begin(const Level& level);
    void sample(const World& world, float dt);
    void finish(const World& world, bool victory);

    float    elapsedSeconds() const { return elapsed_; }
    float    peakKillsPerSecond() const { return peakKps_; }
    bool     breached() const { return breached_; }
    float    breachTime() const { return breachTime_; }
    Lane     breachLane() const { return breachLane_; }
    uint32_t peakDensityAtBreach() const { return breachDensity_; }
    uint32_t peakDensity() const { return peakDensity_; }

    // Both are estimates and are labelled as such in the UI. Damage is not
    // tracked per shot, so DPS is reconstructed from kills and the invasion's
    // average health — good enough to size a gap, and honest about it.
    float estimatedDps() const;
    float requiredDps() const;

    const Attribution& attribution() const { return attribution_; }
    bool  victory() const { return victory_; }
    // True when the invasion fielded anything with armour - Tanks, Brutes or
    // Behemoths. Armour is subtracted per hit, so it is the one condition
    // where the answer is specifically Armor Piercing rather than more of
    // whatever the player already has.
    bool  hasArmored() const { return hasArmored_; }
    // True when something in the invasion repairs itself, which turns "enough
    // damage eventually" into "enough damage per second".
    bool  hasRegen() const { return hasRegen_; }

private:
    Lane laneForRow(int row, int rows) const;

    float    elapsed_       = 0.0f;
    float    sampleClock_   = 0.0f;
    float    breachTime_    = 0.0f;
    float    peakKps_       = 0.0f;
    float    kpsWindow_     = 0.0f;   // seconds accumulated in the window
    uint32_t kpsWindowKills_ = 0u;
    float    levelSeconds_  = 1.0f;
    float    totalHealth_   = 0.0f;
    uint32_t totalEnemies_  = 0u;
    uint32_t lastKills_     = 0u;
    uint32_t peakDensity_   = 0u;
    uint32_t breachDensity_ = 0u;
    float    lastBaseHealth_ = -1.0f;
    Lane     breachLane_    = Lane::Centre;
    bool     breached_      = false;
    bool     victory_       = false;
    bool     hasArmored_    = false;
    bool     hasRegen_      = false;
    Attribution attribution_;
};

// The Battle Report's standout panel (GDD 13.2): where, when, and by how much
// you missed — then the nodes that close it.
struct FailureAnalysis {
    bool        victory      = false;
    const char* lane         = "CENTRE LANE";
    float       breachTime   = 0.0f;
    uint32_t    peakDensity  = 0u;
    uint32_t    yourDps      = 0u;
    uint32_t    requiredDps  = 0u;
    std::array<NodeId, 2> suggestions{};
    int         suggestionCount = 0;
};

// Picks up to two nodes the player does not own that address the diagnosis:
// a density problem wants area damage, a raw shortfall wants damage or fire
// rate, an early breach wants reach, and Tanks want armour piercing.
FailureAnalysis analyse(const BattleTelemetry& telemetry,
                        const UpgradeTree& tree);

}  // namespace ls
