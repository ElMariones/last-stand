#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "sim/EnemyType.h"
#include "sim/LevelMap.h"

namespace ls {

// A spawn burst: `count` enemies of `type` emitted at `timeSeconds`.
struct SpawnEvent {
    float     timeSeconds = 0.0f;
    uint32_t  count       = 0u;
    EnemyType type        = EnemyType::Grunt;
};

// A fixed, finite, deterministic invasion (GDD 4.1). The schedule is authored
// once and precomputed; per-enemy placement RNG still flows from the seeded
// Pcg32 inside World, so the whole battle is reproducible from the level seed.
struct Level {
    std::string              name;
    uint32_t                 recommendedPower = 0u;
    uint32_t                 totalEnemies     = 0u;
    std::vector<SpawnEvent>  schedule;         // sorted by time
    // Later sectors are not just more numerous, they are tougher. Without
    // this the totals climb but the player's damage climbs faster, and every
    // sector after the third falls on the first attempt — which is a campaign
    // with no reason to retry anything.
    float                    enemyHealthMult  = 1.0f;
    float                    killValue        = 3.0f;    // Scrap per kill
    float                    depthBonusWeight = 100.0f;  // scales progress^2
    LevelMap                 map;
};

// The eight sectors, in unlock order. Each pairs a distinct map with a
// distinct invasion; the curve is troughs to breathe and peaks to survive
// (GDD 4.1), and the totals climb an order of magnitude across the campaign
// so the power delta is visible in the number of things dying (pillar 1).
constexpr int kLevelCount = 8;

Level makeLevel1();   // The Outskirts    100  grunts
Level makeLevel2();   // Refinery Gate    250  + runners
Level makeLevel3();   // The Narrows      600  + tanks
Level makeLevel4();   // The Split        900
Level makeLevel5();   // The Spiral     1,400
Level makeLevel6();   // Crossroads     2,000
Level makeLevel7();   // The Gauntlet   3,000
Level makeLevel8();   // Open Ground    5,000

// By index, 0..kLevelCount-1. Out of range yields sector 1.
Level makeLevelByIndex(int index);
const char* levelName(int index);
uint32_t    levelRecommendedPower(int index);

}  // namespace ls
