#include "render/Renderer.h"

#include <cmath>
#include <cstdio>
#include <raylib.h>
#include <rlgl.h>

namespace ls {

namespace {

// Palette from GDD 12.1: cold player, warm world, sickly enemies.
constexpr Color kWall{46, 38, 34, 255};
constexpr Color kBaseGood{150, 220, 255, 255};
constexpr Color kBaseBad{255, 90, 70, 255};
constexpr Color kText{200, 220, 240, 255};
constexpr Color kFlow{70, 90, 110, 255};
constexpr Color kTurret{150, 220, 255, 255};
constexpr Color kHardpoint{90, 90, 96, 255};
constexpr Color kRange{70, 130, 160, 110};
constexpr Color kTracer{200, 235, 255, 255};
constexpr Color kOutline{18, 22, 18, 255};

constexpr float kTracerTtl = 0.08f;   // matches CombatSystem
constexpr float kCullMargin = 24.0f;  // widest body half-extent, rounded up

inline Vector2 toRl(Vec2 v) { return Vector2{v.x, v.y}; }

inline Color lerpColor(Color a, Color b, float t) {
    return Color{
        static_cast<unsigned char>(a.r + (b.r - a.r) * t),
        static_cast<unsigned char>(a.g + (b.g - a.g) * t),
        static_cast<unsigned char>(a.b + (b.b - a.b) * t),
        255};
}

// Enemy base colour by kind (sickly greens, with Tank a rusty bulk); burning
// blends toward fire orange.
Color enemyColor(uint8_t type, float burnDps, float burnTtl) {
    Color base;
    switch (static_cast<ls::EnemyType>(type)) {
        case ls::EnemyType::Runner: base = Color{130, 210, 150, 255}; break;
        case ls::EnemyType::Tank:   base = Color{200, 150, 90, 255};  break;
        default:                    base = Color{168, 200, 120, 255}; break;
    }
    if (burnTtl > 0.0f && burnDps > 0.0f) {
        const float f = (burnDps > 12.0f) ? 1.0f : burnDps / 12.0f;
        base = lerpColor(base, Color{255, 120, 40, 255}, f);
    }
    return base;
}

// One triangle straight into the open batch. No rlSetTexture, no rlBegin per
// primitive: raylib's DrawTriangle sets and clears the shapes texture around
// every single call, which is what made the horde cost draw-call overhead
// proportional to entity count.
inline void emit(Vec2 a, Vec2 b, Vec2 c, Color col) {
    rlColor4ub(col.r, col.g, col.b, col.a);
    rlVertex2f(a.x, a.y);
    rlVertex2f(b.x, b.y);
    rlVertex2f(c.x, c.y);
}

struct Body {
    Vec2  pos;
    Vec2  dir;
    Vec2  side;
    float scale;
    Color color;
};

// Tier 2 — one directional shape. What M1 through M4 drew for everything.
void emitShape(const Body& b) {
    const Vec2 tip  = b.pos + b.dir * 5.0f * b.scale;
    const Vec2 back = b.pos - b.dir * 3.0f * b.scale;
    const Vec2 hw   = b.side * 3.0f * b.scale;
    emit(tip, back - hw, back + hw, b.color);
}

// Tier 1 — torso plus head, with a vertical bob. Three triangles.
void emitSilhouette(const Body& b, float phase) {
    const float bob = std::sin(phase) * 0.8f * b.scale;
    const Vec2  up  = b.side * bob;
    const Vec2 tip  = b.pos + b.dir * 5.0f * b.scale + up;
    const Vec2 back = b.pos - b.dir * 4.0f * b.scale + up;
    const Vec2 hw   = b.side * 3.5f * b.scale;

    emit(tip, back - hw, back + hw, b.color);
    emit(back - hw, back + hw, back - b.dir * 2.0f * b.scale, b.color);

    const Vec2 head = b.pos + b.dir * 6.5f * b.scale + up;
    const Vec2 hhw  = b.side * 1.6f * b.scale;
    emit(head + b.dir * 2.0f * b.scale, head - hhw, head + hhw,
         lerpColor(b.color, Color{255, 255, 255, 255}, 0.25f));
}

// Tier 0 — outline, torso, head, weapon arm and two legs on a sine walk
// cycle. Seven triangles, and the phase offset is per-enemy so a thin crowd
// shimmers instead of marching in lockstep.
void emitFull(const Body& b, float phase) {
    const float bob = std::sin(phase * 2.0f) * 0.7f * b.scale;
    const Vec2  up  = b.side * bob;

    // Outline: the same torso, fattened, drawn first so the body covers it.
    const Vec2 otip  = b.pos + b.dir * 7.0f * b.scale + up;
    const Vec2 oback = b.pos - b.dir * 5.5f * b.scale + up;
    const Vec2 ohw   = b.side * 4.8f * b.scale;
    emit(otip, oback - ohw, oback + ohw, kOutline);

    const Vec2 tip  = b.pos + b.dir * 5.0f * b.scale + up;
    const Vec2 back = b.pos - b.dir * 4.0f * b.scale + up;
    const Vec2 hw   = b.side * 3.5f * b.scale;
    emit(tip, back - hw, back + hw, b.color);
    emit(back - hw, back + hw, back - b.dir * 2.5f * b.scale, b.color);

    const Vec2 head = b.pos + b.dir * 6.0f * b.scale + up;
    const Vec2 hhw  = b.side * 1.7f * b.scale;
    emit(head + b.dir * 2.2f * b.scale, head - hhw, head + hhw,
         lerpColor(b.color, Color{255, 255, 255, 255}, 0.3f));

    // Weapon arm, held out to one side.
    const Vec2 arm = b.pos + b.side * 3.6f * b.scale;
    emit(arm + b.dir * 4.5f * b.scale, arm - b.side * 0.9f * b.scale,
         arm + b.side * 0.9f * b.scale,
         lerpColor(b.color, kOutline, 0.35f));

    // Two legs, counter-phased: the walk cycle.
    const float swing = std::sin(phase) * 2.4f * b.scale;
    const Vec2  hip   = b.pos - b.dir * 2.0f * b.scale;
    const Color legCol = lerpColor(b.color, kOutline, 0.2f);
    for (int leg = 0; leg < 2; ++leg) {
        const float s = (leg == 0) ? swing : -swing;
        const Vec2 root = hip + b.side * ((leg == 0) ? 1.8f : -1.8f) * b.scale;
        const Vec2 foot = root - b.dir * 3.2f * b.scale + b.dir * s;
        emit(root - b.side * 0.8f * b.scale, root + b.side * 0.8f * b.scale,
             foot, legCol);
    }
}

}  // namespace

void Renderer::bucketEnemies(const World& world, float alpha, Rect view,
                             const RenderSettings& settings) {
    const EnemyPool& e = world.enemies();
    const SpatialHash& hash = world.hash();
    const uint32_t n = e.count();

    for (auto& b : buckets_) b.clear();
    stats_ = RenderStats{};
    stats_.enemies = n;

    for (uint32_t i = 0; i < n; ++i) {
        const Vec2 p = lerp(e.prevPosition[i], e.position[i], alpha);
        if (!inView(p, view)) continue;

        const LodTier tier =
            settings.lod
                ? tierForLocalCount(hash.countInCell(hash.cellOfPosition(p)))
                : LodTier::Shape;

        buckets_[static_cast<size_t>(tier)].push_back(i);
        ++stats_.tierCount[static_cast<size_t>(tier)];
        stats_.triangles += static_cast<uint32_t>(trianglesForTier(tier));
        ++stats_.drawn;
    }
}

void Renderer::drawHorde(const World& world, float alpha,
                         const RenderSettings& settings) {
    const EnemyPool& e = world.enemies();

    for (size_t t = 0; t < 3; ++t) {
        const std::vector<uint32_t>& bucket = buckets_[t];
        if (bucket.empty()) continue;

        // One rlBegin span per tier: draw call count is a function of the
        // number of tiers, not the number of things (GDD 14.5).
        if (settings.batched) {
            rlBegin(RL_TRIANGLES);
            ++stats_.batches;
        }

        for (const uint32_t i : bucket) {
            Body b;
            b.pos = lerp(e.prevPosition[i], e.position[i], alpha);
            const Vec2 fwd = normalized(e.velocity[i]);
            b.dir = (lengthSq(fwd) > 0.0f) ? fwd : Vec2{1.0f, 0.0f};
            b.side = Vec2{-b.dir.y, b.dir.x};
            // Tanks read as physically bigger in the crowd.
            b.scale = (e.type[i] == static_cast<uint8_t>(ls::EnemyType::Tank))
                          ? 1.7f : 1.0f;
            b.color = enemyColor(e.type[i], e.burnDps[i], e.burnTtl[i]);

            // A per-enemy phase offset, so the crowd shimmers organically
            // instead of marching in lockstep (GDD 12.1). Derived from the
            // index: render-only, never fed back into the simulation.
            const float phase =
                animClock_ * 9.0f + static_cast<float>(i % 32u) * 0.37f;

            if (!settings.batched) {
                // The M4 path, kept for the benchmark's before/after.
                const Vec2 tip  = b.pos + b.dir * 5.0f * b.scale;
                const Vec2 back = b.pos - b.dir * 3.0f * b.scale;
                const Vec2 hw   = b.side * 3.0f * b.scale;
                DrawTriangle(toRl(tip), toRl(back - hw), toRl(back + hw),
                             b.color);
                continue;
            }

            switch (static_cast<LodTier>(t)) {
                case LodTier::Full:       emitFull(b, phase); break;
                case LodTier::Silhouette: emitSilhouette(b, phase); break;
                case LodTier::Shape:      emitShape(b); break;
            }
        }

        if (settings.batched) rlEnd();
    }
}

// Draws the world content only — it issues draw calls and does NOT own the
// frame. BeginDrawing/ClearBackground/EndDrawing are the caller's job (main),
// so this composes with the report/tree overlays without nested begin/end,
// which is exactly what caused the flicker and frame-pacing jitter.
void Renderer::draw(const World& world,
                    float alpha,
                    const DebugFlags& flags,
                    double frameMs,
                    double tickMs,
                    const RenderSettings& settings) {
    const LevelMap& map = world.map();
    const Grid&     grid = map.grid;
    const float     cs = grid.cellSize();

    animClock_ += static_cast<float>(frameMs) * 0.001f;

    // --- walls -------------------------------------------------------------
    for (int cy = 0; cy < grid.rows(); ++cy) {
        for (int cx = 0; cx < grid.cols(); ++cx) {
            if (map.isWalkable(cx, cy)) continue;
            const float x = static_cast<float>(cx) * cs;
            const float y = static_cast<float>(cy) * cs;
            DrawRectangleV(Vector2{x, y}, Vector2{cs, cs}, kWall);
        }
    }

    // --- optional debug overlays -------------------------------------------
    if (flags.showGrid) {
        for (int cx = 0; cx <= grid.cols(); ++cx) {
            const float x = static_cast<float>(cx) * cs;
            DrawLineV(Vector2{x, 0.0f}, Vector2{x, grid.worldHeight()},
                      Color{40, 40, 46, 120});
        }
        for (int cy = 0; cy <= grid.rows(); ++cy) {
            const float y = static_cast<float>(cy) * cs;
            DrawLineV(Vector2{0.0f, y}, Vector2{grid.worldWidth(), y},
                      Color{40, 40, 46, 120});
        }
    }

    if (flags.showFlowField) {
        for (int cy = 0; cy < grid.rows(); ++cy) {
            for (int cx = 0; cx < grid.cols(); ++cx) {
                if (!world.flowField().isReachable(cx, cy)) continue;
                const Vec2 c = grid.cellCenter(cx, cy);
                const Vec2 d = world.flowField().dirAt(cx, cy);
                DrawLineV(toRl(c), toRl(c + d * (cs * 0.4f)), kFlow);
            }
        }
    }

    // --- enemies (bucketed by density-driven LOD tier) ---------------------
    const Rect view = viewportRect(static_cast<float>(GetScreenWidth()),
                                   static_cast<float>(GetScreenHeight()),
                                   kCullMargin);
    bucketEnemies(world, alpha, view, settings);
    drawHorde(world, alpha, settings);

    // --- hardpoints (empty slots) ------------------------------------------
    for (const Vec2& hp : map.hardpoints) {
        DrawCircleLinesV(toRl(hp), 10.0f, kHardpoint);
    }

    // --- turrets and range rings -------------------------------------------
    for (const ls::Turret& t : world.turrets()) {
        if (flags.showTurretRange) {
            DrawCircleLinesV(toRl(t.position), t.range, kRange);
        }
        switch (t.kind) {
            case ls::TurretKind::MachineGun:
                DrawCircleV(toRl(t.position), 8.0f, kTurret);
                DrawCircleLinesV(toRl(t.position), 8.0f, Color{255, 255, 255, 255});
                break;
            case ls::TurretKind::Cannon: {
                const Rectangle r{t.position.x - 10.0f, t.position.y - 10.0f,
                                  20.0f, 20.0f};
                DrawRectangleV(Vector2{r.x, r.y}, Vector2{r.width, r.height},
                               Color{120, 160, 200, 255});
                DrawRectangleLinesEx(r, 2.0f, Color{255, 255, 255, 255});
                break;
            }
            case ls::TurretKind::Flamethrower: {
                const Vec2 r{10.0f, 0.0f};
                const Vec2 u{0.0f, 8.0f};
                DrawTriangle(toRl(t.position + r), toRl(t.position - r + u),
                             toRl(t.position - r - u), Color{255, 150, 60, 255});
                break;
            }
        }
        if (t.overchargeTtl > 0.0f) {
            DrawCircleLinesV(toRl(t.position), 13.0f, Color{255, 240, 120, 255});
        } else if (t.overheatTtl > 0.0f) {
            DrawCircleLinesV(toRl(t.position), 13.0f, Color{255, 70, 50, 255});
        }
    }

    // --- tracers (fading hitscan lines) ------------------------------------
    for (uint32_t i = 0; i < world.tracerCount(); ++i) {
        const Tracer& tr = world.tracers()[i];
        const float a = (tr.ttl > 0.0f) ? (tr.ttl / kTracerTtl) : 0.0f;
        const Color c{
            kTracer.r, kTracer.g, kTracer.b,
            static_cast<unsigned char>(a * 255.0f)};
        DrawLineV(toRl(tr.from), toRl(tr.to), c);
    }

    // --- base ---------------------------------------------------------------
    const Base& b = world.base();
    const float frac = (b.maxHealth > 0.0f) ? (b.health / b.maxHealth) : 0.0f;
    const Color baseColor = (frac > 0.35f) ? kBaseGood : kBaseBad;
    DrawCircleV(toRl(b.position), b.radius, baseColor);
    DrawCircleLinesV(toRl(b.position), b.radius + 4.0f, baseColor);

    // --- overlay (raylib text; ImGui arrives in M2) -------------------------
    char line[256];
    std::snprintf(line, sizeof(line),
                  "entities %u   frame %.2f ms   tick %.3f ms   fps %d",
                  stats_.enemies, frameMs, tickMs, GetFPS());
    DrawText(line, 12, 12, 18, kText);

    std::snprintf(line, sizeof(line), "base %.0f / %.0f    arrived %u    ticks %llu",
                  static_cast<double>(b.health),
                  static_cast<double>(b.maxHealth),
                  world.totalArrived(),
                  static_cast<unsigned long long>(world.ticks()));
    DrawText(line, 12, 34, 18, kText);

    std::snprintf(line, sizeof(line), "turrets %zu    shots %llu    kills %u",
                  world.turrets().size(),
                  static_cast<unsigned long long>(world.totalShots()),
                  world.totalKills());
    DrawText(line, 12, 56, 18, kText);

    std::snprintf(line, sizeof(line),
                  "lod  full %u  silhouette %u  shape %u   tris %u   batches %u",
                  stats_.tierCount[0], stats_.tierCount[1], stats_.tierCount[2],
                  stats_.triangles, stats_.batches);
    DrawText(line, 12, 78, 16, Color{120, 130, 145, 255});

    DrawText("[F] flow [G] grid [T] range",
             12, 100, 16, Color{120, 130, 145, 255});

    if (world.isOver()) {
        DrawText("BATTLE OVER", 12, 126, 32, kBaseBad);
    }
}

}  // namespace ls
