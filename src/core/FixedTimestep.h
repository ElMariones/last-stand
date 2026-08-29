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

    // Drops any pending fractional time (accumulator). Call when entering a
    // fresh simulation (e.g. a new battle) so a long paused frame doesn't
    // burst 15 ticks of catch-up and make the simulation lurch.
    void reset() { accumulator_ = 0.0; }

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
