#pragma once
#include <cstdint>
#include <vector>

#include "math/Vec2.h"

namespace ls {

// The battlefield should visibly fill with the dead (GDD 12.3), which at four
// thousand kills a minute means a bounded structure or nothing. A ring buffer
// gives both: corpses accumulate, the oldest are overwritten once it is full,
// and the memory never moves. Entries are added with a uniform fade time, so
// insertion order IS expiry order and the ring stays a simple FIFO.
class CorpseRing {
public:
    static constexpr uint32_t kCapacity = 1024u;
    static constexpr float    kFadeSeconds = 2.0f;

    CorpseRing();

    void add(Vec2 position, Vec2 direction, uint8_t type);
    void update(float dt);
    void clear();

    uint32_t count() const { return count_; }

    // Live entries in age order, oldest first. `slot` is 0..count()-1.
    uint32_t indexAt(uint32_t slot) const {
        return (head_ + slot) % kCapacity;
    }
    // 0 when freshly dead, 1 when fully faded.
    float fade(uint32_t index) const {
        return 1.0f - (ttl_[index] / kFadeSeconds);
    }

    Vec2    positionAt(uint32_t index) const { return position_[index]; }
    Vec2    directionAt(uint32_t index) const { return direction_[index]; }
    uint8_t typeAt(uint32_t index) const { return type_[index]; }

private:
    std::vector<Vec2>    position_;
    std::vector<Vec2>    direction_;
    std::vector<float>   ttl_;
    std::vector<uint8_t> type_;

    uint32_t head_  = 0u;
    uint32_t count_ = 0u;
};

}  // namespace ls
