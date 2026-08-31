#include <doctest/doctest.h>
#include "gameplay/Level.h"

using ls::Level;

TEST_CASE("Level 1 totals 100 grunts and is named") {
    const Level lvl = ls::makeLevel1();
    CHECK(lvl.name == "The Outskirts");
    CHECK(lvl.recommendedPower == 10u);
    CHECK(lvl.totalEnemies == 100u);
}

TEST_CASE("Level 1 schedule is non-empty, sorted, and starts at time 0") {
    const Level lvl = ls::makeLevel1();
    REQUIRE_FALSE(lvl.schedule.empty());
    CHECK(lvl.schedule.front().timeSeconds == doctest::Approx(0.0f));

    float last = -1.0f;
    uint32_t sum = 0u;
    for (const auto& e : lvl.schedule) {
        CHECK(e.timeSeconds > last);
        last = e.timeSeconds;
        sum += e.count;
    }
    CHECK(sum == lvl.totalEnemies);
}

TEST_CASE("Level 1 map has somewhere to defend from") {
    const Level lvl = ls::makeLevel1();
    CHECK(ls::defaultDeployPositions(lvl.map, 4).size() == 4u);
}

TEST_CASE("economy constants are non-negative and sane") {
    const Level lvl = ls::makeLevel1();
    CHECK(lvl.killValue > 0.0f);
    CHECK(lvl.depthBonusWeight > 0.0f);
}

TEST_CASE("Level 2 totals 250 and includes Runners") {
    const Level lvl = ls::makeLevel2();
    CHECK(lvl.name == "Refinery Gate");
    CHECK(lvl.recommendedPower == 26u);
    CHECK(lvl.totalEnemies == 250u);

    uint32_t sum = 0u;
    bool hasRunner = false;
    for (const auto& e : lvl.schedule) {
        sum += e.count;
        if (e.type == ls::EnemyType::Runner) hasRunner = true;
    }
    CHECK(sum == lvl.totalEnemies);
    CHECK(hasRunner);
}

TEST_CASE("Level 3 totals 320 and includes Runners") {
    const Level lvl = ls::makeLevel3();
    CHECK(lvl.name == "The Narrows");
    CHECK(lvl.totalEnemies == 320u);

    bool hasRunner = false;
    for (const auto& e : lvl.schedule) {
        if (e.type == ls::EnemyType::Runner) hasRunner = true;
    }
    CHECK(hasRunner);
}

TEST_CASE("every level schedule is sorted by time") {
    // SpawnDirector walks the schedule in order and stops at the first event
    // that is not yet due. An unsorted schedule (Levels 2 and 3 are assembled
    // from several overlapping waves) therefore held later waves back and then
    // dumped them all in one tick, wrecking the authored spawn curve.
    const Level levels[3] = {ls::makeLevel1(), ls::makeLevel2(),
                             ls::makeLevel3()};
    for (const Level& lvl : levels) {
        float last = -1.0f;
        for (const auto& e : lvl.schedule) {
            CHECK(e.timeSeconds >= last);
            last = e.timeSeconds;
        }
    }
}


// ----------------------------------------------------------- the campaign ---

TEST_CASE("every sector has a name, a map and an invasion") {
    for (int i = 0; i < ls::kLevelCount; ++i) {
        const Level lvl = ls::makeLevelByIndex(i);
        CAPTURE(i);
        CHECK(lvl.name == ls::levelName(i));
        CHECK(lvl.totalEnemies > 0u);
        CHECK_FALSE(lvl.schedule.empty());
        CHECK_FALSE(lvl.map.spawnCells.empty());
        CHECK(lvl.killValue > 0.0f);
        CHECK(lvl.enemyHealthMult >= 1.0f);
        // Somewhere to put a real defence, on every map.
        CHECK(ls::defaultDeployPositions(lvl.map, 8).size() == 8u);
    }
}

