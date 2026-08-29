#pragma once
#include "sim/World.h"

namespace ls {

struct DebugFlags {
    bool showFlowField = false;
    bool showGrid      = false;
};

class Renderer {
public:
    // alpha is FixedTimestep::alpha() — the interpolation factor between
    // the previous and current tick.
    void draw(const World& world,
              float alpha,
              const DebugFlags& flags,
              double frameMs,
              double tickMs);
};

}  // namespace ls
