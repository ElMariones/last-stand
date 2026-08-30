#pragma once
#include <cstdint>

namespace ls {

struct Options {
    bool     bench     = false;
    bool     noRender  = false;
    bool     help      = false;
    uint64_t ticks     = 10000u;
    uint64_t seed      = 1234u;
    uint32_t spawn     = 100u;
    // >0: run the windowed app for this many ticks, write shot.png, exit.
    // Exists so the render path can be verified without a human at the
    // keyboard, and so README captures are reproducible.
    uint64_t shotTicks = 0u;

    // Benchmark sweep: run the standard entity ladder and append a row per
    // rung to `sweepPath`, tagged `stage` / `notes`. This is what produces
    // the docs/bench curve, so the optimisation story is a committed
    // artifact rather than a claim (GDD 14.7).
    const char* sweepPath = nullptr;
    const char* stage     = "stage0";
    const char* notes     = "";

    // Stage 0 separation is O(n^2) over every pair. Kept switchable, not
    // deleted, so the M5 baseline stays re-measurable on any machine.
    bool naiveSeparation = false;

    // >0: play the loop headlessly for this many runs with an auto-player and
    // print the progression curve. The economy's tuning instrument.
    int      balanceRuns  = 0;
    int      balanceLevel = 0;

    // >0: open a window, spawn this many enemies and time `renderTicks`
    // frames of the render path. The LOD numbers come from here.
    uint32_t renderBench  = 0u;
    uint64_t renderFrames = 600u;
    bool     noLod        = false;   // draw every enemy at Shape detail
    bool     noBatch      = false;   // one raylib draw call per enemy (M4 path)
};

Options     parseArgs(int argc, const char* const* argv);
const char* usageText();

}  // namespace ls
