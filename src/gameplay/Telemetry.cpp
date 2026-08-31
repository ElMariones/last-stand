#include "gameplay/Telemetry.h"

#include <algorithm>
#include <cmath>

namespace ls {

namespace {

// Time an enemy needs to walk the map once the last one has spawned. Used to
// turn "kill this much health" into "kill it this fast".
constexpr float kTransitSeconds = 30.0f;

}  // namespace

const char* laneName(Lane lane) {
    switch (lane) {
        case Lane::North:  return "NORTH LANE";
        case Lane::Centre: return "CENTRE LANE";
        case Lane::South:  return "SOUTH LANE";
    }
    return "CENTRE LANE";
}

void BattleTelemetry::begin(const Level& level) {
    *this = BattleTelemetry{};

    float lastSpawn = 0.0f;
    for (const SpawnEvent& e : level.schedule) {
        lastSpawn = std::max(lastSpawn, e.timeSeconds);
        const EnemyStats& st = statsFor(e.type);
        // Sector toughness is part of what the player has to kill. Leaving it
        // out made "required DPS" read four times too low on the last tier,
        // which is exactly where the advice matters most.
        totalHealth_ +=
            st.hp * level.enemyHealthMult * static_cast<float>(e.count);
        totalEnemies_ += e.count;
        if (st.armor > 0.0f) hasArmored_ = true;
        if (st.regen > 0.0f) hasRegen_ = true;
    }
    levelSeconds_ = std::max(1.0f, lastSpawn + kTransitSeconds);
}

Lane BattleTelemetry::laneForRow(int row, int rows) const {
    if (rows <= 0) return Lane::Centre;
    const int third = std::max(1, rows / 3);
    if (row < third) return Lane::North;
    if (row < third * 2) return Lane::Centre;
    return Lane::South;
}

void BattleTelemetry::sample(const World& world, float dt) {
    elapsed_ += dt;
    sampleClock_ += dt;

    const Base& base = world.base();
    if (lastBaseHealth_ < 0.0f) lastBaseHealth_ = base.health;

    // A breach is the first moment the base loses health. Recorded the instant
    // it happens rather than at the sampling tick, because "03:12" is the
    // number the panel prints.
    const bool tookDamage = base.health < lastBaseHealth_ - 0.001f;
    lastBaseHealth_ = base.health;

    if (sampleClock_ < kSampleInterval && !tookDamage) return;
    sampleClock_ = 0.0f;

    // Kills per second, peaked over a whole second rather than over one
    // sample. Two kills inside a 0.1s sample is not "20 kills a second", and
    // printing it as one made the report's proudest number a lie.
    const uint32_t kills = world.totalKills();
    if (kills > lastKills_) kpsWindowKills_ += kills - lastKills_;
    lastKills_ = kills;

    kpsWindow_ += kSampleInterval;
    if (kpsWindow_ >= 1.0f) {
        peakKps_ = std::max(peakKps_,
                            static_cast<float>(kpsWindowKills_) / kpsWindow_);
        kpsWindow_ = 0.0f;
        kpsWindowKills_ = 0u;
    }

    // Row occupancy: a fixed stack array, because this runs inside a battle
    // and battles do not allocate.
    const Grid& grid = world.map().grid;
    const int rows = std::min(grid.rows(), kMaxRows);
    int histogram[kMaxRows] = {};

    const EnemyPool& enemies = world.enemies();
    for (uint32_t i = 0; i < enemies.count(); ++i) {
        int cx = 0;
        int cy = 0;
        if (grid.worldToCell(enemies.position[i], cx, cy) && cy < rows) {
            ++histogram[cy];
        }
    }

    int densestRow = 0;
    int densest = 0;
    for (int r = 0; r < rows; ++r) {
        if (histogram[r] > densest) {
            densest = histogram[r];
            densestRow = r;
        }
    }
    peakDensity_ = std::max(peakDensity_, static_cast<uint32_t>(densest));

    if (tookDamage && !breached_) {
        breached_ = true;
        breachTime_ = elapsed_;
        breachLane_ = laneForRow(densestRow, rows);
        breachDensity_ = static_cast<uint32_t>(densest);
    }
}

void BattleTelemetry::finish(const World& world, bool victory) {
    victory_ = victory;

    Attribution a;
    for (const Turret& t : world.turrets()) {
        switch (t.kind) {
            case TurretKind::MachineGun:   a.machineGun += t.kills; break;
            case TurretKind::Cannon:       a.cannon += t.kills; break;
            case TurretKind::Flamethrower: a.flamethrower += t.kills; break;
        }
    }
    a.total = world.totalKills();
    const uint32_t direct = a.machineGun + a.cannon + a.flamethrower;
    a.burn = (a.total > direct) ? (a.total - direct) : 0u;
    attribution_ = a;
}

float BattleTelemetry::estimatedDps() const {
    if (elapsed_ <= 0.0f || attribution_.total == 0u) return 0.0f;
    // Reconstructed from kills times the invasion's average health. Per-shot
    // damage is not tracked, and tracking it would put a report feature inside
    // the hot loop for a number the panel already labels an estimate.
    const float avgHp = (totalEnemies_ > 0u)
                            ? (totalHealth_ / static_cast<float>(totalEnemies_))
                            : 100.0f;
    return (static_cast<float>(attribution_.total) * avgHp) / elapsed_;
}

float BattleTelemetry::requiredDps() const {
    return totalHealth_ / levelSeconds_;
}

FailureAnalysis analyse(const BattleTelemetry& t, const UpgradeTree& tree) {
    FailureAnalysis fa;
    fa.victory = t.victory();
    fa.lane = laneName(t.breachLane());
    fa.breachTime = t.breachTime();
    fa.peakDensity = t.breached() ? t.peakDensityAtBreach() : t.peakDensity();
    fa.yourDps = static_cast<uint32_t>(std::lround(t.estimatedDps()));
    fa.requiredDps = static_cast<uint32_t>(std::lround(t.requiredDps()));

    const auto suggest = [&](NodeId node) {
        if (fa.suggestionCount >= 2) return;
        if (!isRepeatable(node) && tree.has(node)) return;
        for (int i = 0; i < fa.suggestionCount; ++i) {
            if (fa.suggestions[static_cast<size_t>(i)] == node) return;
        }
        fa.suggestions[static_cast<size_t>(fa.suggestionCount++)] = node;
    };

    // Ordered by what the diagnosis actually says, most specific first.
    const bool densityProblem = fa.peakDensity >= 40u;
    const bool bigGap = fa.requiredDps > fa.yourDps * 2u;

    if (t.hasArmored() && !tree.has(NodeId::ArmorPiercing)) {
        suggest(NodeId::ArmorPiercing);
    }
    // Something out there heals faster than it is being hurt. Fire rate is
    // the cheapest way to raise a sustained floor, and it does not care about
    // armour the way a single big shell does.
    if (t.hasRegen()) {
        suggest(NodeId::FireRate);
    }
    if (densityProblem) {
        suggest(NodeId::UnlockCannon);
        suggest(NodeId::UnlockFlamethrower);
        suggest(NodeId::Splash);
    }
    if (bigGap) {
        suggest(NodeId::MGOverclock);
        suggest(NodeId::Damage);
    }
    if (t.breached() && t.breachTime() < 25.0f) {
        suggest(NodeId::Range);
        suggest(NodeId::ExtraHardpoint);
    }
    // Something is always suggested: a report that shrugs is a report the
    // player closes.
    suggest(NodeId::Damage);
    suggest(NodeId::FireRate);

    return fa;
}

}  // namespace ls
