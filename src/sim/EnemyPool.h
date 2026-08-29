#pragma once
#include <cstdint>
#include <vector>

#include "math/Vec2.h"
#include "sim/EnemyType.h"

namespace ls {

// Fixed-capacity struct-of-arrays storage (GDD 14.3). Hot fields (position,
// velocity, health) are separate arrays so movement touches only what it needs
// and walks contiguous memory. Deliberately NOT a generic ECS.
//
// Arrays are sized once at construction and never resized, which is what
// gives us the "no heap allocation inside a tick" invariant.
class EnemyPool {
public:
    static constexpr uint32_t kCapacity = 100'000u;
    static constexpr uint32_t kInvalid  = 0xFFFFFFFFu;

    EnemyPool();

    uint32_t spawn(Vec2 pos, EnemyType type);   // hp/speed looked up from type
    void     kill(uint32_t i);       // swap-remove; O(1), does not preserve order
    void     clear();

    uint32_t count() const { return count_; }

    // Public by design: systems iterate these directly in tight loops.
    std::vector<Vec2>    position;
    std::vector<Vec2>    prevPosition;
    std::vector<Vec2>    velocity;
    std::vector<float>   health;
    std::vector<uint8_t> type;
    std::vector<float>   speed;

    // Burn (Flamethrower DoT): burnDps is the summed stacking intensity,
    // burnTtl the remaining seconds. Zero for non-burning enemies.
    std::vector<float>   burnDps;
    std::vector<float>   burnTtl;

private:
    uint32_t count_ = 0u;
};

}  // namespace ls
