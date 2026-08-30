#pragma once
#include <cstdint>

#include "math/Rect.h"
#include "math/Vec2.h"
#include "sim/EnemyPool.h"
#include "sim/SpatialHash.h"

namespace ls {

// GDD 12.2. Detail degrades by LOCAL density, not by global count, so a lone
// scout crossing an empty field stays fully articulated while the chokepoint
// reduces to a churning mass. Nobody can see individual legs inside a crowd
// of four hundred, and the system is invisible when it works.
enum class LodTier : uint8_t {
    Full       = 0,   // body, head, two legs on a walk cycle, outline
    Silhouette = 1,   // body, head, bob only
    Shape      = 2,   // one directional shape
};

// Occupancy of the enemy's own spatial-hash cell is the density signal: the
// hash is rebuilt every tick anyway, so this costs one array read per enemy.
// Cells are one separation radius across, so "6 in a cell" really is
// shoulder to shoulder.
struct LodPolicy {
    int silhouetteAt = 2;   // >= this many in the cell -> Silhouette
    int shapeAt      = 6;   // >= this many -> Shape
};

constexpr LodTier tierForLocalCount(int localCount,
                                    const LodPolicy& policy = LodPolicy{}) {
    if (localCount >= policy.shapeAt) return LodTier::Shape;
    if (localCount >= policy.silhouetteAt) return LodTier::Silhouette;
    return LodTier::Full;
}

// Triangles emitted per enemy at each tier. Used by the render benchmark to
// report the primitive count the LOD pass actually saved.
constexpr int trianglesForTier(LodTier tier) {
    switch (tier) {
        case LodTier::Full:       return 7;
        case LodTier::Silhouette: return 3;
        case LodTier::Shape:      return 1;
    }
    return 1;
}

// A body whose centre is outside the viewport by more than `margin` cannot
// contribute a pixel, so it is never bucketed or submitted.
constexpr bool inView(Vec2 p, Rect view) { return contains(view, p); }

constexpr Rect viewportRect(float width, float height, float margin) {
    return Rect{{-margin, -margin}, {width + margin, height + margin}};
}

// The submission cost of one frame's horde, computed without a GPU or even a
// window. `triangles` is what the LOD pass actually submits; `drawn` is what
// the flat one-shape-each path would have. The ratio is the artifact — it is
// what "render cost decoupled from entity count" means in numbers, and it is
// reproducible in a headless benchmark on any machine.
struct LodCensus {
    uint32_t culled    = 0u;
    uint32_t drawn     = 0u;
    uint32_t triangles = 0u;
    uint32_t tier[3]   = {0u, 0u, 0u};
};

inline LodCensus lodCensus(const EnemyPool& enemies, const SpatialHash& hash,
                           Rect view, const LodPolicy& policy = LodPolicy{}) {
    LodCensus c;
    const uint32_t n = enemies.count();
    for (uint32_t i = 0; i < n; ++i) {
        const Vec2 p = enemies.position[i];
        if (!inView(p, view)) {
            ++c.culled;
            continue;
        }
        const LodTier tier =
            tierForLocalCount(hash.countInCell(hash.cellOfPosition(p)), policy);
        ++c.tier[static_cast<size_t>(tier)];
        c.triangles += static_cast<uint32_t>(trianglesForTier(tier));
        ++c.drawn;
    }
    return c;
}

}  // namespace ls
