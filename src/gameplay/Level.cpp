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

// ------------------------------------------------------------- the graph ---

struct SectorInfo {
    const char* name;
    const char* blurb;
    int         tier;
    int         parents[kMaxParents];   // -1 for "none"
    uint32_t    power;
    float       killValue;
    float       healthMult;
    float       depthWeight;
};

// Kill value falls tier by tier on purpose. The old campaign paid a flat 4.0
// a kill, so income was linear in enemy count while the upgrade tree made
// damage exponential in Scrap - the two curves crossed around the sixth
// sector, and everything after it was a formality funded by the sector
// before. The last tier fields forty times the first tier's invasion and
// pays about eight times as much for it.
constexpr SectorInfo kSectors[kLevelCount] = {
    // 0 ------------------------------------------------------------- tier 0
    {"The Outskirts", "One lane, one chokepoint. Where it starts.",
     0, {-1, -1}, 10u, 4.00f, 1.00f, 100.0f},

    // 1..4 ---------------------------------------------------------- tier 1
    {"Refinery Gate", "Two lanes converging. Rear-only coverage dies here.",
     1, {0, -1}, 26u, 3.20f, 1.10f, 240.0f},
    {"The Narrows", "An open approach into a hard funnel.",
     1, {0, -1}, 30u, 3.20f, 1.10f, 260.0f},
    {"Culvert", "A corridor folded in three. Everything passes you twice.",
     1, {0, -1}, 28u, 3.20f, 1.15f, 250.0f},
    {"Scrapyard", "Broad front, scattered cover. First swarm.",
     1, {0, -1}, 32u, 3.20f, 1.05f, 280.0f},

    // 5..8 ---------------------------------------------------------- tier 2
    {"The Split", "Two paths that never meet. Split your guns.",
     2, {1, 2}, 70u, 2.40f, 1.45f, 600.0f},
    {"Foundry", "One huge block, two ways round. Cover both.",
     2, {2, 3}, 72u, 2.40f, 1.50f, 600.0f},
    {"Aqueduct", "Three sealed lanes. Nothing helps anywhere else.",
     2, {3, 4}, 76u, 2.40f, 1.40f, 650.0f},
    {"The Hollow", "An open bowl. Armour arrives, and bullets bounce.",
     2, {4, 1}, 80u, 2.40f, 1.55f, 680.0f},

    // 9..12 --------------------------------------------------------- tier 3
    {"The Spiral", "One long switchback. Range beats burst here.",
     3, {5, 6}, 150u, 1.70f, 2.00f, 1200.0f},
    {"Crossroads", "Four entrances. Nothing is defended facing one way.",
     3, {6, 7}, 160u, 1.70f, 2.05f, 1300.0f},
    {"Catacombs", "Offset gaps, short sightlines. Something weaves.",
     3, {7, 8}, 155u, 1.70f, 2.10f, 1250.0f},
    {"The Pit", "Four corners onto a centre you cannot cover twice.",
     3, {8, 5}, 165u, 1.70f, 2.15f, 1300.0f},

    // 13..15 -------------------------------------------------------- tier 4
    {"The Gauntlet", "Three chokepoints in series. Compress, release.",
     4, {9, 10}, 300u, 1.20f, 2.90f, 2200.0f},
    {"Meatgrinder", "A ring corridor. Survivors walked the whole circuit.",
     4, {10, 11}, 310u, 1.20f, 3.00f, 2200.0f},
    {"Causeway", "Two sprinting lanes onto one bridge. All at once.",
     4, {11, 12}, 320u, 1.20f, 2.80f, 2400.0f},

    // 16..17 -------------------------------------------------------- tier 5
    {"Open Ground", "No cover, three sides. Purely how fast you kill.",
     5, {13, 14}, 520u, 0.85f, 4.20f, 3600.0f},
    {"The Breach", "Broken lines, every side, and things that heal.",
     5, {14, 15}, 560u, 0.85f, 4.50f, 3900.0f},
};

