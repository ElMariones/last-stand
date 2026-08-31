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
    // Later sectors are tougher as well as more numerous - but only mildly.
    // The real difficulty curve is COMPOSITION: armour, burn resistance and
    // regeneration ask a build different questions, where a health multiplier
    // only asks the same question louder.
    float                    enemyHealthMult  = 1.0f;
    // Scrap per kill. Falls sharply across the campaign, because payout used
    // to be linear in enemy count while the player's damage was exponential
    // in Scrap - so the curves crossed around the sixth sector and the rest
    // of the game paid for itself. A late sector now fields forty times the
    // enemies of the first and pays about eight times as much.
    float                    killValue        = 4.0f;
    float                    depthBonusWeight = 100.0f;  // scales progress^2
    LevelMap                 map;
};

// The campaign is a graph, not a corridor: eighteen sectors across six
// difficulty tiers, 1 / 4 / 4 / 4 / 3 / 2. Every sector past the first names
// two parent sectors and opens as soon as EITHER of them has been held, so
// the tiers fan out and converge and there are many routes to the end.
constexpr int kLevelCount  = 18;
constexpr int kTierCount   = 6;
constexpr int kMaxParents  = 2;
// The widest tier, which is what the sector map has to lay out vertically.
constexpr int kMaxTierWidth = 4;

// By index, 0..kLevelCount-1. Out of range yields the first sector.
Level makeLevelByIndex(int index);

const char* levelName(int index);
const char* levelBlurb(int index);
uint32_t    levelRecommendedPower(int index);

// 0-based difficulty band. Tier 0 is always unlocked.
int         levelTier(int index);
const char* tierName(int tier);

// Fills `out` with the sectors that unlock this one and returns how many
// there are (0 for tier 0). Clearing ANY of them opens it.
int levelParents(int index, int out[kMaxParents]);

// How many sectors sit on `tier`, and the index of the `slot`-th one. The
// sector map and the campaign tests both walk the graph through these rather
// than re-deriving the layout.
int tierWidth(int tier);
int levelAtTier(int tier, int slot);

// The first three sectors by name, kept because the tests, the benchmark and
// the golden scenarios all pin specific invasions and should not be silently
// re-pointed at a different one when the campaign is re-ordered.
Level makeLevel1();   // The Outskirts
Level makeLevel2();   // Refinery Gate
Level makeLevel3();   // The Narrows

}  // namespace ls
