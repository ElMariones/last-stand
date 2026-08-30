#include "render/Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <raylib.h>
#include <rlgl.h>

#include "render/Theme.h"

namespace ls {

namespace {

constexpr Color kWall       = theme::kWall;
constexpr Color kBaseGood   = theme::kCold;
constexpr Color kBaseBad    = theme::kDanger;
constexpr Color kText       = theme::kInk;
constexpr Color kFlow       {70, 90, 110, 255};
constexpr Color kTurret     = theme::kCold;
constexpr Color kHardpoint  {90, 90, 96, 255};
constexpr Color kRange      {70, 130, 160, 110};
constexpr Color kTracer     = theme::kTracer;
constexpr Color kOutline    = theme::kOutline;

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
        case ls::EnemyType::Runner: base = theme::kRunner; break;
        case ls::EnemyType::Tank:   base = theme::kTank;   break;
        default:                    base = theme::kGrunt;  break;
    }
    if (burnTtl > 0.0f && burnDps > 0.0f) {
        const float f = (burnDps > 12.0f) ? 1.0f : burnDps / 12.0f;
        base = lerpColor(base, theme::kFireMid, f);
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
// so this composes with the report/tree overlays without nested begin/end.
//
// Everything world-space goes through a Camera2D whose offset is the
// screenshake, so the shake is one transform rather than an offset threaded
// through every draw call — and the HUD, drawn outside it, stays legible
// while the battlefield comes apart.
void Renderer::drawTerrain(const World& world, const DebugFlags& flags) {
    const LevelMap& map = world.map();
    const Grid&     grid = map.grid;
    const float     cs = grid.cellSize();

    DrawRectangleV(Vector2{0.0f, 0.0f},
                   Vector2{grid.worldWidth(), grid.worldHeight()},
                   theme::kGround);

    // A coarse floor grid every four cells. Without it the field is an
    // undifferentiated dark rectangle and the horde has nothing to move
    // against, which reads as emptiness rather than as space.
    for (int cx = 0; cx <= grid.cols(); cx += 4) {
        const float x = static_cast<float>(cx) * cs;
        DrawLineV(Vector2{x, 0.0f}, Vector2{x, grid.worldHeight()},
                  theme::kGroundEdge);
    }
    for (int cy = 0; cy <= grid.rows(); cy += 4) {
        const float y = static_cast<float>(cy) * cs;
        DrawLineV(Vector2{0.0f, y}, Vector2{grid.worldWidth(), y},
                  theme::kGroundEdge);
    }
    DrawRectangleLinesEx(Rectangle{0.0f, 0.0f, grid.worldWidth(),
                                   grid.worldHeight()},
                         2.0f, theme::kGroundLine);

    // The edge the horde comes from, marked once rather than explained in
    // text: a warm bar down the spawn column.
    DrawRectangleV(Vector2{0.0f, 0.0f}, Vector2{cs * 0.35f, grid.worldHeight()},
                   theme::withAlpha(theme::kFireDeep, 0.35f));

    // Walls get a warm rimlight on their upper edge and a shadow below, which
    // is what stops a flat top-down field from reading as a spreadsheet.
    for (int cy = 0; cy < grid.rows(); ++cy) {
        for (int cx = 0; cx < grid.cols(); ++cx) {
            if (map.isWalkable(cx, cy)) continue;
            const float x = static_cast<float>(cx) * cs;
            const float y = static_cast<float>(cy) * cs;
            DrawRectangleV(Vector2{x, y}, Vector2{cs, cs}, kWall);
            if (map.isWalkable(cx, cy - 1)) {
                DrawRectangleV(Vector2{x, y}, Vector2{cs, 2.0f},
                               theme::kWallLight);
            }
            if (map.isWalkable(cx, cy + 1)) {
                DrawRectangleV(Vector2{x, y + cs - 2.0f}, Vector2{cs, 2.0f},
                               theme::kWallShadow);
            }
        }
    }

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
}

void Renderer::drawCorpses(const FxScene& fx) {
    if (fx.corpses == nullptr) return;
    const CorpseRing& ring = *fx.corpses;

    // One batch for the whole graveyard, drawn under everything living.
    rlBegin(RL_TRIANGLES);
    for (uint32_t s = 0; s < ring.count(); ++s) {
        const uint32_t i = ring.indexAt(s);
        const float f = ring.fade(i);
        const Vec2 p = ring.positionAt(i);
        const Vec2 d = ring.directionAt(i);
        const Vec2 side{-d.y, d.x};
        // Corpses flatten as they fade: a smear, not a sleeping enemy.
        const float w = 4.0f * (1.0f - f * 0.35f);
        const Color c = theme::withAlpha(theme::kOutline, 1.0f - f);
        emit(p + side * w, p - side * w, p - d * (6.0f - f * 2.0f), c);
    }
    rlEnd();
}

void Renderer::drawParticles(const FxScene& fx) {
    if (fx.particles == nullptr) return;
    const ParticlePool& pool = *fx.particles;

    rlBegin(RL_TRIANGLES);
    for (uint32_t i = 0; i < pool.count(); ++i) {
        const float t = pool.progress(i);
        const Vec2 p = pool.position[i];
        float size = pool.size[i];
        Color c{};
        switch (static_cast<ParticleKind>(pool.kind[i])) {
            case ParticleKind::Spark:
                c = theme::mix(theme::kFireCore, theme::kFireMid, t);
                size *= (1.0f - t * 0.6f);
                break;
            case ParticleKind::Ember:
                c = theme::mix(theme::kFireMid, theme::kFireDeep, t);
                break;
            case ParticleKind::Smoke:
                c = theme::mix(Color{90, 78, 70, 255}, theme::kVoid, t);
                size *= (1.0f + t * 1.6f);
                break;
            case ParticleKind::Flash:
                c = theme::kFireCore;
                size *= (1.0f - t);
                break;
            case ParticleKind::ScrapArc:
                c = theme::kScrap;
                break;
        }
        c = theme::withAlpha(c, 1.0f - t * t);

        // A quad as two triangles: cheaper to reason about than a circle and
        // indistinguishable at these sizes.
        const Vec2 a{p.x - size, p.y - size};
        const Vec2 b{p.x + size, p.y - size};
        const Vec2 d{p.x + size, p.y + size};
        const Vec2 e{p.x - size, p.y + size};
        emit(a, e, d, c);
        emit(a, d, b, c);
    }
    rlEnd();
}

void Renderer::drawNumbers(const FxScene& fx) {
    if (fx.numbers == nullptr || !fx.showNumbers) return;
    const DamageNumbers& dn = *fx.numbers;
    char buf[24];
    for (uint32_t i = 0; i < dn.count(); ++i) {
        const float t = dn.progressAt(i);
        const Vec2 p = dn.positionAt(i);
        const float amount = dn.amountAt(i);
        // Bigger hits get bigger type, which is the whole reason to aggregate.
        const int size = (amount >= 500.0f) ? theme::kBody
                        : (amount >= 150.0f) ? theme::kSmall : theme::kMicro;
        std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(amount));
        DrawText(buf, static_cast<int>(p.x), static_cast<int>(p.y), size,
                 theme::withAlpha(theme::kInk, 1.0f - t * t));
    }
}

void Renderer::drawTurrets(const World& world, const DebugFlags& flags) {
    for (const ls::Turret& t : world.turrets()) {
        if (flags.showTurretRange) {
            DrawCircleLinesV(toRl(t.position), t.range, kRange);
        }
        switch (t.kind) {
            case ls::TurretKind::MachineGun:
                DrawCircleV(toRl(t.position), 8.0f, kTurret);
                DrawCircleLinesV(toRl(t.position), 8.0f, theme::kInk);
                break;
            case ls::TurretKind::Cannon: {
                const Rectangle r{t.position.x - 10.0f, t.position.y - 10.0f,
                                  20.0f, 20.0f};
                DrawRectangleV(Vector2{r.x, r.y}, Vector2{r.width, r.height},
                               theme::kColdDim);
                DrawRectangleLinesEx(r, 2.0f, theme::kInk);
                break;
            }
            case ls::TurretKind::Flamethrower: {
                const Vec2 r{10.0f, 0.0f};
                const Vec2 u{0.0f, 8.0f};
                DrawTriangle(toRl(t.position + r), toRl(t.position - r + u),
                             toRl(t.position - r - u), theme::kFireMid);
                break;
            }
        }
        if (t.overchargeTtl > 0.0f) {
            DrawCircleLinesV(toRl(t.position), 13.0f, theme::kScrap);
        } else if (t.overheatTtl > 0.0f) {
            DrawCircleLinesV(toRl(t.position), 13.0f, theme::kDanger);
        }
    }
}

void Renderer::drawVignette() {
    // Four gradient bands rather than a texture: no asset, no shader, and it
    // pulls the eye to the middle of the field where the fighting is.
    const int w = GetScreenWidth();
    const int h = GetScreenHeight();
    const int band = 120;
    const Color edge = theme::withAlpha(theme::kVoid, 0.75f);
    const Color none = theme::withAlpha(theme::kVoid, 0.0f);
    DrawRectangleGradientV(0, 0, w, band, edge, none);
    DrawRectangleGradientV(0, h - band, w, band, none, edge);
    DrawRectangleGradientH(0, 0, band, h, edge, none);
    DrawRectangleGradientH(w - band, 0, band, h, none, edge);
}

void Renderer::draw(const World& world,
                    const Viewport& viewport,
                    float alpha,
                    const DebugFlags& flags,
                    const RenderSettings& settings,
                    const FxScene& fx) {
    // One transform carries both the fit-to-window scale and the screenshake,
    // so every world-space draw call below is written in world units and
    // nothing has to know how big the window is.
    Camera2D camera{};
    camera.target = Vector2{0.0f, 0.0f};
    camera.offset = Vector2{viewport.origin.x + fx.shake.x,
                            viewport.origin.y + fx.shake.y};
    camera.rotation = 0.0f;
    camera.zoom = viewport.zoom;

    // Outside the fitted world is not playfield; fill it so letterbox bars
    // read as frame rather than as more map.
    ClearBackground(theme::kVoid);

    BeginMode2D(camera);

    drawTerrain(world, flags);
    drawCorpses(fx);

    // Culled against the world, not the window: the whole map is always on
    // screen now, so the only thing culling drops is what has been shoved
    // outside the playfield.
    const Rect view = viewportRect(world.map().grid.worldWidth(),
                                   world.map().grid.worldHeight(), kCullMargin);
    bucketEnemies(world, alpha, view, settings);
    drawHorde(world, alpha, settings);

    for (const Vec2& hp : world.map().hardpoints) {
        DrawCircleLinesV(toRl(hp), 10.0f, kHardpoint);
    }
    drawTurrets(world, flags);

    // Tracers get a bright core over a wider soft body: the difference
    // between "a line" and "a shot".
    for (uint32_t i = 0; i < world.tracerCount(); ++i) {
        const Tracer& tr = world.tracers()[i];
        const float a = (tr.ttl > 0.0f) ? (tr.ttl / kTracerTtl) : 0.0f;
        DrawLineEx(toRl(tr.from), toRl(tr.to), 3.0f,
                   theme::withAlpha(theme::kColdDim, a * 0.5f));
        DrawLineV(toRl(tr.from), toRl(tr.to),
                  theme::withAlpha(kTracer, a));
    }

    drawParticles(fx);

    const Base& b = world.base();
    const float frac = (b.maxHealth > 0.0f) ? (b.health / b.maxHealth) : 0.0f;
    const Color baseColor = (frac > 0.35f) ? kBaseGood : kBaseBad;
    // The base pulses faster the closer it is to falling.
    const float pulse =
        0.5f + 0.5f * std::sin(animClock_ * (3.0f + (1.0f - frac) * 9.0f));
    DrawCircleV(toRl(b.position), b.radius,
                theme::mix(theme::kColdDeep, baseColor, 0.55f + pulse * 0.45f));
    DrawCircleLinesV(toRl(b.position), b.radius + 4.0f, baseColor);
    DrawCircleLinesV(toRl(b.position), b.radius + 8.0f + pulse * 3.0f,
                     theme::withAlpha(baseColor, 0.35f));

    drawNumbers(fx);

    EndMode2D();

    drawVignette();
}

void Renderer::drawDebugOverlay(const World& world, double frameMs,
                                double tickMs) {
    char line[256];
    std::snprintf(line, sizeof(line),
                  "entities %u   frame %.2f ms   tick %.3f ms   fps %d",
                  stats_.enemies, frameMs, tickMs, GetFPS());
    DrawText(line, 12, GetScreenHeight() - 92, theme::kMicro, kText);

    std::snprintf(line, sizeof(line),
                  "lod  full %u  silhouette %u  shape %u   tris %u  batches %u",
                  stats_.tierCount[0], stats_.tierCount[1], stats_.tierCount[2],
                  stats_.triangles, stats_.batches);
    DrawText(line, 12, GetScreenHeight() - 76, theme::kMicro,
             theme::kInkFaint);

    std::snprintf(line, sizeof(line),
                  "turrets %zu  shots %llu  kills %u  arrived %u  ticks %llu",
                  world.turrets().size(),
                  static_cast<unsigned long long>(world.totalShots()),
                  world.totalKills(), world.totalArrived(),
                  static_cast<unsigned long long>(world.ticks()));
    DrawText(line, 12, GetScreenHeight() - 60, theme::kMicro,
             theme::kInkFaint);
}

}  // namespace ls
