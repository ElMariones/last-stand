#include <doctest/doctest.h>

#include "app/Balance.h"

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

TEST_CASE("all three sectors are reachable and clearable") {
    const ls::BalanceReport r = ls::runBalance(16);
    int deepest = 0;
    bool clearedDeepest = false;
    for (const ls::BalanceRun& run : r.runs) {
        if (run.level > deepest) deepest = run.level;
        if (run.level == 2 && run.victory) clearedDeepest = true;
    }
    CHECK(deepest == 2);
    CHECK(clearedDeepest);
}

TEST_CASE("replaying a cleared sector pays progressively less") {
    // GDD 8.4's anti-grind rule, observed end to end rather than unit-tested
    // on the formula.
    const ls::BalanceReport r = ls::runBalance(20);
    uint32_t firstClearPayout = 0u;
    uint32_t lastRepeatPayout = 0u;
    for (const ls::BalanceRun& run : r.runs) {
        if (run.level != 2 || !run.victory) continue;
        if (firstClearPayout == 0u) firstClearPayout = run.payout;
        lastRepeatPayout = run.payout;
    }
    REQUIRE(firstClearPayout > 0u);
    CHECK(lastRepeatPayout < firstClearPayout);
}
