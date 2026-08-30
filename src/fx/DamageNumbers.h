#pragma once
#include <cstdint>
#include <vector>

#include "math/Vec2.h"

namespace ls {

// GDD 12.3: "aggregated above a threshold; never 10,000 individual popups".
// A new number merges into any live number close enough on screen instead of
// stacking a second label on top of it, so a cannon shell that hits forty
// enemies reads as one big number rather than forty small ones fighting for
// the same pixels.
class DamageNumbers {
public:
    static constexpr uint32_t kCapacity    = 48u;
    static constexpr float    kMergeRadius = 44.0f;
    static constexpr float    kLifetime    = 0.9f;
    static constexpr float    kRise        = 34.0f;   // pixels over its life

    DamageNumbers();

    void add(Vec2 position, float amount);
    void update(float dt);
    void clear() { count_ = 0u; }

    uint32_t count() const { return count_; }
    Vec2     positionAt(uint32_t i) const;
    float    amountAt(uint32_t i) const { return amount_[i]; }
    // 0 fresh, 1 gone.
    float    progressAt(uint32_t i) const {
        return 1.0f - (ttl_[i] / kLifetime);
    }

private:
    int  findMergeTarget(Vec2 p) const;
    void kill(uint32_t i);

    std::vector<Vec2>  anchor_;
    std::vector<float> amount_;
    std::vector<float> ttl_;
    uint32_t count_ = 0u;
};

}  // namespace ls
