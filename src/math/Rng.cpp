#include "math/Rng.h"

namespace ls {

namespace {
constexpr uint64_t kMultiplier = 6364136223846793005ULL;
}

Pcg32::Pcg32(uint64_t seed, uint64_t stream)
    : state_(0u), inc_((stream << 1u) | 1u) {
    nextU32();
    state_ += seed;
    nextU32();
}

uint32_t Pcg32::nextU32() {
    const uint64_t old = state_;
    state_ = old * kMultiplier + inc_;
    const uint32_t xorshifted = static_cast<uint32_t>(((old >> 18u) ^ old) >> 27u);
    const uint32_t rot = static_cast<uint32_t>(old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((~rot + 1u) & 31u));
}

float Pcg32::nextFloat() {
    // 24 mantissa bits scaled by 2^-24 — exactly representable, never 1.0f.
    return static_cast<float>(nextU32() >> 8u) * 0x1.0p-24f;
}

uint32_t Pcg32::nextBounded(uint32_t bound) {
    if (bound == 0u) return 0u;
    // Rejection sampling removes modulo bias.
    const uint32_t threshold = (~bound + 1u) % bound;
    for (;;) {
        const uint32_t r = nextU32();
        if (r >= threshold) return r % bound;
    }
}

float Pcg32::nextRange(float lo, float hi) {
    return lo + (hi - lo) * nextFloat();
}

}  // namespace ls
