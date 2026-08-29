#pragma once
#include <cstdint>

namespace ls {

// The outcome of a finished battle, everything computePayout needs.
struct BattleResult {
    bool     victory       = false;
    uint32_t kills         = 0u;
    uint32_t totalEnemies  = 0u;    // spawned during the battle
    uint32_t previousBest  = 0u;    // this level's best before the battle
    uint32_t clearCount    = 0u;    // prior successful clears of this level
};

// The Scrap awarded for a battle, with the breakdown the Battle Report shows
// (GDD 13.2 makes each of these a visible line).
struct Payout {
    uint32_t scrap      = 0u;
    uint32_t killScrap  = 0u;
    uint32_t depthScrap = 0u;
    uint32_t bestBonus  = 0u;
    bool     newBest    = false;
    float    multiplier = 1.0f;     // defeat 0.75 and/or diminishing replay
};

// GDD 8: scrap = (kills * killValue * scrapMult) + (depthWeight * progress^2)
//               + personalBestBonus, times the defeat/replay factor.
// Defeat pays 75% of the identical victory (8.3); personal best pays even on
// a loss (8.5); replaying a cleared level pays progressively less, floored at
// 25% (8.4).
Payout computePayout(const BattleResult& r, float killValue,
                     float depthWeight, float scrapMult);

}  // namespace ls
