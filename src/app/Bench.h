#pragma once
#include <cstdint>

#include "app/Cli.h"

namespace ls {

struct BenchResult {
    uint64_t ticks         = 0u;
    uint32_t peakEntities  = 0u;
    uint32_t arrived       = 0u;
    double   minMs         = 0.0;
    double   meanMs        = 0.0;
    double   p99Ms         = 0.0;
    uint64_t stateHash     = 0u;
};

BenchResult runBench(const Options& options);
void        printBench(const BenchResult& r);

// Runs the standard entity ladder (100 .. 5000) and appends one row per rung
// to `path`, writing the header first if the file does not exist yet. The
// committed docs/bench/*.csv files are exactly this output, which is what
// makes the optimisation curve an artifact instead of a claim (GDD 14.7).
bool runSweep(const Options& options);

}  // namespace ls
