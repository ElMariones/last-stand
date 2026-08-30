#include "app/Bench.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

#include "gameplay/Level.h"
#include "render/Lod.h"
#include "gameplay/SpawnDirector.h"
#include "sim/World.h"

namespace ls {

namespace {

// The entity ladder every milestone is measured on. Fixed so the CSVs from
// different milestones plot against each other.
constexpr uint32_t kSweepCounts[] = {100u, 500u, 1000u, 2000u, 5000u};
constexpr uint64_t kSweepTicks = 1200u;

// A synthetic level that emits everything at t=0: the benchmark wants to
// measure steady-state per-tick cost at a given entity count, not the shape of
// a real spawn curve. Victory sits far beyond the tick budget, so the run
// measures peak-density ticks.
Level makeBenchLevel(uint32_t count) {
    Level level;
    level.name = "bench";
    level.schedule = {SpawnEvent{0.0f, count}};
    level.totalEnemies = count;
    level.map = makeM1Map();
    return level;
}

bool fileExists(const char* path) {
    std::FILE* f = std::fopen(path, "r");
    if (f == nullptr) return false;
    std::fclose(f);
    return true;
}

}  // namespace

BenchResult runBench(const Options& options) {
    const Level level = makeBenchLevel(options.spawn);
    World world{level.map, options.seed};
    world.setNaiveSeparation(options.naiveSeparation);
    for (const Vec2& p : defaultDeployPositions(level.map, 4)) {
        world.placeTurret(p);
    }
    world.setLevelTotal(level.totalEnemies);

    // An indestructible base. World::tick short-circuits once a battle is
    // over, so a base that falls mid-run turns the remaining samples into
    // near-zero no-ops and quietly deflates the mean. A benchmark measures
    // per-tick simulation cost, not who wins.
    world.base().maxHealth = 1.0e9f;
    world.base().health = world.base().maxHealth;
    SpawnDirector director;

    BenchResult r;
    r.peakEntities = 0u;

    std::vector<double> samples;
    samples.reserve(static_cast<size_t>(options.ticks));

    const float dt = 1.0f / 60.0f;
    for (uint64_t i = 0; i < options.ticks; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        director.update(world, level, dt);
        world.tick(dt);
        const auto t1 = std::chrono::steady_clock::now();
        samples.push_back(
            std::chrono::duration<double, std::milli>(t1 - t0).count());
        r.peakEntities = std::max(r.peakEntities, world.enemies().count());
    }

    r.ticks = world.ticks();
    r.arrived = world.totalArrived();
    r.stateHash = world.stateHash();

    // The window is 1280x720 and the world is drawn 1:1 into it.
    const LodCensus census =
        lodCensus(world.enemies(), world.hash(),
                  viewportRect(1280.0f, 720.0f, 24.0f));
    r.lodTier[0] = census.tier[0];
    r.lodTier[1] = census.tier[1];
    r.lodTier[2] = census.tier[2];
    r.lodTriangles = census.triangles;
    r.lodDrawn = census.drawn;

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
    std::printf("lod_tiers      full %u  silhouette %u  shape %u\n",
                r.lodTier[0], r.lodTier[1], r.lodTier[2]);
    // What the horde costs to submit, against the two paths that do not
    // choose: articulate everyone, or articulate no one.
    const uint32_t allFull =
        r.lodDrawn * static_cast<uint32_t>(trianglesForTier(LodTier::Full));
    std::printf("lod_triangles  %u   (all-full %u, all-shape %u)\n",
                r.lodTriangles, allFull, r.lodDrawn);
}

bool runSweep(const Options& options) {
    if (options.sweepPath == nullptr) return false;

    const bool needHeader = !fileExists(options.sweepPath);
    std::FILE* f = std::fopen(options.sweepPath, "a");
    if (f == nullptr) return false;
    if (needHeader) {
        std::fprintf(f, "stage,peak_entities,ticks,tick_mean_ms,tick_p99_ms,notes\n");
    }

    for (const uint32_t count : kSweepCounts) {
        Options run = options;
        run.spawn = count;
        run.ticks = kSweepTicks;
        const BenchResult r = runBench(run);

        std::fprintf(f, "%s,%u,%llu,%.6f,%.6f,%s\n", options.stage,
                     r.peakEntities,
                     static_cast<unsigned long long>(r.ticks), r.meanMs,
                     r.p99Ms, options.notes);
        std::printf("%-8s %6u entities   mean %8.4f ms   p99 %8.4f ms\n",
                    options.stage, r.peakEntities, r.meanMs, r.p99Ms);
        std::fflush(f);
    }

    std::fclose(f);
    return true;
}

}  // namespace ls