TEST_CASE("the campaign graph is a DAG that reaches every sector") {
    // A parent must sit on an earlier tier. That single rule is what makes
    // the graph acyclic, and a cycle here would be a sector that can never
    // unlock - invisible until a player got stuck on it.
    for (int i = 0; i < ls::kLevelCount; ++i) {
        int parents[ls::kMaxParents];
        const int n = ls::levelParents(i, parents);
        CAPTURE(i);
        if (ls::levelTier(i) == 0) {
            CHECK(n == 0);
            continue;
        }
        CHECK(n > 0);
        for (int p = 0; p < n; ++p) {
            CHECK(parents[p] >= 0);
            CHECK(parents[p] < ls::kLevelCount);
            CHECK(ls::levelTier(parents[p]) < ls::levelTier(i));
        }
    }

    // Every sector is reachable by clearing everything that comes before it.
    bool reachable[ls::kLevelCount] = {};
    for (int i = 0; i < ls::kLevelCount; ++i) {
        if (ls::levelTier(i) == 0) { reachable[i] = true; continue; }
        int parents[ls::kMaxParents];
        const int n = ls::levelParents(i, parents);
        for (int p = 0; p < n; ++p) {
            if (reachable[parents[p]]) reachable[i] = true;
        }
    }
    for (int i = 0; i < ls::kLevelCount; ++i) {
        CAPTURE(i);
        CHECK(reachable[i]);
    }
}

TEST_CASE("the tiers actually branch") {
    // The point of the graph is choice. If a tier held one sector it would be
    // a corridor with extra ceremony, which is what this replaced.
    int widest = 0;
    int total = 0;
    for (int t = 0; t < ls::kTierCount; ++t) {
        const int w = ls::tierWidth(t);
        CHECK(w > 0);
        widest = (w > widest) ? w : widest;
        total += w;
        for (int slot = 0; slot < w; ++slot) {
            CHECK(ls::levelTier(ls::levelAtTier(t, slot)) == t);
        }
    }
    CHECK(total == ls::kLevelCount);
    CHECK(widest >= 3);
    CHECK(widest <= ls::kMaxTierWidth);
    CHECK(ls::tierWidth(1) >= 3);   // the second difficulty band, specifically
}

TEST_CASE("difficulty rises with tier and payout per kill falls") {
    // The two halves of the economy fix, asserted rather than remembered:
    // later sectors are tougher, and they pay LESS per kill, because payout
    // used to be linear in enemy count while damage was exponential in Scrap.
    float prevHealth = 0.0f;
    float prevKillValue = 1e9f;
    for (int t = 0; t < ls::kTierCount; ++t) {
        float health = 0.0f;
        float killValue = 0.0f;
        const int w = ls::tierWidth(t);
        for (int slot = 0; slot < w; ++slot) {
            const Level lvl = ls::makeLevelByIndex(ls::levelAtTier(t, slot));
            health += lvl.enemyHealthMult;
            killValue += lvl.killValue;
        }
        health /= static_cast<float>(w);
        killValue /= static_cast<float>(w);
        CAPTURE(t);
        CHECK(health > prevHealth);
        CHECK(killValue < prevKillValue);
        prevHealth = health;
        prevKillValue = killValue;
    }
}

TEST_CASE("the campaign introduces each enemy kind somewhere") {
    bool seen[ls::kEnemyTypeCount] = {};
    for (int i = 0; i < ls::kLevelCount; ++i) {
        for (const auto& e : ls::makeLevelByIndex(i).schedule) {
            seen[static_cast<size_t>(e.type)] = true;
        }
    }
    for (size_t k = 0; k < ls::kEnemyTypeCount; ++k) {
        CAPTURE(k);
        CHECK(seen[k]);
    }
}

TEST_CASE("a new enemy kind is introduced on its own, not in a crowd") {
    // Meeting armour for the first time in a sector that also debuts weaving
    // and swarming teaches nothing. Each kind's first appearance is checked to
    // be the ONLY debut in that sector.
    int debutSector[ls::kEnemyTypeCount];
    for (size_t k = 0; k < ls::kEnemyTypeCount; ++k) {
        debutSector[k] = -1;
    }
    for (int i = 0; i < ls::kLevelCount; ++i) {
        for (const auto& e : ls::makeLevelByIndex(i).schedule) {
            const size_t k = static_cast<size_t>(e.type);
            if (debutSector[k] < 0) debutSector[k] = i;
        }
    }
    for (size_t a = 0; a < ls::kEnemyTypeCount; ++a) {
        for (size_t b = a + 1u; b < ls::kEnemyTypeCount; ++b) {
            if (debutSector[a] == 0 && debutSector[b] == 0) continue;
            CAPTURE(a);
            CAPTURE(b);
            CHECK(debutSector[a] != debutSector[b]);
        }
    }
}
