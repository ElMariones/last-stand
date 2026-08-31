#include <doctest/doctest.h>

#include "app/Balance.h"
#include "gameplay/Level.h"

// The economy has a shape, and the shape is the design: lose the first run,
// be able to buy something anyway, see the number go up nearly every run, and
// clear the first sector inside a session rather than an evening. Tuning a
// constant without checking this is how an incremental game quietly becomes a
// grind, so the curve is a test.

TEST_CASE("losing the first run still pays for an upgrade") {
    // GDD pillar 2: a defeat is a transaction, never a punishment. If the
    // first loss cannot buy anything, the loop has not started yet.
    const ls::BalanceReport r = ls::runBalance(3);
    REQUIRE(r.runs.size() == 3u);
    CHECK_FALSE(r.runs[0].victory);
    CHECK(r.runs[0].payout > 0u);
    CHECK(r.runsToFirstPurchase == 1);
}

TEST_CASE("the first sector falls within a handful of runs") {
    const ls::BalanceReport r = ls::runBalance(10);
    REQUIRE(r.runsToFirstClear > 0);
    CHECK(r.runsToFirstClear >= 2);    // never on the first try (pillar 2)
    CHECK(r.runsToFirstClear <= 8);    // and never a grind
}

TEST_CASE("progress is visible: no long flat stretch of identical runs") {
    // Six identical runs in a row is the failure mode this catches — the
    // player buys upgrades and nothing on screen changes.
    const ls::BalanceReport r = ls::runBalance(10);

    int flatRun = 0;
    int worstFlatRun = 0;
    for (size_t i = 1; i < r.runs.size(); ++i) {
        if (r.runs[i].kills == r.runs[i - 1].kills) {
            ++flatRun;
            worstFlatRun = (flatRun > worstFlatRun) ? flatRun : worstFlatRun;
        } else {
            flatRun = 0;
        }
    }
    CHECK(worstFlatRun <= 2);
}

TEST_CASE("upgrades never make a run worse") {
    // Kills may plateau, but a run that spent Scrap must never come back with
    // a smaller number than the run before it.
    const ls::BalanceReport r = ls::runBalance(8);
    for (size_t i = 1; i < r.runs.size(); ++i) {
        if (r.runs[i].level != r.runs[i - 1].level) continue;   // new sector
        CHECK(r.runs[i].kills >= r.runs[i - 1].kills);
    }
}

TEST_CASE("the power delta across a session is enormous, not incremental") {
    // Pillar 1: the player should SEE the progress in the number of things
    // dying, without reading a stat sheet.
    const ls::BalanceReport r = ls::runBalance(14);
    REQUIRE(r.runs.size() >= 14u);
    CHECK(r.peakKills >= r.runs[0].kills * 20u);
}

TEST_CASE("the whole campaign is reachable and clearable") {
    // Every sector must be beatable by a player who follows the game's own
    // advice. A sector nobody can clear is content that does not exist.
    const ls::BalanceReport r = ls::runBalance(30);
    int deepest = 0;
    bool clearedLast = false;
    for (const ls::BalanceRun& run : r.runs) {
        if (run.level > deepest) deepest = run.level;
        if (run.level == ls::kLevelCount - 1 && run.victory) clearedLast = true;
    }
    CHECK(deepest == ls::kLevelCount - 1);
    CHECK(clearedLast);
}

TEST_CASE("later sectors are not free: most of them cost a retry") {
    // Sectors that fall on the first attempt every time are filler. The
    // campaign should ask for at least a couple of second tries.
    const ls::BalanceReport r = ls::runBalance(24);
    int lossesAfterFirstSector = 0;
    for (const ls::BalanceRun& run : r.runs) {
        if (run.level > 0 && !run.victory) ++lossesAfterFirstSector;
    }
    CHECK(lossesAfterFirstSector >= 4);
}

