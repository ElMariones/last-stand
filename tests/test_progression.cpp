#include <doctest/doctest.h>
#include "gameplay/Progression.h"

using ls::BattleResult;
using ls::computePayout;

namespace {
BattleResult result(bool victory, uint32_t kills, uint32_t total,
                    uint32_t best = 0u, uint32_t clears = 0u) {
    return BattleResult{victory, kills, total, best, clears};
}
}  // namespace

TEST_CASE("the squared depth term rewards deeper progress") {
    const ls::Payout full = computePayout(result(true, 100u, 100u), 3.0f, 100.0f, 1.0f);
    const ls::Payout near = computePayout(result(true, 90u, 100u), 3.0f, 100.0f, 1.0f);
    // full: depth = 100 * 1.0 = 100 ; near: 100 * 0.81 = 81
    CHECK(full.depthScrap == 100u);
    CHECK(near.depthScrap == 81u);
    CHECK(full.scrap > near.scrap);
}

TEST_CASE("a defeat pays 75% of the identical victory") {
    const ls::Payout win = computePayout(result(true, 50u, 100u), 3.0f, 100.0f, 1.0f);
    const ls::Payout loss = computePayout(result(false, 50u, 100u), 3.0f, 100.0f, 1.0f);
    // base = 150 + 25 = 175 ; defeat = round(175 * 0.75) = 131
    CHECK(win.killScrap == 150u);
    CHECK(win.depthScrap == 25u);
    CHECK(loss.multiplier == doctest::Approx(0.75f));
    CHECK(loss.scrap == 131u);
}

TEST_CASE("a 0-kill defeat pays nothing but still reports newBest correctly") {
    const ls::Payout p = computePayout(result(false, 0u, 100u), 3.0f, 100.0f, 1.0f);
    CHECK(p.scrap == 0u);
    CHECK(p.killScrap == 0u);
    CHECK(p.depthScrap == 0u);
}

TEST_CASE("diminishing replays floor at 0.25") {
    CHECK(computePayout(result(true, 100u, 100u), 3.0f, 0.0f, 1.0f).multiplier ==
          doctest::Approx(1.0f));   // first clear
    CHECK(computePayout(result(true, 100u, 100u, 0u, 1u), 3.0f, 0.0f, 1.0f).multiplier ==
          doctest::Approx(0.7f));
    CHECK(computePayout(result(true, 100u, 100u, 0u, 4u), 3.0f, 0.0f, 1.0f).multiplier ==
          doctest::Approx(0.25f));
    CHECK(computePayout(result(true, 100u, 100u, 0u, 99u), 3.0f, 0.0f, 1.0f).multiplier ==
          doctest::Approx(0.25f));   // floor holds past the 5th
}

TEST_CASE("personal-best bonus fires on a new best, even on a loss") {
    const ls::Payout lose = computePayout(result(false, 120u, 200u, 100u), 3.0f, 0.0f, 1.0f);
    CHECK(lose.newBest == true);
    CHECK(lose.bestBonus == 50u);

    const ls::Payout worse = computePayout(result(false, 80u, 200u, 100u), 3.0f, 0.0f, 1.0f);
    CHECK(worse.newBest == false);
    CHECK(worse.bestBonus == 0u);
}

TEST_CASE("the first run offers no best bonus") {
    const ls::Payout p = computePayout(result(true, 100u, 100u, 0u), 3.0f, 100.0f, 1.0f);
    CHECK(p.newBest == true);
    CHECK(p.bestBonus == 0u);   // nothing to beat on a first run
}

TEST_CASE("economy multiplier scales the kill term only") {
    const ls::Payout base = computePayout(result(true, 100u, 100u), 3.0f, 0.0f, 1.0f);
    const ls::Payout eco = computePayout(result(true, 100u, 100u), 3.0f, 0.0f, 1.2f);
    CHECK(eco.killScrap ==
          doctest::Approx(static_cast<float>(base.killScrap) * 1.2f).epsilon(0.001));
    CHECK(eco.depthScrap == base.depthScrap);   // depth term is unscaled
}
