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
    float    multiplier   = 1.0f;   // the defeat / diminishing-replay factor
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

// --------------------------------------------------------- the difficulty ---

// A campaign playthrough only ever tests one budget per sector: whatever the
// auto-player happened to arrive with. That cannot answer "is the last sector
// a challenge", because the answer depends entirely on what you bring. The
// matrix drops a fresh player onto EVERY sector at several fixed budgets, so
// the difficulty curve is a grid you can read down and across.
constexpr int kMaxBudgets = 8;

struct MatrixCell {
    int      budget       = 0;
    bool     victory      = false;
    uint32_t kills        = 0u;
    uint32_t totalEnemies = 0u;
    float    seconds      = 0.0f;
};

struct MatrixRow {
    int        level = 0;
    int        count = 0;
    MatrixCell cells[kMaxBudgets];
};

struct MatrixReport {
    std::vector<MatrixRow> rows;
};

MatrixReport runMatrix(const int* budgets, int budgetCount);
void         printMatrix(const MatrixReport& report);

}  // namespace ls
