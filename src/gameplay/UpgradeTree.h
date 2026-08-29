#pragma once
#include <array>
#include <cstdint>

namespace ls {

// 24 nodes: eight repeatable stat nodes + sixteen one-shot behaviour/unlock
// nodes (the transformation lines and abilities). See docs/GDD.md 7 and the
// M4 plan. Order is significant only for serialization (persist/SaveGame).
enum class NodeId : uint8_t {
    Damage = 0,
    FireRate,
    Range,
    BaseHp,
    BaseRegen,
    Economy,
    Splash,
    Burn,
    UnlockCannon,
    UnlockFlamethrower,
    ExtraHardpoint,
    TargetingDensest,
    MGOverclock,
    MGRicochet,
    MGBulletStorm,
    CannonExplosive,
    CannonKnockback,
    CannonCluster,
    FlameLingering,
    FlameIgnite,
    FlameFirestorm,
    AbilityAirstrike,
    AbilityOvercharge,
    ArmorPiercing,
};

constexpr size_t kNodeCount = 24u;

// The folded effect of every purchased node, ready to be applied to a fresh
// battle's turrets and base. Multiplicative factors start at 1; additive
// bonuses start at 0; booleans are false until their one-shot node is owned.
struct Effects {
    float damageMult   = 1.0f;
    float fireRateMult = 1.0f;
    float rangeMult    = 1.0f;
    float splashMult   = 1.0f;
    float burnMult     = 1.0f;
    float scrapMult    = 1.0f;
    float baseBonusHp  = 0.0f;
    float baseRegen    = 0.0f;

    bool unlockCannon     = false;
    bool unlockFlamethrower = false;
    bool extraHardpoint   = false;
    bool densest          = false;

    bool mgOverclock      = false;
    bool mgRicochet       = false;
    bool mgBulletStorm    = false;

    bool cannonExplosive  = false;
    bool cannonKnockback  = false;
    bool cannonCluster    = false;

    bool flameLingering   = false;
    bool flameIgnite      = false;
    bool flameFirestorm   = false;

    bool airstrike        = false;
    bool overcharge       = false;
    bool armorPiercing    = false;
};

bool isRepeatable(NodeId node);

class UpgradeTree {
public:
    uint32_t level(NodeId node) const;
    uint32_t cost(NodeId node) const;               // price of the NEXT level
    bool     canAfford(NodeId node, uint32_t scrap) const;
    bool     purchase(NodeId node, uint32_t& scrap);
    bool     has(NodeId node) const {
        return levels_[static_cast<size_t>(node)] > 0u;
    }
    void     respecAll(uint32_t& scrap);
    uint32_t totalSpent() const { return totalSpent_; }

    void loadLevels(const std::array<uint32_t, kNodeCount>& levels);

    Effects bonuses() const;

    const std::array<uint32_t, kNodeCount>& levels() const { return levels_; }

private:
    std::array<uint32_t, kNodeCount> levels_{};
    uint32_t totalSpent_ = 0u;
};

}  // namespace ls
