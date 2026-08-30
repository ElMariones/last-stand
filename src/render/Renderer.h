#pragma once
#include <cstdint>
#include <vector>

#include "fx/Corpses.h"
#include "fx/DamageNumbers.h"
#include "fx/Particles.h"
#include "render/Lod.h"
#include "render/Viewport.h"
#include "sim/World.h"

namespace ls {

struct DebugFlags {
    bool showFlowField  = false;
    bool showGrid       = false;
    bool showTurretRange = false;
};

// The two Stage 3 techniques, switchable so the benchmark can price each of
// them (`--no-lod`, `--no-batch`). Off, the renderer reproduces M4's path:
// one raylib DrawTriangle call per enemy, every enemy at Shape detail.
struct RenderSettings {
    bool lod     = true;
    bool batched = true;
};

// Everything the renderer draws that is not the simulation. Passed as a view
// of pools it does not own, so fx/ stays free of raylib and render/ stays
// free of app state.
struct FxScene {
    const ParticlePool*  particles = nullptr;
    const CorpseRing*    corpses   = nullptr;
    const DamageNumbers* numbers   = nullptr;
    Vec2                 shake{0.0f, 0.0f};
    bool                 showNumbers = true;
};

// What one draw() spent its time on. The render benchmark reports these; the
// point of LOD is that `triangles` stops tracking `enemies`.
struct RenderStats {
    uint32_t enemies   = 0u;   // live
    uint32_t drawn     = 0u;   // survived view culling
    uint32_t triangles = 0u;
    uint32_t batches   = 0u;   // rlBegin/rlEnd spans for the horde
    uint32_t tierCount[3] = {0u, 0u, 0u};
};

class Renderer {
public:
    // alpha is FixedTimestep::alpha() — the interpolation factor between
    // the previous and current tick.
    void draw(const World& world,
              const Viewport& viewport,
              float alpha,
              const DebugFlags& flags,
              const RenderSettings& settings = RenderSettings{},
              const FxScene& fx = FxScene{});

    // The developer overlay, drawn outside the shaken world so the numbers
    // stay readable while the screen is coming apart.
    void drawDebugOverlay(const World& world, double frameMs, double tickMs);

    void tickAnimationClock(float seconds) { animClock_ += seconds; }

    const RenderStats& stats() const { return stats_; }

private:
    void bucketEnemies(const World& world, float alpha, Rect view,
                       const RenderSettings& settings);
    void drawHorde(const World& world, float alpha,
                   const RenderSettings& settings);
    void drawTerrain(const World& world, const DebugFlags& flags);
    void drawCorpses(const FxScene& fx);
    void drawParticles(const FxScene& fx);
    void drawNumbers(const FxScene& fx);
    void drawTurrets(const World& world, const DebugFlags& flags);
    void drawVignette();

    // Per-tier index lists, reused across frames so drawing never allocates
    // once the pool has been seen at its peak size.
    std::vector<uint32_t> buckets_[3];
    RenderStats stats_;
    float       animClock_ = 0.0f;
};

}  // namespace ls
