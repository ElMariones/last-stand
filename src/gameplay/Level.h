#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "sim/LevelMap.h"

namespace ls {

// A spawn burst: `count` enemies emitted at `timeSeconds` into the battle.
struct SpawnEvent {
    float    timeSeconds = 0.0f;
    uint32_t count       = 0u;
};

// A fixed, finite, deterministic invasion (GDD 4.1). The schedule is authored
// once and precomputed; per-enemy placement RNG still flows from the seeded
// Pcg32 inside World, so the whole battle is reproducible from the level seed.
struct Level {
    std::string              name;
    uint32_t                 recommendedPower = 0u;
    uint32_t                 totalEnemies     = 0u;
    std::vector<SpawnEvent>  schedule;         // sorted by time
    float                    killValue        = 3.0f;    // Scrap per kill
    float                    depthBonusWeight = 100.0f;  // scales progress^2
    LevelMap                 map;
};

// Level 1 "The Outskirts" — 100 Grunts over ~60s, sparse opening, a mid peak,
// then a thinning tail (troughs to breathe, GDD 4.1). Machine-gun turrets on
// the four authored hardpoints are the only defense.
Level makeLevel1();

}  // namespace ls
