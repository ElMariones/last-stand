#include "render/Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <raylib.h>
#include <rlgl.h>

#include "render/Icons.h"
#include "render/Theme.h"

namespace ls {

namespace {

constexpr Color kWall       = theme::kWall;
constexpr Color kText       = theme::kInk;
constexpr Color kFlow       {70, 90, 110, 255};
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
        case ls::EnemyType::Runner:   base = theme::kRunner;   break;
        case ls::EnemyType::Tank:     base = theme::kTank;     break;
        case ls::EnemyType::Swarmer:  base = theme::kSwarmer;  break;
        case ls::EnemyType::Brute:    base = theme::kBrute;    break;
        case ls::EnemyType::Phantom:  base = theme::kPhantom;  break;
        case ls::EnemyType::Behemoth: base = theme::kBehemoth; break;
        default:                      base = theme::kGrunt;    break;
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
//
// The winding is fixed here rather than by hand at each call site. Backface
// culling silently discards a triangle wound the wrong way - no warning, no
// error, the detail simply is not there - and that has now eaten an
// afternoon twice, because the natural way to read points off a sketch is
// whichever way you happened to draw it. One cross product per triangle buys
// the guarantee that a shape as authored is a shape as drawn. It is four
// multiplies against three vertex submissions, and it is render-side, so the
// simulation budget never sees it.
inline void emit(Vec2 a, Vec2 b, Vec2 c, Color col) {
    const float cross =
        (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cross > 0.0f) {
        const Vec2 t = b;
        b = c;
        c = t;
    }
    rlColor4ub(col.r, col.g, col.b, col.a);
    rlVertex2f(a.x, a.y);
    rlVertex2f(b.x, b.y);
    rlVertex2f(c.x, c.y);
}

struct Body {
    Vec2    pos;
    Vec2    dir;
    Vec2    side;
    float   scale;
    Color   color;
    uint8_t kind = 0u;
};

// Tier 2 — one directional shape. What M1 through M4 drew for everything.
void emitShape(const Body& b) {
    const Vec2 tip  = b.pos + b.dir * 5.0f * b.scale;
    const Vec2 back = b.pos - b.dir * 3.0f * b.scale;
    const Vec2 hw   = b.side * 3.0f * b.scale;
    emit(tip, back - hw, back + hw, b.color);
}

// Tier 1 — torso plus head, with a vertical bob. Three triangles.
//
// The proportions come from the kind rather than being one shape for
// everybody: at this density a Brute still has to look wider than a Phantom
// is long, or the second tier of detail quietly erases the bestiary.
void emitSilhouette(const Body& b, float phase) {
    float reach = 5.0f;
    float width = 3.5f;
    switch (static_cast<ls::EnemyType>(b.kind)) {
        case ls::EnemyType::Swarmer:  reach = 6.0f; width = 2.3f; break;
        case ls::EnemyType::Brute:    reach = 4.2f; width = 5.6f; break;
        case ls::EnemyType::Phantom:  reach = 8.0f; width = 2.6f; break;
        case ls::EnemyType::Behemoth: reach = 7.0f; width = 6.4f; break;
        default: break;
    }

    const float bob = std::sin(phase) * 0.8f * b.scale;
    const Vec2  up  = b.side * bob;
    const Vec2 tip  = b.pos + b.dir * reach * b.scale + up;
    const Vec2 back = b.pos - b.dir * 4.0f * b.scale + up;
    const Vec2 hw   = b.side * width * b.scale;

    emit(tip, back - hw, back + hw, b.color);
    emit(back - hw, back + hw, back - b.dir * 2.0f * b.scale, b.color);

    const Vec2 head = b.pos + b.dir * (reach + 1.5f) * b.scale + up;
    const Vec2 hhw  = b.side * (width * 0.46f) * b.scale;
    emit(head + b.dir * 2.0f * b.scale, head - hhw, head + hhw,
         lerpColor(b.color, Color{255, 255, 255, 255}, 0.25f));
}

// Tier 0 — outline, torso, head, weapon arm and two legs on a sine walk
// cycle. Seven triangles, and the phase offset is per-enemy so a thin crowd
// shimmers instead of marching in lockstep.
void emitHumanoid(const Body& b, float phase) {
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

// Swarmer — a dart, not a soldier. No legs, no weapon arm: it does not walk
// and it is not carrying anything. Four triangles, and it is small enough
// that anything more would be mud.
void emitSwarmer(const Body& b, float phase) {
    const float skitter = std::sin(phase * 3.0f) * 1.1f * b.scale;
    const Vec2  up = b.side * skitter;

    const Vec2 tip  = b.pos + b.dir * 6.5f * b.scale + up;
    const Vec2 back = b.pos - b.dir * 3.0f * b.scale + up;
    const Vec2 hw   = b.side * 2.4f * b.scale;
    emit(tip, back - hw, back + hw, b.color);
    // Split abdomen: two short spurs trailing behind the point.
    emit(back, back - b.dir * 3.0f * b.scale - b.side * 1.6f * b.scale,
         back - b.side * 0.6f * b.scale, lerpColor(b.color, kOutline, 0.3f));
    emit(back, back - b.dir * 3.0f * b.scale + b.side * 1.6f * b.scale,
         back + b.side * 0.6f * b.scale, lerpColor(b.color, kOutline, 0.3f));
    // Antennae, swept forward.
    const Vec2 brow = b.pos + b.dir * 5.0f * b.scale + up;
    emit(brow + b.dir * 3.5f * b.scale - b.side * 2.6f * b.scale,
         brow - b.side * 0.4f * b.scale, brow + b.side * 0.4f * b.scale,
         lerpColor(b.color, Color{255, 255, 255, 255}, 0.35f));
}

// Brute — armour you can see. A wide slab of a torso under two pauldrons,
// on legs too short for its bulk. The read has to be "shooting this with a
// machine gun is a mistake" from across the field.
void emitBrute(const Body& b, float phase) {
    const float bob = std::sin(phase * 1.6f) * 0.5f * b.scale;
    const Vec2  up  = b.side * bob;

    const Vec2 otip  = b.pos + b.dir * 6.0f * b.scale + up;
    const Vec2 oback = b.pos - b.dir * 5.5f * b.scale + up;
    const Vec2 ohw   = b.side * 6.4f * b.scale;
    emit(otip, oback - ohw, oback + ohw, kOutline);

    // Slab torso: two triangles making a broad rectangle rather than a wedge.
    const Vec2 fwd = b.pos + b.dir * 4.0f * b.scale + up;
    const Vec2 aft = b.pos - b.dir * 4.0f * b.scale + up;
    const Vec2 hw  = b.side * 5.0f * b.scale;
    emit(fwd - hw, fwd + hw, aft - hw, b.color);
    emit(fwd + hw, aft + hw, aft - hw, b.color);

    // Pauldrons, brighter than the body: the plates are the whole point.
    const Color plate = lerpColor(b.color, Color{255, 255, 255, 255}, 0.32f);
    for (int side = 0; side < 2; ++side) {
        const float s = (side == 0) ? 1.0f : -1.0f;
        const Vec2 root = b.pos + b.side * (5.0f * s) * b.scale + up;
        emit(root + b.dir * 3.4f * b.scale,
             root + b.side * (2.6f * s) * b.scale,
             root - b.dir * 2.6f * b.scale, plate);
    }

    // A low, sunken head between the shoulders.
    const Vec2 head = b.pos + b.dir * 5.2f * b.scale + up;
    emit(head + b.dir * 1.8f * b.scale, head - b.side * 1.9f * b.scale,
         head + b.side * 1.9f * b.scale, lerpColor(b.color, kOutline, 0.45f));

    const float swing = std::sin(phase) * 1.6f * b.scale;
    const Vec2  hip = b.pos - b.dir * 3.4f * b.scale;
    const Color legCol = lerpColor(b.color, kOutline, 0.3f);
    for (int leg = 0; leg < 2; ++leg) {
        const float s = (leg == 0) ? swing : -swing;
        const Vec2 root = hip + b.side * ((leg == 0) ? 2.6f : -2.6f) * b.scale;
        const Vec2 foot = root - b.dir * 2.6f * b.scale + b.dir * s;
        emit(root - b.side * 1.2f * b.scale, root + b.side * 1.2f * b.scale,
             foot, legCol);
    }
}

// Phantom — it does not walk, it streams. A long tapered body with a wake
// behind it and no legs at all, so a crowd of them reads as something moving
// differently even before you notice it weaving.
void emitPhantom(const Body& b, float phase) {
    const float sway = std::sin(phase * 1.3f) * 1.8f * b.scale;
    const Vec2  up   = b.side * sway;

    // The wake: a long, dim triangle trailing straight back.
    const Vec2 tail = b.pos - b.dir * 13.0f * b.scale - up * 1.6f;
    emit(tail, b.pos - b.side * 2.2f * b.scale, b.pos + b.side * 2.2f * b.scale,
         theme::withAlpha(b.color, 0.28f));

    const Vec2 tip  = b.pos + b.dir * 8.5f * b.scale + up;
    const Vec2 back = b.pos - b.dir * 4.0f * b.scale + up;
    const Vec2 hw   = b.side * 2.9f * b.scale;
    emit(tip, back - hw, back + hw, b.color);

    // Two swept vanes where a humanoid would have arms.
    const Color vane = lerpColor(b.color, Color{255, 255, 255, 255}, 0.4f);
    for (int side = 0; side < 2; ++side) {
        const float s = (side == 0) ? 1.0f : -1.0f;
        const Vec2 root = b.pos + b.side * (2.4f * s) * b.scale + up;
        emit(root + b.dir * 2.0f * b.scale,
             root + b.side * (5.2f * s) * b.scale - b.dir * 3.0f * b.scale,
             root - b.dir * 1.4f * b.scale, vane);
    }

    // A single bright eye, which is all a player needs to pick one out.
    const Vec2 eye = b.pos + b.dir * 6.0f * b.scale + up;
    emit(eye + b.dir * 1.4f * b.scale, eye - b.side * 1.0f * b.scale,
         eye + b.side * 1.0f * b.scale,
         lerpColor(b.color, Color{255, 255, 255, 255}, 0.75f));
}

// Behemoth — three segments and four legs, and big enough that the LOD tier
// it lands in barely matters. There are never many on screen, so it can
// afford the triangles the horde cannot.
void emitBehemoth(const Body& b, float phase) {
    const float bob = std::sin(phase * 0.9f) * 0.6f * b.scale;
    const Vec2  up  = b.side * bob;

    const Vec2 otip  = b.pos + b.dir * 9.0f * b.scale + up;
    const Vec2 oback = b.pos - b.dir * 9.5f * b.scale + up;
    const Vec2 ohw   = b.side * 7.0f * b.scale;
    emit(otip, oback - ohw, oback + ohw, kOutline);

    const Color shell = lerpColor(b.color, Color{255, 255, 255, 255}, 0.18f);
    const Color deep  = lerpColor(b.color, kOutline, 0.35f);

    // Abdomen, thorax, head: three plates down the length of it.
    const struct { float front; float back; float halfWidth; Color col; }
    segments[3] = {
        {-1.0f, -8.0f, 5.6f, deep},
        { 4.5f, -1.5f, 6.2f, b.color},
        { 8.5f,  4.0f, 3.4f, shell},
    };
    for (const auto& seg : segments) {
        const Vec2 f  = b.pos + b.dir * seg.front * b.scale + up;
        const Vec2 a  = b.pos + b.dir * seg.back * b.scale + up;
        const Vec2 hw = b.side * seg.halfWidth * b.scale;
        emit(f - hw, f + hw, a - hw, seg.col);
        emit(f + hw, a + hw, a - hw, seg.col);
    }

    // Four legs on two counter-phased pairs.
    const Color legCol = lerpColor(b.color, kOutline, 0.25f);
    for (int leg = 0; leg < 4; ++leg) {
        const float s = std::sin(phase + static_cast<float>(leg) * 1.57f) *
                        2.0f * b.scale;
        const float alongBody = (leg < 2) ? 2.0f : -4.0f;
        const float sideSign = (leg % 2 == 0) ? 1.0f : -1.0f;
        const Vec2 root = b.pos + b.dir * alongBody * b.scale +
                          b.side * (5.8f * sideSign) * b.scale;
        const Vec2 foot = root + b.side * (3.2f * sideSign) * b.scale +
                          b.dir * s;
        emit(root - b.dir * 1.4f * b.scale, root + b.dir * 1.4f * b.scale,
             foot, legCol);
    }
}

void emitFull(const Body& b, float phase) {
    switch (static_cast<ls::EnemyType>(b.kind)) {
        case ls::EnemyType::Swarmer:  emitSwarmer(b, phase); break;
        case ls::EnemyType::Brute:    emitBrute(b, phase); break;
        case ls::EnemyType::Phantom:  emitPhantom(b, phase); break;
        case ls::EnemyType::Behemoth: emitBehemoth(b, phase); break;
        default:                      emitHumanoid(b, phase); break;
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
            // Size is the kind's own, straight off the stats table, so a
            // Swarmer is visibly slight and a Behemoth visibly is not.
            b.kind = e.type[i];
            b.scale = ls::enemyStatsTable()[e.type[i]].scale;
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
        // A soft shadow under the chassis, so turrets sit on the ground
        // rather than floating over it.
        DrawCircleV(Vector2{t.position.x + 1.5f, t.position.y + 3.0f}, 13.0f,
                    theme::withAlpha(theme::kVoid, 0.5f));
        drawTurret(t.kind, toRl(t.position), 1.25f, t.facing,
                   t.overchargeTtl > 0.0f, t.overheatTtl > 0.0f);
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
    drawBase(toRl(b.position), b.radius, frac, animClock_);

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