LevelMap mapForIndex(int index) {
    switch (index) {
        case 0:  return makeOutskirtsMap();
        case 1:  return makeRefineryMap();
        case 2:  return makeNarrowsMap();
        case 3:  return makeCulvertMap();
        case 4:  return makeScrapyardMap();
        case 5:  return makeSplitMap();
        case 6:  return makeFoundryMap();
        case 7:  return makeAqueductMap();
        case 8:  return makeHollowMap();
        case 9:  return makeSpiralMap();
        case 10: return makeCrossroadsMap();
        case 11: return makeCatacombsMap();
        case 12: return makePitMap();
        case 13: return makeGauntletMap();
        case 14: return makeMeatgrinderMap();
        case 15: return makeCausewayMap();
        case 16: return makeOpenGroundMap();
        default: return makeBreachMap();
    }
}

// The invasion for each sector. Kept separate from the SectorInfo table
// because a schedule is a paragraph and a sector is a row.
void buildSchedule(int index, std::vector<SpawnEvent>& s) {
    using E = EnemyType;
    switch (index) {
        case 0: {
            // Sparse opening, density peaks mid-battle, thins to a tail.
            const struct { float t; uint32_t n; } bursts[] = {
                {0.0f, 8u},   {2.0f, 6u},   {4.0f, 6u},   {6.0f, 8u},
                {9.0f, 10u},  {12.0f, 12u}, {15.0f, 14u}, {18.0f, 12u},
                {21.0f, 10u}, {24.0f, 6u},  {28.0f, 4u},  {32.0f, 4u},
            };
            for (const auto& b : bursts) s.push_back(SpawnEvent{b.t, b.n, E::Grunt});
            break;                                              // 100
        }
        case 1:
            addWave(s, 0.0f, 2.0f, 140u, 10u, E::Grunt);
            // Runners hit in fast packs: low fire-rate or rear-only coverage
            // is punished (GDD 6.1).
            addWave(s, 6.0f, 3.0f, 60u, 8u, E::Runner);
            addWave(s, 20.0f, 2.0f, 50u, 6u, E::Grunt);
            break;                                              // 250
        case 2:
            addWave(s, 0.0f, 2.0f, 180u, 12u, E::Grunt);
            addWave(s, 5.0f, 2.5f, 100u, 10u, E::Runner);
            addWave(s, 26.0f, 2.0f, 40u, 8u, E::Grunt);
            break;                                              // 320
        case 3:
            // A long walk, so the invasion is slow and heavy rather than fast.
            addWave(s, 0.0f, 2.2f, 240u, 12u, E::Grunt);
            addWave(s, 18.0f, 3.0f, 40u, 8u, E::Runner);
            break;                                              // 280
        case 4:
            // Swarmers: nearly free individually, and they arrive as a tide
            // that packs tight rather than a queue that spreads out.
            addWave(s, 0.0f, 2.0f, 120u, 10u, E::Grunt);
            addWave(s, 4.0f, 1.6f, 240u, 24u, E::Swarmer);
            addWave(s, 22.0f, 2.5f, 40u, 8u, E::Runner);
            break;                                              // 400
        case 5:
            addWave(s, 0.0f, 1.6f, 340u, 14u, E::Grunt);
            addWave(s, 4.0f, 2.2f, 220u, 12u, E::Runner);
            // Tanks are the check on pure-AoE builds (GDD 6.1).
            addWave(s, 18.0f, 5.0f, 40u, 4u, E::Tank);
            addWave(s, 30.0f, 1.8f, 100u, 12u, E::Grunt);
            break;                                              // 700
        case 6:
            addWave(s, 0.0f, 1.8f, 300u, 14u, E::Grunt);
            addWave(s, 5.0f, 2.4f, 180u, 12u, E::Runner);
            addWave(s, 16.0f, 5.5f, 30u, 3u, E::Tank);
            addWave(s, 24.0f, 1.6f, 140u, 20u, E::Swarmer);
            break;                                              // 650
        case 7:
            // Three lanes, so three streams that never help each other.
            addWave(s, 0.0f, 1.4f, 420u, 28u, E::Swarmer);
            addWave(s, 3.0f, 1.8f, 300u, 16u, E::Grunt);
            addWave(s, 20.0f, 2.2f, 180u, 14u, E::Runner);
            break;                                              // 900
        case 8:
            addWave(s, 0.0f, 1.6f, 380u, 16u, E::Grunt);
            // Brutes: seven points of armour off every hit, so a fast weak
            // gun does nothing and damage-per-shot is suddenly the stat that
            // matters.
            addWave(s, 8.0f, 4.0f, 50u, 4u, E::Brute);
            addWave(s, 14.0f, 2.0f, 220u, 14u, E::Runner);
            addWave(s, 30.0f, 1.8f, 100u, 18u, E::Swarmer);
            break;                                              // 750
        case 9:
            addWave(s, 0.0f, 1.4f, 560u, 18u, E::Grunt);
            addWave(s, 6.0f, 2.0f, 380u, 16u, E::Runner);
            addWave(s, 18.0f, 4.5f, 60u, 5u, E::Tank);
            addWave(s, 24.0f, 5.0f, 60u, 5u, E::Brute);
            addWave(s, 34.0f, 1.4f, 240u, 24u, E::Swarmer);
            break;                                              // 1300
        case 10:
            addWave(s, 0.0f, 1.2f, 600u, 22u, E::Grunt);
            addWave(s, 3.0f, 1.6f, 500u, 20u, E::Runner);
            addWave(s, 12.0f, 1.5f, 280u, 26u, E::Swarmer);
            addWave(s, 20.0f, 3.5f, 70u, 6u, E::Tank);
            addWave(s, 28.0f, 4.0f, 50u, 4u, E::Brute);
            break;                                              // 1500
        case 11:
            // Phantoms weave across their own path and shrug off fire, so
            // the short sightlines here are the worst possible place to meet
            // them with a flamethrower wall.
            addWave(s, 0.0f, 1.5f, 420u, 18u, E::Grunt);
            addWave(s, 5.0f, 2.0f, 200u, 10u, E::Phantom);
            addWave(s, 10.0f, 1.3f, 400u, 28u, E::Swarmer);
            addWave(s, 22.0f, 3.5f, 80u, 6u, E::Brute);
            addWave(s, 32.0f, 2.0f, 100u, 12u, E::Runner);
            break;                                              // 1200
        case 12:
            addWave(s, 0.0f, 1.3f, 500u, 20u, E::Grunt);
            addWave(s, 3.0f, 1.6f, 400u, 18u, E::Runner);
            addWave(s, 12.0f, 2.2f, 150u, 10u, E::Phantom);
            addWave(s, 18.0f, 3.5f, 80u, 6u, E::Tank);
            addWave(s, 24.0f, 4.0f, 70u, 5u, E::Brute);
            addWave(s, 30.0f, 1.4f, 200u, 24u, E::Swarmer);
            break;                                              // 1400
        case 13:
            addWave(s, 0.0f, 1.0f, 800u, 26u, E::Grunt);
            addWave(s, 4.0f, 1.4f, 600u, 24u, E::Runner);
            addWave(s, 10.0f, 1.2f, 500u, 30u, E::Swarmer);
            addWave(s, 18.0f, 3.0f, 120u, 8u, E::Tank);
            addWave(s, 24.0f, 3.0f, 120u, 8u, E::Brute);
            addWave(s, 34.0f, 2.5f, 60u, 6u, E::Phantom);
            break;                                              // 2200
        case 14:
            addWave(s, 0.0f, 1.1f, 700u, 24u, E::Grunt);
            addWave(s, 3.0f, 1.1f, 700u, 32u, E::Swarmer);
            addWave(s, 14.0f, 2.4f, 200u, 10u, E::Brute);
            addWave(s, 20.0f, 2.4f, 200u, 12u, E::Phantom);
            addWave(s, 28.0f, 3.0f, 100u, 8u, E::Tank);
            addWave(s, 36.0f, 2.0f, 100u, 12u, E::Runner);
            break;                                              // 2000
        case 15:
            // Wide lanes and a bridge: this is the sector runners were built
            // for, and they arrive faster than anything before them.
            addWave(s, 0.0f, 1.0f, 1100u, 30u, E::Runner);
            addWave(s, 5.0f, 1.1f, 800u, 34u, E::Swarmer);
            addWave(s, 14.0f, 1.4f, 500u, 24u, E::Grunt);
            addWave(s, 24.0f, 2.6f, 150u, 10u, E::Phantom);
            addWave(s, 34.0f, 3.5f, 50u, 5u, E::Brute);
            break;                                              // 2600
        case 16:
            addWave(s, 0.0f, 0.8f, 1400u, 34u, E::Grunt);
            addWave(s, 3.0f, 1.0f, 1100u, 30u, E::Runner);
            addWave(s, 8.0f, 0.9f, 900u, 36u, E::Swarmer);
            addWave(s, 16.0f, 2.5f, 180u, 12u, E::Tank);
            addWave(s, 22.0f, 2.5f, 180u, 12u, E::Brute);
            addWave(s, 30.0f, 3.0f, 30u, 5u, E::Phantom);
            // Behemoths regenerate. Ten of them is a standing demand for a
            // damage-per-second floor: chip at one and it heals the chip off.
            addWave(s, 34.0f, 8.0f, 10u, 1u, E::Behemoth);
            break;                                              // 3800
        default:
            addWave(s, 0.0f, 0.8f, 1500u, 36u, E::Grunt);
            addWave(s, 2.0f, 0.9f, 1200u, 32u, E::Runner);
            addWave(s, 6.0f, 0.8f, 1000u, 38u, E::Swarmer);
            addWave(s, 14.0f, 2.0f, 250u, 14u, E::Brute);
            addWave(s, 20.0f, 2.0f, 200u, 12u, E::Phantom);
            addWave(s, 28.0f, 4.0f, 30u, 5u, E::Tank);
            addWave(s, 30.0f, 6.0f, 20u, 2u, E::Behemoth);
            break;                                              // 4200
    }
}

