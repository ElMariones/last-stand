#include "render/Renderer.h"

#include <cstdio>
#include <raylib.h>

namespace ls {

namespace {

// Palette from GDD 12.1: cold player, warm world, sickly enemies.
constexpr Color kBackground{12, 10, 10, 255};
constexpr Color kWall{46, 38, 34, 255};
constexpr Color kEnemy{168, 200, 120, 255};
constexpr Color kBaseGood{150, 220, 255, 255};
constexpr Color kBaseBad{255, 90, 70, 255};
constexpr Color kText{200, 220, 240, 255};
constexpr Color kFlow{70, 90, 110, 255};

inline Vector2 toRl(Vec2 v) { return Vector2{v.x, v.y}; }

}  // namespace

void Renderer::draw(const World& world,
                    float alpha,
                    const DebugFlags& flags,
                    double frameMs,
                    double tickMs) {
    const LevelMap& map = world.map();
    const Grid&     grid = map.grid;
    const float     cs = grid.cellSize();

    BeginDrawing();
    ClearBackground(kBackground);

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
        // A degenerate (stationary) enemy still needs an orientation.
        const Vec2 dir = (lengthSq(fwd) > 0.0f) ? fwd : Vec2{1.0f, 0.0f};
        const Vec2 side{-dir.y, dir.x};

        const Vec2 tip  = p + dir * 5.0f;
        const Vec2 back = p - dir * 3.0f;
        // If the triangles render invisible, raylib has culled them for
        // winding order — swap the last two vertices.
        DrawTriangle(toRl(tip),
                     toRl(back - side * 3.0f),
                     toRl(back + side * 3.0f),
                     kEnemy);
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

    DrawText("[F] flow field   [G] grid   [SPACE] spawn 100   [R] reset",
             12, 56, 16, Color{120, 130, 145, 255});

    if (world.isOver()) {
        DrawText("BASE DESTROYED", 12, 84, 32, kBaseBad);
    }

    EndDrawing();
}

}  // namespace ls
