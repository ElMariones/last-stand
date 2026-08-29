#include "gameplay/Progression.h"

#include <algorithm>
#include <cmath>

namespace ls {

namespace {

// Diminishing-replay factor by number of PRIOR clears (GDD 8.4), floored at
// 0.25 from the fifth replay onwards.
constexpr float kReplay[5] = {1.0f, 0.7f, 0.5f, 0.35f, 0.25f};
constexpr float kDefeatFactor = 0.75f;
constexpr uint32_t kBestBonus = 50u;

}  // namespace

Payout computePayout(const BattleResult& r, float killValue,
                     float depthWeight, float scrapMult) {
    const float progress = (r.totalEnemies == 0u)
                               ? 0.0f
                               : static_cast<float>(r.kills) /
                                     static_cast<float>(r.totalEnemies);

    Payout p;
    p.killScrap = static_cast<uint32_t>(std::llround(
        static_cast<float>(r.kills) * killValue * scrapMult));
    p.depthScrap = static_cast<uint32_t>(
        std::llround(depthWeight * progress * progress));

    p.newBest = r.kills > r.previousBest;
    p.bestBonus =
        (p.newBest && r.previousBest > 0u) ? kBestBonus : 0u;

    if (r.victory) {
        p.multiplier = kReplay[std::min<uint32_t>(r.clearCount, 4u)];
    } else {
        p.multiplier = kDefeatFactor;
    }

    const double base = static_cast<double>(p.killScrap) +
                        static_cast<double>(p.depthScrap) +
                        static_cast<double>(p.bestBonus);
    p.scrap = static_cast<uint32_t>(
        std::llround(base * static_cast<double>(p.multiplier)));
    return p;
}

}  // namespace ls
