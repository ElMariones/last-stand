#include "sim/EnemyPool.h"

namespace ls {

EnemyPool::EnemyPool() {
    const size_t cap = static_cast<size_t>(kCapacity);
    position.resize(cap);
    prevPosition.resize(cap);
    velocity.resize(cap);
    health.resize(cap, 0.0f);
    type.resize(cap, 0u);
}

uint32_t EnemyPool::spawn(Vec2 pos, float hp, uint8_t enemyType) {
    if (count_ >= kCapacity) return kInvalid;

    const uint32_t i = count_++;
    const size_t   s = static_cast<size_t>(i);

    position[s]     = pos;
    prevPosition[s] = pos;          // so the first interpolated frame is stable
    velocity[s]     = Vec2{0.0f, 0.0f};
    health[s]       = hp;
    type[s]         = enemyType;
    return i;
}

void EnemyPool::kill(uint32_t i) {
    if (i >= count_) return;

    const uint32_t last = count_ - 1u;
    if (i != last) {
        const size_t d = static_cast<size_t>(i);
        const size_t l = static_cast<size_t>(last);
        position[d]     = position[l];
        prevPosition[d] = prevPosition[l];
        velocity[d]     = velocity[l];
        health[d]       = health[l];
        type[d]         = type[l];
    }
    count_ = last;
}

void EnemyPool::clear() {
    count_ = 0u;
}

}  // namespace ls
