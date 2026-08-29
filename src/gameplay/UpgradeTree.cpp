#include "gameplay/UpgradeTree.h"

#include <cmath>

namespace ls {

namespace {

constexpr double kCostGrowth = 1.35;

size_t idx(NodeId n) { return static_cast<size_t>(n); }

constexpr std::array<uint32_t, kNodeCount> kBaseCost = {
    40u,   // Damage
    40u,   // FireRate
    35u,   // Range
    50u,   // BaseHp
    60u,   // BaseRegen
    45u,   // Economy
    50u,   // Splash
    45u,   // Burn
    120u,  // UnlockCannon
    120u,  // UnlockFlamethrower
    100u,  // ExtraHardpoint
    40u,   // TargetingDensest
    120u,  // MGOverclock
    150u,  // MGRicochet
    150u,  // MGBulletStorm
    130u,  // CannonExplosive
    130u,  // CannonKnockback
    150u,  // CannonCluster
    120u,  // FlameLingering
    150u,  // FlameIgnite
    160u,  // FlameFirestorm
    140u,  // AbilityAirstrike
    120u,  // AbilityOvercharge
    80u,   // ArmorPiercing
};

// Stat scalar factors (repeatable nodes). Additive nodes use a factor of 0
// and carry their add value in the bonuses() switch.
constexpr std::array<float, kNodeCount> kMult = {
    1.20f, 1.15f, 1.12f, 0.0f, 0.0f, 1.20f, 1.15f, 1.20f,
};

}  // namespace

bool isRepeatable(NodeId node) {
    switch (node) {
        case NodeId::Damage:
        case NodeId::FireRate:
        case NodeId::Range:
        case NodeId::BaseHp:
        case NodeId::BaseRegen:
        case NodeId::Economy:
        case NodeId::Splash:
        case NodeId::Burn:
            return true;
        default:
            return false;
    }
}

uint32_t UpgradeTree::level(NodeId node) const {
    return levels_[idx(node)];
}

uint32_t UpgradeTree::cost(NodeId node) const {
    const uint32_t n = levels_[idx(node)];
    const double base = static_cast<double>(kBaseCost[idx(node)]);
    return static_cast<uint32_t>(
        std::llround(base * std::pow(kCostGrowth, static_cast<double>(n))));
}

bool UpgradeTree::canAfford(NodeId node, uint32_t scrap) const {
    return scrap >= cost(node);
}

bool UpgradeTree::purchase(NodeId node, uint32_t& scrap) {
    if (!isRepeatable(node) && levels_[idx(node)] > 0u) return false;
    const uint32_t c = cost(node);
    if (scrap < c) return false;
    scrap -= c;
    totalSpent_ += c;
    ++levels_[idx(node)];
    return true;
}

void UpgradeTree::respecAll(uint32_t& scrap) {
    scrap += totalSpent_;
    totalSpent_ = 0u;
    levels_.fill(0u);
}

void UpgradeTree::loadLevels(const std::array<uint32_t, kNodeCount>& levels) {
    levels_ = levels;
    totalSpent_ = 0u;
    for (size_t n = 0; n < kNodeCount; ++n) {
        const double base = static_cast<double>(kBaseCost[n]);
        for (uint32_t lv = 0; lv < levels_[n]; ++lv) {
            totalSpent_ += static_cast<uint32_t>(
                std::llround(base * std::pow(kCostGrowth, static_cast<double>(lv))));
        }
    }
}

Effects UpgradeTree::bonuses() const {
    Effects e;

    e.damageMult = static_cast<float>(std::pow(
        kMult[idx(NodeId::Damage)], static_cast<double>(levels_[0])));
    e.fireRateMult = static_cast<float>(std::pow(
        kMult[idx(NodeId::FireRate)], static_cast<double>(levels_[1])));
    e.rangeMult = static_cast<float>(std::pow(
        kMult[idx(NodeId::Range)], static_cast<double>(levels_[2])));
    e.baseBonusHp = 300.0f * static_cast<float>(levels_[3]);
    e.baseRegen = 2.0f * static_cast<float>(levels_[4]);
    e.scrapMult = static_cast<float>(std::pow(
        kMult[idx(NodeId::Economy)], static_cast<double>(levels_[5])));
    e.splashMult = static_cast<float>(std::pow(
        kMult[idx(NodeId::Splash)], static_cast<double>(levels_[6])));
    e.burnMult = static_cast<float>(std::pow(
        kMult[idx(NodeId::Burn)], static_cast<double>(levels_[7])));

    e.unlockCannon = has(NodeId::UnlockCannon);
    e.unlockFlamethrower = has(NodeId::UnlockFlamethrower);
    e.extraHardpoint = has(NodeId::ExtraHardpoint);
    e.densest = has(NodeId::TargetingDensest);

    e.mgOverclock = has(NodeId::MGOverclock);
    e.mgRicochet = has(NodeId::MGRicochet);
    e.mgBulletStorm = has(NodeId::MGBulletStorm);

    e.cannonExplosive = has(NodeId::CannonExplosive);
    e.cannonKnockback = has(NodeId::CannonKnockback);
    e.cannonCluster = has(NodeId::CannonCluster);

    e.flameLingering = has(NodeId::FlameLingering);
    e.flameIgnite = has(NodeId::FlameIgnite);
    e.flameFirestorm = has(NodeId::FlameFirestorm);

    e.airstrike = has(NodeId::AbilityAirstrike);
    e.overcharge = has(NodeId::AbilityOvercharge);
    e.armorPiercing = has(NodeId::ArmorPiercing);

    return e;
}

}  // namespace ls
