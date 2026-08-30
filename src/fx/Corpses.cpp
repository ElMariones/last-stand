#include "fx/Corpses.h"

namespace ls {

CorpseRing::CorpseRing() {
    const size_t cap = static_cast<size_t>(kCapacity);
    position_.resize(cap);
    direction_.resize(cap);
    ttl_.resize(cap, 0.0f);
    type_.resize(cap, 0u);
}

void CorpseRing::add(Vec2 pos, Vec2 dir, uint8_t type) {
    uint32_t slot = 0u;
    if (count_ < kCapacity) {
        slot = (head_ + count_) % kCapacity;
        ++count_;
    } else {
        // Full: the oldest corpse is the one that gets forgotten.
        slot = head_;
        head_ = (head_ + 1u) % kCapacity;
    }
    position_[slot] = pos;
    direction_[slot] = dir;
    ttl_[slot] = kFadeSeconds;
    type_[slot] = type;
}

void CorpseRing::update(float dt) {
    for (uint32_t s = 0; s < count_; ++s) {
        const uint32_t i = (head_ + s) % kCapacity;
        ttl_[i] -= dt;
    }
    // Uniform lifetimes mean the expired ones are always at the front.
    while (count_ > 0u && ttl_[head_] <= 0.0f) {
        head_ = (head_ + 1u) % kCapacity;
        --count_;
    }
}

void CorpseRing::clear() {
    head_ = 0u;
    count_ = 0u;
}

}  // namespace ls
