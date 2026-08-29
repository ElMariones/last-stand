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
};

Options     parseArgs(int argc, const char* const* argv);
const char* usageText();

}  // namespace ls
