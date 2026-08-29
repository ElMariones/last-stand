#pragma once
#include <array>
#include <cstdint>

namespace ls {

enum class NodeId : uint8_t { Damage, FireRate, Range, BaseHp, BaseRegen, Economy };
constexpr size_t kNodeCount = 6u;

// The folded effect of every purchased node, ready to be applied to a fresh
// battle's turrets and base. Multiplicative factors start at 1; additive
// bonuses start at 0.
struct Bonuses {
    float damageMult   = 1.0f;
    float fireRateMult = 1.0f;
    float rangeMult    = 1.0f;
    float baseBonusHp  = 0.0f;
    float baseRegen    = 0.0f;
    float scrapMult    = 1.0f;
};

// Persistent, branching-free upgrade tree (GDD 7). M3 ships six repeatable
// stat nodes to prove the loop; behaviour-changing nodes are M4. Cost follows
// cost(n) = base * 1.35^n where n is the node's current level (GDD 7.2).
// Respec is free and refunds 100% (GDD 7.4).
class UpgradeTree {
public:
    uint32_t level(NodeId node) const;
    uint32_t cost(NodeId node) const;               // price of the NEXT level
    bool     canAfford(NodeId node, uint32_t scrap) const;
    bool     purchase(NodeId node, uint32_t& scrap);
    void     respecAll(uint32_t& scrap);
    uint32_t totalSpent() const { return totalSpent_; }

    // Restores levels from a save and recomputes totalSpent (so a load can
    // reproduce the wallet exactly without re-running purchases).
    void loadLevels(const std::array<uint32_t, kNodeCount>& levels);

    Bonuses bonuses() const;

    const std::array<uint32_t, kNodeCount>& levels() const { return levels_; }

private:
    std::array<uint32_t, kNodeCount> levels_{};
    uint32_t totalSpent_ = 0u;
};

}  // namespace ls
