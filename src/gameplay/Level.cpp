#include "gameplay/Level.h"

#include <algorithm>

namespace ls {

namespace {

// Appends a schedule that emits `count` enemies of `type` in bursts of
// `perBurst` starting at `t0`, `gap` seconds apart.
void addWave(std::vector<SpawnEvent>& schedule, float t0, float gap,
             uint32_t count, uint32_t perBurst, EnemyType type) {
    uint32_t remaining = count;
    float t = t0;
    while (remaining > 0u) {
        const uint32_t n = remaining < perBurst ? remaining : perBurst;
        schedule.push_back(SpawnEvent{t, n, type});
        remaining -= n;
        t += gap;
    }
}

// SpawnDirector releases events in order and stops at the first one that is
// not yet due, so a schedule assembled from several overlapping addWave calls
// MUST be sorted by time or later waves are held back and then dumped in a
// single tick. stable_sort keeps same-time bursts in authoring order, which
// keeps the level deterministic.
void finalize(Level& lvl) {
    std::stable_sort(lvl.schedule.begin(), lvl.schedule.end(),
                     [](const SpawnEvent& a, const SpawnEvent& b) {
                         return a.timeSeconds < b.timeSeconds;
                     });
    uint32_t total = 0u;
    for (const SpawnEvent& e : lvl.schedule) total += e.count;
    lvl.totalEnemies = total;
}

}  // namespace

Level makeLevel1() {
    Level lvl;
    lvl.name = "The Outskirts";
    lvl.recommendedPower = 10u;
    lvl.killValue = 4.0f;
    lvl.depthBonusWeight = 100.0f;
    lvl.map = makeM1Map();

    // 100 Grunts: sparse opening, density peaks mid-battle, thins to a tail.
    const struct { float t; uint32_t n; } bursts[] = {
        {0.0f, 8u},   {2.0f, 6u},  {4.0f, 6u},   {6.0f, 8u},
        {9.0f, 10u},  {12.0f, 12u}, {15.0f, 14u}, {18.0f, 12u},
        {21.0f, 10u}, {24.0f, 6u}, {28.0f, 4u},  {32.0f, 4u},
    };

    for (const auto& b : bursts) {
        lvl.schedule.push_back(SpawnEvent{b.t, b.n, EnemyType::Grunt});
    }
    finalize(lvl);   // 100
    return lvl;
}

Level makeLevel2() {
    Level lvl;
    lvl.name = "Refinery Gate";
    lvl.recommendedPower = 25u;
    lvl.killValue = 4.0f;
    lvl.depthBonusWeight = 250.0f;
    lvl.map = makeM1Map();

    addWave(lvl.schedule, 0.0f, 2.0f, 140u, 10u, EnemyType::Grunt);
    // Runners hit in fast packs: low fire-rate / rear-only coverage is
    // punished (GDD 6.1).
    addWave(lvl.schedule, 6.0f, 3.0f, 60u, 8u, EnemyType::Runner);
    addWave(lvl.schedule, 20.0f, 2.0f, 50u, 6u, EnemyType::Grunt);

    finalize(lvl);   // 250
    return lvl;
}

Level makeLevel3() {
    Level lvl;
    lvl.name = "The Narrows";
    lvl.recommendedPower = 60u;
    lvl.killValue = 4.0f;
    lvl.depthBonusWeight = 600.0f;
    lvl.map = makeM1Map();

    addWave(lvl.schedule, 0.0f, 2.0f, 160u, 12u, EnemyType::Grunt);
    addWave(lvl.schedule, 5.0f, 3.0f, 90u, 8u, EnemyType::Runner);
    // Tanks are the check on pure-AoE builds (GDD 6.1): a handful of very
    // slow, very tough targets amid the crowd.
    addWave(lvl.schedule, 12.0f, 6.0f, 40u, 4u, EnemyType::Tank);
    addWave(lvl.schedule, 22.0f, 2.0f, 160u, 12u, EnemyType::Grunt);
    addWave(lvl.schedule, 30.0f, 4.0f, 90u, 8u, EnemyType::Runner);
    addWave(lvl.schedule, 40.0f, 6.0f, 30u, 4u, EnemyType::Tank);
    addWave(lvl.schedule, 50.0f, 2.0f, 30u, 6u, EnemyType::Runner);

    finalize(lvl);   // 600
    return lvl;
}

}  // namespace ls
