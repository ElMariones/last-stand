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
    float                    killValue        = 3.0f;    // Scrap per kill
    float                    depthBonusWeight = 100.0f;  // scales progress^2
    LevelMap                 map;
};

// Level 1 "The Outskirts" — 100 Grunts over ~60s, sparse opening, a mid peak,
// then a thinning tail (troughs to breathe, GDD 4.1). Machine-gun turrets on
// the four authored hardpoints are the only defense.
Level makeLevel1();

// Level 2 "Refinery Gate" — 250 enemies, Grunt + Runner mix (GDD 9.1).
Level makeLevel2();

// Level 3 "The Narrows" — 600 enemies, Grunt + Runner + Tank (GDD 9.1).
Level makeLevel3();

}  // namespace ls
