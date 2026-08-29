#pragma once
#include <cstdint>

namespace ls {

// PCG32 (O'Neill, 2014). Chosen over std::mt19937 for a smaller state,
// better statistical quality per byte, and — critically — a specified,
// portable algorithm. std::mt19937's *distributions* are implementation
// defined, which would silently break determinism across toolchains.
class Pcg32 {
public:
    explicit Pcg32(uint64_t seed, uint64_t stream = 1u);

    uint32_t nextU32();
    float    nextFloat();                       // [0, 1)
    uint32_t nextBounded(uint32_t bound);       // [0, bound), unbiased
    float    nextRange(float lo, float hi);     // [lo, hi]

private:
    uint64_t state_;
    uint64_t inc_;
};

}  // namespace ls
