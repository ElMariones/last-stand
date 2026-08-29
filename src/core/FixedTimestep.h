#pragma once
#include <cstdint>

namespace ls {

// Decouples simulation rate from frame rate. The renderer interpolates
// between the previous and current tick using alpha().
class FixedTimestep {
public:
    explicit FixedTimestep(double tickRate = 60.0, double maxFrameSeconds = 0.25);

    // Feed the real elapsed frame time. Returns how many fixed ticks to run.
    int advance(double frameSeconds);

    double   alpha() const;          // [0,1) progress toward the next tick
    double   tickSeconds() const { return tickSeconds_; }
    uint64_t totalTicks() const { return totalTicks_; }

private:
    double   tickSeconds_;
    double   maxFrameSeconds_;
    double   accumulator_ = 0.0;
    uint64_t totalTicks_  = 0u;
};

}  // namespace ls
