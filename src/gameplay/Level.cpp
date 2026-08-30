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
    lvl.enemyHealthMult = 1.15f;
    lvl.recommendedPower = 25u;
    lvl.killValue = 4.0f;
    lvl.depthBonusWeight = 250.0f;
    lvl.map = makeRefineryMap();

    addWave(lvl.schedule, 0.0f, 2.0f, 140u, 10u, EnemyType::Grunt);
    // Runners hit in fast packs: low fire-rate or rear-only coverage is
    // punished (GDD 6.1).
    addWave(lvl.schedule, 6.0f, 3.0f, 60u, 8u, EnemyType::Runner);
    addWave(lvl.schedule, 20.0f, 2.0f, 50u, 6u, EnemyType::Grunt);

    finalize(lvl);   // 250
    return lvl;
}

Level makeLevel3() {
    Level lvl;
    lvl.name = "The Narrows";
    lvl.enemyHealthMult = 1.35f;
    lvl.recommendedPower = 60u;
    lvl.killValue = 4.0f;
    lvl.depthBonusWeight = 600.0f;
    lvl.map = makeNarrowsMap();

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

Level makeLevel4() {
    Level lvl;
    lvl.name = "The Split";
    lvl.enemyHealthMult = 1.8f;
    lvl.recommendedPower = 110u;
    lvl.killValue = 4.0f;
    lvl.depthBonusWeight = 900.0f;
    lvl.map = makeSplitMap();

    // Two lanes that never meet, so the invasion is authored as two streams
    // that arrive together. Splitting your guns is not optional here.
    addWave(lvl.schedule, 0.0f, 1.6f, 300u, 14u, EnemyType::Grunt);
    addWave(lvl.schedule, 4.0f, 2.2f, 200u, 12u, EnemyType::Runner);
    addWave(lvl.schedule, 18.0f, 5.0f, 60u, 5u, EnemyType::Tank);
    addWave(lvl.schedule, 26.0f, 1.6f, 260u, 16u, EnemyType::Grunt);
    addWave(lvl.schedule, 44.0f, 2.4f, 80u, 8u, EnemyType::Runner);

    finalize(lvl);   // 900
    return lvl;
}

Level makeLevel5() {
    Level lvl;
    lvl.name = "The Spiral";
    lvl.enemyHealthMult = 3.2f;
    lvl.recommendedPower = 180u;
    lvl.killValue = 4.0f;
    lvl.depthBonusWeight = 1400.0f;
    lvl.map = makeSpiralMap();

    // Everything walks past every gun several times, so this is where range
    // and sustained fire finally beat burst.
    addWave(lvl.schedule, 0.0f, 1.4f, 500u, 18u, EnemyType::Grunt);
    addWave(lvl.schedule, 6.0f, 2.0f, 380u, 16u, EnemyType::Runner);
    addWave(lvl.schedule, 20.0f, 4.0f, 120u, 6u, EnemyType::Tank);
    addWave(lvl.schedule, 34.0f, 1.4f, 400u, 20u, EnemyType::Grunt);

    finalize(lvl);   // 1,400
    return lvl;
}

Level makeLevel6() {
    Level lvl;
    lvl.name = "Crossroads";
    lvl.enemyHealthMult = 5.5f;
    lvl.recommendedPower = 280u;
    lvl.killValue = 4.0f;
    lvl.depthBonusWeight = 2000.0f;
    lvl.map = makeCrossroadsMap();

    // Four entrances onto a central base: nothing can be defended by facing
    // one way, and the peaks are deliberately simultaneous.
    addWave(lvl.schedule, 0.0f, 1.2f, 700u, 22u, EnemyType::Grunt);
    addWave(lvl.schedule, 3.0f, 1.6f, 600u, 20u, EnemyType::Runner);
    addWave(lvl.schedule, 16.0f, 3.5f, 200u, 8u, EnemyType::Tank);
    addWave(lvl.schedule, 30.0f, 1.2f, 500u, 25u, EnemyType::Grunt);

    finalize(lvl);   // 2,000
    return lvl;
}

Level makeLevel7() {
    Level lvl;
    lvl.name = "The Gauntlet";
    lvl.enemyHealthMult = 9.0f;
    lvl.recommendedPower = 420u;
    lvl.killValue = 4.0f;
    lvl.depthBonusWeight = 3000.0f;
    lvl.map = makeGauntletMap();

    // Three chokepoints in series: compression, release, compression. The
    // sector that rewards spreading area damage across the whole run.
    addWave(lvl.schedule, 0.0f, 1.0f, 1000u, 26u, EnemyType::Grunt);
    addWave(lvl.schedule, 4.0f, 1.4f, 900u, 24u, EnemyType::Runner);
    addWave(lvl.schedule, 14.0f, 3.0f, 300u, 10u, EnemyType::Tank);
    addWave(lvl.schedule, 34.0f, 1.0f, 800u, 30u, EnemyType::Grunt);

    finalize(lvl);   // 3,000
    return lvl;
}

Level makeLevel8() {
    Level lvl;
    lvl.name = "Open Ground";
    lvl.enemyHealthMult = 15.0f;
    lvl.recommendedPower = 650u;
    lvl.killValue = 4.0f;
    lvl.depthBonusWeight = 5000.0f;
    lvl.map = makeOpenGroundMap();

    // No cover, no funnel, three sides. Purely a question of how much you can
    // kill per second, which is what the whole campaign has been building to.
    addWave(lvl.schedule, 0.0f, 0.8f, 1800u, 34u, EnemyType::Grunt);
    addWave(lvl.schedule, 3.0f, 1.1f, 1500u, 30u, EnemyType::Runner);
    addWave(lvl.schedule, 12.0f, 2.5f, 500u, 14u, EnemyType::Tank);
    addWave(lvl.schedule, 30.0f, 0.8f, 1200u, 40u, EnemyType::Grunt);

    finalize(lvl);   // 5,000
    return lvl;
}

Level makeLevelByIndex(int index) {
    switch (index) {
        case 1: return makeLevel2();
        case 2: return makeLevel3();
        case 3: return makeLevel4();
        case 4: return makeLevel5();
        case 5: return makeLevel6();
        case 6: return makeLevel7();
        case 7: return makeLevel8();
        default: return makeLevel1();
    }
}

const char* levelName(int index) {
    static const char* const kNames[kLevelCount] = {
        "The Outskirts", "Refinery Gate", "The Narrows", "The Split",
        "The Spiral",    "Crossroads",    "The Gauntlet", "Open Ground",
    };
    if (index < 0 || index >= kLevelCount) return kNames[0];
    return kNames[index];
}

uint32_t levelRecommendedPower(int index) {
    static const uint32_t kPower[kLevelCount] = {10u,  25u,  60u,  110u,
                                                 180u, 280u, 420u, 650u};
    if (index < 0 || index >= kLevelCount) return kPower[0];
    return kPower[index];
}

}  // namespace ls