TEST_CASE("replaying a cleared sector pays a shrinking multiplier") {
    // GDD 8.4's anti-grind rule, observed end to end. Tested on the
    // multiplier rather than the gross payout: a player who keeps buying
    // Economy can out-earn the decay for a while, and that is fine - the
    // rule is that the same sector is worth less each time, not that the
    // player's income can never grow.
    //
    // Which sector gets replayed is no longer knowable in advance: the
    // campaign is a graph, and the auto-player takes whichever route the
    // game itself suggests. So this finds the sector it actually settled on
    // rather than assuming it was the last one.
    const ls::BalanceReport r = ls::runBalance(40);

    int counts[ls::kLevelCount] = {};
    for (const ls::BalanceRun& run : r.runs) {
        if (run.victory) ++counts[run.level];
    }
    int repeated = -1;
    for (int i = 0; i < ls::kLevelCount; ++i) {
        if (counts[i] >= 3 && (repeated < 0 || counts[i] > counts[repeated])) {
            repeated = i;
        }
    }
    REQUIRE(repeated >= 0);

    float firstClear = 0.0f;
    float lastRepeat = 0.0f;
    int clears = 0;
    for (const ls::BalanceRun& run : r.runs) {
        if (run.level != repeated || !run.victory) continue;
        if (clears == 0) firstClear = run.multiplier;
        lastRepeat = run.multiplier;
        ++clears;
    }
    CHECK(firstClear == doctest::Approx(1.0f));
    CHECK(lastRepeat < firstClear);
}

TEST_CASE("the campaign keeps the player broke while it lasts") {
    // The complaint this economy pass answers: it was easy to get rich. Kill
    // value used to be flat, so income was linear in enemy count while the
    // tree made damage exponential in Scrap, and by the middle of the
    // campaign every purchase was affordable the moment it was wanted.
    //
    // While there is still a sector to take, the player should end most runs
    // having spent nearly everything. Afterwards Scrap piling up is correct -
    // there is nothing left to buy.
    const ls::BalanceReport r = ls::runBalance(20);

    int flush = 0;
    int counted = 0;
    for (const ls::BalanceRun& run : r.runs) {
        if (run.payout == 0u) continue;
        ++counted;
        // "Rich" means walking away from a run still holding a third of what
        // it paid, having already bought everything worth buying.
        if (run.scrapAfter * 3u > run.payout) ++flush;
    }
    REQUIRE(counted >= 15);
    CHECK(flush * 4 <= counted);
}

TEST_CASE("the last tier costs an order of magnitude more than the first") {
    // The difficulty matrix as an assertion. Each cell is a fresh player
    // handed a fixed budget, so this measures the sectors themselves rather
    // than the order the auto-player happened to meet them in.
    const int budgets[3] = {400, 5000, 80000};
    const ls::MatrixReport m = ls::runMatrix(budgets, 3);
    REQUIRE(m.rows.size() == static_cast<size_t>(ls::kLevelCount));

    for (const ls::MatrixRow& row : m.rows) {
        CAPTURE(row.level);
        // Everything is beatable by someone who brought enough. A sector that
        // cannot be cleared at any budget is content that does not exist.
        CHECK(row.cells[2].victory);

        // ...and nothing in the last tier is beatable on pocket change, which
        // is what "the last levels do not feel like a challenge" meant.
        if (ls::levelTier(row.level) >= ls::kTierCount - 2) {
            CHECK_FALSE(row.cells[0].victory);
            CHECK_FALSE(row.cells[1].victory);
        }
    }

    // The first sector, by contrast, has to fall to almost nothing.
    CHECK(m.rows[0].cells[0].victory);
}

TEST_CASE("more Scrap never buys a worse result") {
    // Monotonicity is the property that makes the matrix readable at all. If
    // a bigger budget could do worse, the grid would be noise and no
    // conclusion drawn from it would hold.
    const int budgets[4] = {0, 1500, 5000, 80000};
    const ls::MatrixReport m = ls::runMatrix(budgets, 4);
    for (const ls::MatrixRow& row : m.rows) {
        for (int b = 1; b < row.count; ++b) {
            CAPTURE(row.level);
            CAPTURE(b);
            const ls::MatrixCell& lo = row.cells[static_cast<size_t>(b - 1)];
            const ls::MatrixCell& hi = row.cells[static_cast<size_t>(b)];
            if (lo.victory) CHECK(hi.victory);
            else CHECK(hi.kills >= lo.kills);
        }
    }
}
