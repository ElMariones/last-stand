#include "gameplay/Level.h"

namespace ls {

Level makeLevel1() {
    Level lvl;
    lvl.name = "The Outskirts";
    lvl.recommendedPower = 10u;
    lvl.killValue = 3.0f;
    lvl.depthBonusWeight = 100.0f;
    lvl.map = makeM1Map();

    // 100 Grunts: sparse opening, density peaks mid-battle, thins to a tail.
    // Bursts are authored counts at whole-second times.
    const struct { float t; uint32_t n; } bursts[] = {
        {0.0f, 8u},   {2.0f, 6u},  {4.0f, 6u},   {6.0f, 8u},
        {9.0f, 10u},  {12.0f, 12u}, {15.0f, 14u}, {18.0f, 12u},
        {21.0f, 10u}, {24.0f, 6u}, {28.0f, 4u},  {32.0f, 4u},
    };

    uint32_t total = 0u;
    for (const auto& b : bursts) {
        lvl.schedule.push_back(SpawnEvent{b.t, b.n});
        total += b.n;
    }
    lvl.totalEnemies = total;   // 100
    return lvl;
}

}  // namespace ls
