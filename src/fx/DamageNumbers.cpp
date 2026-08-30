#include "fx/DamageNumbers.h"

namespace ls {

DamageNumbers::DamageNumbers() {
    const size_t cap = static_cast<size_t>(kCapacity);
    anchor_.resize(cap);
    amount_.resize(cap, 0.0f);
    ttl_.resize(cap, 0.0f);
}

int DamageNumbers::findMergeTarget(Vec2 p) const {
    int best = -1;
    float bestD = kMergeRadius * kMergeRadius;
    for (uint32_t i = 0; i < count_; ++i) {
        const float d = distanceSq(anchor_[i], p);
        if (d < bestD) {
            bestD = d;
            best = static_cast<int>(i);
        }
    }
    return best;
}

void DamageNumbers::add(Vec2 p, float amount) {
    if (amount <= 0.0f) return;

    const int merge = findMergeTarget(p);
    if (merge >= 0) {
        const size_t i = static_cast<size_t>(merge);
        amount_[i] += amount;
        // Refresh, so a sustained stream reads as one growing number rather
        // than a flicker of new ones.
        ttl_[i] = kLifetime;
        return;
    }

    if (count_ >= kCapacity) {
        // Full and nothing near enough to merge with: fold into the oldest
        // rather than dropping the damage on the floor.
        uint32_t oldest = 0u;
        for (uint32_t i = 1; i < count_; ++i) {
            if (ttl_[i] < ttl_[oldest]) oldest = i;
        }
        amount_[oldest] += amount;
        return;
    }

    const size_t i = static_cast<size_t>(count_++);
    anchor_[i] = p;
    amount_[i] = amount;
    ttl_[i] = kLifetime;
}

void DamageNumbers::update(float dt) {
    for (uint32_t i = count_; i-- > 0u;) {
        ttl_[i] -= dt;
        if (ttl_[i] <= 0.0f) kill(i);
    }
}

void DamageNumbers::kill(uint32_t i) {
    if (i >= count_) return;
    const uint32_t last = count_ - 1u;
    if (i != last) {
        anchor_[i] = anchor_[last];
        amount_[i] = amount_[last];
        ttl_[i] = ttl_[last];
    }
    count_ = last;
}

Vec2 DamageNumbers::positionAt(uint32_t i) const {
    // Floats upward as it ages; the anchor itself never moves, so merging
    // stays stable.
    return Vec2{anchor_[i].x, anchor_[i].y - progressAt(i) * kRise};
}

}  // namespace ls
