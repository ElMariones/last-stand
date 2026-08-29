#include "core/FixedTimestep.h"

namespace ls {

FixedTimestep::FixedTimestep(double tickRate, double maxFrameSeconds)
    : tickSeconds_(1.0 / tickRate), maxFrameSeconds_(maxFrameSeconds) {}

int FixedTimestep::advance(double frameSeconds) {
    if (frameSeconds < 0.0) frameSeconds = 0.0;
    // Clamping is what prevents the spiral of death: if a frame stalls,
    // we drop simulation time rather than queue an unbounded tick backlog
    // that guarantees the next frame stalls worse.
    if (frameSeconds > maxFrameSeconds_) frameSeconds = maxFrameSeconds_;

    accumulator_ += frameSeconds;

    int ticks = 0;
    while (accumulator_ >= tickSeconds_) {
        accumulator_ -= tickSeconds_;
        ++ticks;
        ++totalTicks_;
    }
    return ticks;
}

double FixedTimestep::alpha() const {
    return accumulator_ / tickSeconds_;
}

}  // namespace ls
