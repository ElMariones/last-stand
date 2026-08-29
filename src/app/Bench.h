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
bool        writeBenchCsv(const BenchResult& r, const char* path);

}  // namespace ls
