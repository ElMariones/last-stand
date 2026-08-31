#include "sim/MovementSystem.h"

#include <cmath>

namespace ls {

namespace {

// Pairing every cell with itself plus these four offsets visits each pair of
// neighbouring cells exactly once. Valid only while the separation radius
// fits inside one cell, which updateMovement checks before taking this path.
constexpr int kHalfNeighbours[4][2] = {{1, 0}, {-1, 1}, {0, 1}, {1, 1}};

// How fast a weaving enemy swings from one side of its path to the other.
// About 0.6 s per full cycle: fast enough that a turret's aim slips off it,
// slow enough to read as a body swerving rather than a sprite vibrating.
constexpr float kWeaveRadiansPerSecond = 10.0f;
constexpr float kTwoPi = 6.28318530717958647692f;

// The separation force neighbour j (at jPos) exerts on enemy i (at iPos).
// Antisymmetric by construction — f(i,j) == -f(j,i) — which is what lets the
// pair walk compute each interaction once and apply it to both enemies.
inline Vec2 separationForce(Vec2 iPos, Vec2 jPos, uint32_t i, uint32_t j,
                            float radiusSq, float invRadius) {
    const Vec2  delta = iPos - jPos;
    const float dSq   = lengthSq(delta);
    if (dSq > radiusSq) return Vec2{0.0f, 0.0f};

    if (dSq <= 1e-6f) {
        // Perfectly coincident. Nudge by index order rather than randomly,
        // so the simulation stays reproducible.
        return Vec2{(i < j) ? -1.0f : 1.0f, 0.0f};
    }

    // One divide, not three: invDist recovers the distance (dSq * invDist)
    // and normalises delta, and invRadius is loop-invariant.
    const float invDist = 1.0f / std::sqrt(dSq);
    const float falloff = 1.0f - (dSq * invDist) * invRadius;  // 1 at 0, 0 at r
    return delta * (invDist * falloff);
}

// Stage 0: every ordered pair, every tick.
void accumulateNaive(const EnemyPool& pool, uint32_t n, float radiusSq,
                     float invRadius, std::vector<Vec2>& push) {
    for (uint32_t i = 0; i < n; ++i) {
        const Vec2 self = pool.prevPosition[i];
        Vec2 sum{0.0f, 0.0f};
        for (uint32_t j = 0; j < n; ++j) {
            if (j == i) continue;
            sum += separationForce(self, pool.prevPosition[j], i, j, radiusSq,
                                   invRadius);
        }
        push[i] = sum;
    }
}

// Stage 2: walk cell pairs, not entities. Each interacting pair is found once
// and applied to both ends, so the neighbour maths runs half as often as the
// per-entity query does — and both sides of it read cell-packed positions.
void accumulatePairs(const SpatialHash& hash, float radiusSq, float invRadius,
                     std::vector<Vec2>& push) {
    const int cols = hash.cols();
    const int rows = hash.rows();

    for (int cy = 0; cy < rows; ++cy) {
        for (int cx = 0; cx < cols; ++cx) {
            const int cell = cy * cols + cx;
            const size_t begin = hash.cellBegin(cell);
            const size_t end   = hash.cellEnd(cell);
            if (begin == end) continue;

            // Pairs inside the cell.
            for (size_t a = begin; a < end; ++a) {
                const uint32_t i = hash.indexAt(a);
                const Vec2 ip = hash.positionAt(a);
                for (size_t b = a + 1u; b < end; ++b) {
                    const uint32_t j = hash.indexAt(b);
                    const Vec2 f = separationForce(ip, hash.positionAt(b), i, j,
                                                   radiusSq, invRadius);
                    push[i] += f;
                    push[j] -= f;
                }
            }

            // Pairs across each cell boundary, counted once.
            for (const auto& off : kHalfNeighbours) {
                const int nx = cx + off[0];
                const int ny = cy + off[1];
                if (nx < 0 || ny < 0 || nx >= cols || ny >= rows) continue;

                const int other = ny * cols + nx;
                const size_t obegin = hash.cellBegin(other);
                const size_t oend   = hash.cellEnd(other);
                if (obegin == oend) continue;

                for (size_t a = begin; a < end; ++a) {
                    const uint32_t i = hash.indexAt(a);
                    const Vec2 ip = hash.positionAt(a);
                    for (size_t b = obegin; b < oend; ++b) {
                        const uint32_t j = hash.indexAt(b);
                        const Vec2 f = separationForce(
                            ip, hash.positionAt(b), i, j, radiusSq, invRadius);
                        push[i] += f;
                        push[j] -= f;
                    }
                }
            }
        }
    }
}

// Fallback for a separation radius wider than one hash cell, where the
// half-neighbourhood above would miss pairs: query per entity instead.
void accumulateQueries(const EnemyPool& pool, const SpatialHash& hash,
                       uint32_t n, float radius, float radiusSq,
                       float invRadius, std::vector<Vec2>& push) {
    for (uint32_t i = 0; i < n; ++i) {
        const Vec2 self = pool.prevPosition[i];
        Vec2 sum{0.0f, 0.0f};
        hash.forEachInRadius(self, radius, [&](uint32_t j, Vec2 jPos) {
            if (j == i) return;
            sum += separationForce(self, jPos, i, j, radiusSq, invRadius);
        });
        push[i] = sum;
    }
}

// Slides along a wall rather than stopping dead against it: try the whole
// move, then each axis alone. An enemy already inside geometry (spawned there,
// or shoved there before this existed) is allowed to move freely so it can get
// out, rather than being sealed in.
inline Vec2 resolveWalls(const LevelMap& map, Vec2 from, Vec2 to) {
    int cx = 0;
    int cy = 0;
    if (!map.grid.worldToCell(from, cx, cy) || !map.isWalkable(cx, cy)) {
        return to;
    }
    const auto open = [&](Vec2 p) {
        int tx = 0;
        int ty = 0;
        return map.grid.worldToCell(p, tx, ty) && map.isWalkable(tx, ty);
    };
    if (open(to)) return to;

    const Vec2 alongX{to.x, from.y};
    if (open(alongX)) return alongX;
    const Vec2 alongY{from.x, to.y};
    if (open(alongY)) return alongY;
    return from;
}

}  // namespace

void updateMovement(EnemyPool& pool,
                    const LevelMap& map,
                    const FlowField& field,
                    const SpatialHash& hash,
                    float dt,
                    const MovementParams& params,
                    std::vector<Vec2>& pushScratch) {
    const uint32_t n = pool.count();
    if (n == 0u) return;

    for (uint32_t i = 0; i < n; ++i) {
        pool.prevPosition[i] = pool.position[i];
    }

    const float radius   = params.separationRadius;
    const float radiusSq = radius * radius;
    const float invRadius = (radius > 0.0f) ? (1.0f / radius) : 0.0f;

    if (params.naiveSeparation) {
        accumulateNaive(pool, n, radiusSq, invRadius, pushScratch);
    } else if (radius > hash.cellSize()) {
        accumulateQueries(pool, hash, n, radius, radiusSq, invRadius,
                          pushScratch);
    } else {
        for (uint32_t i = 0; i < n; ++i) pushScratch[i] = Vec2{0.0f, 0.0f};
        accumulatePairs(hash, radiusSq, invRadius, pushScratch);
    }

    // Taken once: the loop subscripts it with the raw type byte rather than
    // calling through statsFor per enemy per tick.
    //
    // These two constants were briefly denormalised into per-entity arrays on
    // the theory that a 36-byte indexed struct load was breaking the
    // contiguous walk. It measured as no change at all - the cost is touching
    // the data, not the indirection - so the version that carries no extra
    // state and no extra work in swap-remove is the one that stayed.
    const EnemyStats* stats = enemyStatsTable();

    for (uint32_t i = 0; i < n; ++i) {
        const Vec2 self = pool.prevPosition[i];
        const EnemyStats& s = stats[pool.type[i]];

        const Vec2 flow = field.sample(self);
        Vec2 desired = flow * pool.speed[i];

        // Weaving kinds slide sideways across their own path. The oscillation
        // is perpendicular to the FLOW, not to the current velocity, or the
        // separation force would feed back into it and the weave would grow
        // until the enemy walked into a wall.
        const float weave = s.weave;
        if (weave != 0.0f) {
            float ph = pool.phase[i] + dt * kWeaveRadiansPerSecond;
            if (ph > kTwoPi) ph -= kTwoPi;
            pool.phase[i] = ph;
            if (lengthSq(flow) > 1e-6f) {
                const Vec2 perp{-flow.y, flow.x};
                desired += perp * (std::sin(ph) * weave);
            }
        }

        // crowd < 1 makes a kind ignore its neighbours and pile up, which is
        // what turns Swarmers into a tide instead of a queue.
        const Vec2 velocity =
            desired + pushScratch[i] * (params.separationStrength * s.crowding);
        pool.velocity[i] = velocity;
        pool.position[i] = resolveWalls(map, self, self + velocity * dt);
    }
}

}  // namespace ls
