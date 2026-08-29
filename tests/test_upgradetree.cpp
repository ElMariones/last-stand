#include <doctest/doctest.h>
#include "gameplay/UpgradeTree.h"

using ls::NodeId;
using ls::UpgradeTree;

TEST_CASE("cost follows base * 1.35^n") {
    UpgradeTree t;
    CHECK(t.cost(NodeId::Damage) == 40u);     // 40 * 1.35^0
    CHECK(t.cost(NodeId::BaseRegen) == 60u);
    CHECK(t.cost(NodeId::Economy) == 45u);

    uint32_t scrap = 100000u;
    t.purchase(NodeId::Damage, scrap);
    CHECK(t.cost(NodeId::Damage) == 54u);     // 40 * 1.35^1
    t.purchase(NodeId::Damage, scrap);
    CHECK(t.cost(NodeId::Damage) == 73u);     // 40 * 1.35^2 (72.9 rounds to 73)
    t.purchase(NodeId::Damage, scrap);
    CHECK(t.cost(NodeId::Damage) == 98u);     // 40 * 1.35^3 (98.415 rounds to 98)
}

TEST_CASE("purchase deducts scrap and increments level") {
    UpgradeTree t;
    uint32_t scrap = 100u;
    CHECK(t.purchase(NodeId::Damage, scrap));
    CHECK(scrap == 60u);
    CHECK(t.level(NodeId::Damage) == 1u);
    CHECK(t.totalSpent() == 40u);
}

TEST_CASE("an unaffordable purchase leaves scrap untouched") {
    UpgradeTree t;
    uint32_t scrap = 30u;
    CHECK_FALSE(t.purchase(NodeId::Damage, scrap));   // cost 40
    CHECK(scrap == 30u);
    CHECK(t.level(NodeId::Damage) == 0u);
    CHECK(t.totalSpent() == 0u);
}

TEST_CASE("respecAll refunds everything and zeroes the tree") {
    UpgradeTree t;
    uint32_t scrap = 1000u;
    t.purchase(NodeId::Damage, scrap);
    t.purchase(NodeId::BaseHp, scrap);
    t.purchase(NodeId::Damage, scrap);   // 40 + 54 = 94 spent on Damage

    const uint32_t spent = t.totalSpent();
    REQUIRE(spent > 0u);

    t.respecAll(scrap);
    CHECK(t.totalSpent() == 0u);
    CHECK(t.level(NodeId::Damage) == 0u);
    CHECK(t.level(NodeId::BaseHp) == 0u);
    CHECK(scrap == 1000u);   // nothing lost
}

TEST_CASE("bonuses are identity at zero levels") {
    UpgradeTree t;
    const ls::Effects b = t.bonuses();
    CHECK(b.damageMult == doctest::Approx(1.0f));
    CHECK(b.fireRateMult == doctest::Approx(1.0f));
    CHECK(b.rangeMult == doctest::Approx(1.0f));
    CHECK(b.baseBonusHp == doctest::Approx(0.0f));
    CHECK(b.baseRegen == doctest::Approx(0.0f));
    CHECK(b.scrapMult == doctest::Approx(1.0f));
}

TEST_CASE("bonuses fold levels correctly") {
    UpgradeTree t;
    uint32_t scrap = 100000u;
    t.purchase(NodeId::Damage, scrap);
    t.purchase(NodeId::Damage, scrap);
    t.purchase(NodeId::Damage, scrap);      // 3 levels
    t.purchase(NodeId::BaseRegen, scrap);   // 1 level
    t.purchase(NodeId::BaseHp, scrap);      // 1 level

    const ls::Effects b = t.bonuses();
    CHECK(b.damageMult == doctest::Approx(1.2f * 1.2f * 1.2f).epsilon(0.001));
    CHECK(b.baseRegen == doctest::Approx(2.0f));
    CHECK(b.baseBonusHp == doctest::Approx(300.0f));
    CHECK(b.scrapMult == doctest::Approx(1.0f));   // economy untouched
}

TEST_CASE("one-shot nodes can be bought exactly once") {
    UpgradeTree t;
    uint32_t scrap = 100000u;
    CHECK(t.purchase(NodeId::UnlockCannon, scrap));
    CHECK(t.has(NodeId::UnlockCannon));
    CHECK_FALSE(t.purchase(NodeId::UnlockCannon, scrap));   // no stacking
    CHECK(t.level(NodeId::UnlockCannon) == 1u);
}

TEST_CASE("unlock and transformation nodes surface in effects") {
    UpgradeTree t;
    uint32_t scrap = 100000u;
    t.purchase(NodeId::UnlockFlamethrower, scrap);
    t.purchase(NodeId::MGOverclock, scrap);
    t.purchase(NodeId::FlameIgnite, scrap);
    t.purchase(NodeId::ArmorPiercing, scrap);

    const ls::Effects e = t.bonuses();
    CHECK(e.unlockFlamethrower);
    CHECK(e.mgOverclock);
    CHECK(e.flameIgnite);
    CHECK(e.armorPiercing);
    CHECK_FALSE(e.unlockCannon);
    CHECK_FALSE(e.airstrike);
}

TEST_CASE("the tree has exactly 24 nodes with ~13+ behaviour nodes") {
    CHECK(ls::kNodeCount == 24u);
    // Stat nodes are the repeatable ones; the rest are behaviour/unlock.
    size_t repeatable = 0u;
    for (size_t i = 0; i < ls::kNodeCount; ++i) {
        if (ls::isRepeatable(static_cast<ls::NodeId>(i))) ++repeatable;
    }
    CHECK(repeatable == 8u);
    CHECK(ls::kNodeCount - repeatable == 16u);   // behaviour/unlock nodes
}
