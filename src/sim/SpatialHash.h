#pragma once
#include <cstdint>
#include <vector>

#include "math/Vec2.h"

namespace ls {

// Uniform-grid spatial hash over the world. Rebuilt each tick with a
// counting sort (tally -> prefix-sum -> scatter: two passes, no allocation
// after construction). Serves turret target acquisition and, from M5,
// neighbour separation (GDD 14.5).
//
// The only query primitive is forEachInRadius: it writes to no shared buffer,
// so two of them can be nested (DENSEST scores a cluster around each
// candidate) without one clobbering the other. The earlier buffer-returning
// query() did exactly that and silently corrupted DENSEST's candidate list.
class SpatialHash {
public:
    SpatialHash(float worldWidth, float worldHeight, float cellSize,
                uint32_t maxEntities);

    void build(const std::vector<Vec2>& positions, uint32_t count);

    // Invokes fn(uint32_t index) for every entity within `radius` of `center`
    // (exact: cell overlap gates, distance^2 confirms). Visit order is
    // cell-major, ascending index within a cell — deterministic, which is what
    // lets separation sums be reproducible.
    template <typename Fn>
    void forEachInRadius(const std::vector<Vec2>& positions, Vec2 center,
                         float radius, Fn&& fn) const {
        const float rSq = radius * radius;
        const CellRange r = cellRange(center, radius);
        for (int cy = r.minCy; cy <= r.maxCy; ++cy) {
            const int rowBase = cy * cols_;
            for (int cx = r.minCx; cx <= r.maxCx; ++cx) {
                const size_t cell = static_cast<size_t>(rowBase + cx);
                const int begin = starts_[cell];
                const int end   = starts_[cell + 1u];
                for (int k = begin; k < end; ++k) {
                    const uint32_t idx = sorted_[static_cast<size_t>(k)];
                    if (distanceSq(positions[idx], center) <= rSq) fn(idx);
                }
            }
        }
    }

    int    cols() const { return cols_; }
    int    rows() const { return rows_; }
    float  cellSize() const { return cellSize_; }

    // Entities currently binned into `cell` (row-major). Used by the renderer's
    // density-driven LOD, which needs occupancy, not membership.
    int    countInCell(int cell) const {
        return starts_[static_cast<size_t>(cell) + 1u] -
               starts_[static_cast<size_t>(cell)];
    }
    int    cellOfPosition(Vec2 p) const { return cellOf(p); }

private:
    struct CellRange {
        int minCx;
        int maxCx;
        int minCy;
        int maxCy;
    };

    int       cellOf(Vec2 p) const;
    CellRange cellRange(Vec2 center, float radius) const;

    float cellSize_;
    int   cols_;
    int   rows_;

    std::vector<int>      counts_;   // cols * rows
    std::vector<int>      starts_;   // cols * rows + 1 (prefix sums)
    std::vector<int>      cursor_;   // cols * rows (scatter cursors)
    std::vector<uint32_t> sorted_;   // maxEntities (build output)
};

}  // namespace ls
