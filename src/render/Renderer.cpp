#include "render/Renderer.h"

#include <cstdio>
#include <raylib.h>

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

constexpr float kTracerTtl = 0.08f;   // matches CombatSystem

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

}  // namespace

// Draws the world content only — it issues draw calls and does NOT own the
// frame. BeginDrawing/ClearBackground/EndDrawing are the caller's job (main),
// so this composes with the report/tree overlays without nested begin/end,
// which is exactly what caused the flicker and frame-pacing jitter.
void Renderer::draw(const World& world,
                    float alpha,
                    const DebugFlags& flags,
                    double frameMs,
                    double tickMs) {
    const LevelMap& map = world.map();
    const Grid&     grid = map.grid;
    const float     cs = grid.cellSize();

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

    // --- enemies (LOD tier 2: one directional shape each) -------------------
    const EnemyPool& e = world.enemies();
    const uint32_t   n = e.count();
    for (uint32_t i = 0; i < n; ++i) {
        const Vec2 p = lerp(e.prevPosition[i], e.position[i], alpha);
        const Vec2 fwd = normalized(e.velocity[i]);
        const Vec2 dir = (lengthSq(fwd) > 0.0f) ? fwd : Vec2{1.0f, 0.0f};
        const Vec2 side{-dir.y, dir.x};

        // Tanks read as physically bigger in the crowd.
        const float scale =
            (e.type[i] == static_cast<uint8_t>(ls::EnemyType::Tank)) ? 1.7f : 1.0f;
        const Vec2 tip  = p + dir * 5.0f * scale;
        const Vec2 back = p - dir * 3.0f * scale;
        const Vec2 hw   = side * 3.0f * scale;

        // If the triangles render invisible, raylib has culled them for
        // winding order — swap the last two vertices.
        DrawTriangle(toRl(tip), toRl(back - hw), toRl(back + hw),
                     enemyColor(e.type[i], e.burnDps[i], e.burnTtl[i]));
    }

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
                  n, frameMs, tickMs, GetFPS());
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

    DrawText("[F] flow [G] grid [T] range",
             12, 78, 16, Color{120, 130, 145, 255});

    if (world.isOver()) {
        DrawText("BATTLE OVER", 12, 104, 32, kBaseBad);
    }
}

}  // namespace ls
