#include "gameplay/UpgradeTree.h"

#include <cmath>

namespace ls {

namespace {

// Base costs per node, indexed by NodeId.
constexpr std::array<uint32_t, kNodeCount> kBaseCost = {
    40u, 40u, 35u, 50u, 60u, 45u};

// Multiplicative factors per node.
constexpr std::array<float, kNodeCount> kMult = {
    1.20f, 1.15f, 1.12f, 0.0f, 0.0f, 1.20f};

// Additive bonuses per node (used when the mult factor is 0).
constexpr std::array<float, kNodeCount> kAdd = {
    0.0f, 0.0f, 0.0f, 300.0f, 2.0f, 0.0f};

constexpr double kCostGrowth = 1.35;

size_t idx(NodeId n) { return static_cast<size_t>(n); }

}  // namespace

uint32_t UpgradeTree::level(NodeId node) const {
    return levels_[idx(node)];
}

uint32_t UpgradeTree::cost(NodeId node) const {
    const uint32_t n = levels_[idx(node)];
    const double base = static_cast<double>(kBaseCost[idx(node)]);
    return static_cast<uint32_t>(
        std::llround(base * std::pow(kCostGrowth, static_cast<double>(n))));
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

bool UpgradeTree::canAfford(NodeId node, uint32_t scrap) const {
    return scrap >= cost(node);
}

bool UpgradeTree::purchase(NodeId node, uint32_t& scrap) {
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

Bonuses UpgradeTree::bonuses() const {
    Bonuses b;
    b.damageMult = static_cast<float>(
        std::pow(kMult[idx(NodeId::Damage)], static_cast<double>(levels_[0])));
    b.fireRateMult = static_cast<float>(
        std::pow(kMult[idx(NodeId::FireRate)], static_cast<double>(levels_[1])));
    b.rangeMult = static_cast<float>(
        std::pow(kMult[idx(NodeId::Range)], static_cast<double>(levels_[2])));
    b.baseBonusHp = kAdd[idx(NodeId::BaseHp)] * static_cast<float>(levels_[3]);
    b.baseRegen = kAdd[idx(NodeId::BaseRegen)] * static_cast<float>(levels_[4]);
    b.scrapMult = static_cast<float>(
        std::pow(kMult[idx(NodeId::Economy)], static_cast<double>(levels_[5])));
    return b;
}

}  // namespace ls
