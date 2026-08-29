#include "app/Bench.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

#include "sim/World.h"

namespace ls {

BenchResult runBench(const Options& options) {
    World world{makeM1Map(), options.seed};
    world.spawnWave(options.spawn);

    BenchResult r;
    r.peakEntities = world.enemies().count();

    std::vector<double> samples;
    samples.reserve(static_cast<size_t>(options.ticks));

    const float dt = 1.0f / 60.0f;
    for (uint64_t i = 0; i < options.ticks; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        world.tick(dt);
        const auto t1 = std::chrono::steady_clock::now();
        samples.push_back(
            std::chrono::duration<double, std::milli>(t1 - t0).count());
    }

    r.ticks = world.ticks();
    r.arrived = world.totalArrived();
    r.stateHash = world.stateHash();

    if (!samples.empty()) {
        double sum = 0.0;
        for (const double s : samples) sum += s;
        r.meanMs = sum / static_cast<double>(samples.size());

        std::sort(samples.begin(), samples.end());
        r.minMs = samples.front();
        const size_t idx = static_cast<size_t>(
            static_cast<double>(samples.size() - 1) * 0.99);
        r.p99Ms = samples[idx];
    }
    return r;
}

void printBench(const BenchResult& r) {
    std::printf("ticks_run      %llu\n", static_cast<unsigned long long>(r.ticks));
    std::printf("peak_entities  %u\n", r.peakEntities);
    std::printf("arrived        %u\n", r.arrived);
    std::printf("tick_min_ms    %.6f\n", r.minMs);
    std::printf("tick_mean_ms   %.6f\n", r.meanMs);
    std::printf("tick_p99_ms    %.6f\n", r.p99Ms);
    std::printf("state_hash     %llu\n", static_cast<unsigned long long>(r.stateHash));
}

bool writeBenchCsv(const BenchResult& r, const char* path) {
    std::FILE* f = std::fopen(path, "w");
    if (f == nullptr) return false;
    std::fprintf(f, "stage,peak_entities,ticks,arrived,tick_min_ms,tick_mean_ms,tick_p99_ms\n");
    std::fprintf(f, "stage0,%u,%llu,%u,%.6f,%.6f,%.6f\n",
                 r.peakEntities,
                 static_cast<unsigned long long>(r.ticks),
                 r.arrived, r.minMs, r.meanMs, r.p99Ms);
    std::fclose(f);
    return true;
}

}  // namespace ls
