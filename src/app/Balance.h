#pragma once
#include <cstdint>
#include <vector>

#include "app/Cli.h"
#include "gameplay/UpgradeTree.h"

namespace ls {

// One run of the loop, as an auto-player experienced it.
struct BalanceRun {
    int      index        = 0;
    int      level        = 0;
    bool     victory      = false;
    uint32_t kills        = 0u;
    uint32_t totalEnemies = 0u;
    float    seconds      = 0.0f;
    uint32_t payout       = 0u;
    uint32_t scrapAfter   = 0u;
    int      bought       = 0;
    NodeId   lastBought   = NodeId::Damage;
};

struct BalanceReport {
    std::vector<BalanceRun> runs;
    int      runsToFirstPurchase = -1;
    int      runsToFirstClear    = -1;
    uint32_t peakKills           = 0u;
};

// Plays the game headlessly with a greedy-but-plausible purchaser: it takes
// the two nodes the game's own Failure Analysis recommended if it can afford
// them, and otherwise buys the cheapest thing available. This is the economy's
// regression test and its tuning instrument at once — "how many runs until the
// player can buy anything" is a number, not a feeling.
BalanceReport runBalance(int runs, int levelIndex = 0);

void printBalance(const BalanceReport& report);

}  // namespace ls
