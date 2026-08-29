#pragma once
#include <cstdint>
#include <vector>

#include "math/Vec2.h"

namespace ls {

struct SpatialQuery {
    const uint32_t* indices = nullptr;
    uint32_t        count   = 0u;
};

// Uniform-grid spatial hash over the world. Rebuilt each tick with a
// counting sort (tally -> prefix-sum -> scatter: two passes, no allocation
// after construction). Serves turret target acquisition now and neighbour
// separation from M5 (GDD 14.5).
//
// Both the sorted-index array and the query output buffer are sized once at
// construction (maxEntities) and never resized, preserving the
// no-allocation-inside-a-tick invariant.
class SpatialHash {
public:
    SpatialHash(float worldWidth, float worldHeight, float cellSize,
                uint32_t maxEntities);

    void build(const std::vector<Vec2>& positions, uint32_t count);

    // Returns every entity index within `radius` of `center` (exact: cell
    // overlap gates, distance^2 confirms). The returned pointer is valid only
    // until the next query() call, and results are single-threaded sequential.
    SpatialQuery query(const std::vector<Vec2>& positions, Vec2 center,
                       float radius) const;

    int    cols() const { return cols_; }
    int    rows() const { return rows_; }
    float  cellSize() const { return cellSize_; }

private:
    int cellOf(Vec2 p) const;

    float cellSize_;
    int   cols_;
    int   rows_;

    std::vector<int>      counts_;   // cols * rows
    std::vector<int>      starts_;   // cols * rows + 1 (prefix sums)
    std::vector<int>      cursor_;   // cols * rows (scatter cursors)
    std::vector<uint32_t> sorted_;   // maxEntities (build output)
    mutable std::vector<uint32_t> queryBuf_;  // maxEntities (query output)
};

}  // namespace ls