int clampIndex(int index) {
    return (index < 0 || index >= kLevelCount) ? 0 : index;
}

}  // namespace

Level makeLevelByIndex(int index) {
    const int i = clampIndex(index);
    const SectorInfo& info = kSectors[static_cast<size_t>(i)];

    Level lvl;
    lvl.name             = info.name;
    lvl.recommendedPower = info.power;
    lvl.killValue        = info.killValue;
    lvl.enemyHealthMult  = info.healthMult;
    lvl.depthBonusWeight = info.depthWeight;
    lvl.map              = mapForIndex(i);
    buildSchedule(i, lvl.schedule);
    finalize(lvl);
    return lvl;
}

const char* levelName(int index) {
    return kSectors[static_cast<size_t>(clampIndex(index))].name;
}

const char* levelBlurb(int index) {
    return kSectors[static_cast<size_t>(clampIndex(index))].blurb;
}

uint32_t levelRecommendedPower(int index) {
    return kSectors[static_cast<size_t>(clampIndex(index))].power;
}

int levelTier(int index) {
    return kSectors[static_cast<size_t>(clampIndex(index))].tier;
}

const char* tierName(int tier) {
    static const char* const kNames[kTierCount] = {
        "CONTACT", "THE PERIMETER", "THE BELT",
        "DEEP GROUND", "THE APPROACH", "LAST STAND",
    };
    if (tier < 0 || tier >= kTierCount) return kNames[0];
    return kNames[tier];
}

int levelParents(int index, int out[kMaxParents]) {
    const SectorInfo& info = kSectors[static_cast<size_t>(clampIndex(index))];
    int n = 0;
    for (int i = 0; i < kMaxParents; ++i) {
        if (info.parents[i] < 0) continue;
        out[n++] = info.parents[i];
    }
    return n;
}

int tierWidth(int tier) {
    int n = 0;
    for (int i = 0; i < kLevelCount; ++i) {
        if (kSectors[static_cast<size_t>(i)].tier == tier) ++n;
    }
    return n;
}

int levelAtTier(int tier, int slot) {
    int n = 0;
    for (int i = 0; i < kLevelCount; ++i) {
        if (kSectors[static_cast<size_t>(i)].tier != tier) continue;
        if (n == slot) return i;
        ++n;
    }
    return 0;
}

Level makeLevel1() { return makeLevelByIndex(0); }
Level makeLevel2() { return makeLevelByIndex(1); }
Level makeLevel3() { return makeLevelByIndex(2); }

}  // namespace ls
