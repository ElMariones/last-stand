#include "sim/SpatialHash.h"

#include <algorithm>
#include <cmath>

namespace ls {

SpatialHash::SpatialHash(float worldWidth, float worldHeight, float cellSize,
                         uint32_t maxEntities)
    : cellSize_(cellSize),
      cols_(std::max(1, static_cast<int>(std::ceil(worldWidth / cellSize)))),
      rows_(std::max(1, static_cast<int>(std::ceil(worldHeight / cellSize)))),
      counts_(static_cast<size_t>(cols_) * static_cast<size_t>(rows_), 0),
      starts_(static_cast<size_t>(cols_) * static_cast<size_t>(rows_) + 1, 0),
      cursor_(static_cast<size_t>(cols_) * static_cast<size_t>(rows_), 0),
      sorted_(static_cast<size_t>(maxEntities), 0u),
      queryBuf_(static_cast<size_t>(maxEntities), 0u) {}

int SpatialHash::cellOf(Vec2 p) const {
    int cx = static_cast<int>(p.x / cellSize_);
    int cy = static_cast<int>(p.y / cellSize_);
    cx = std::clamp(cx, 0, cols_ - 1);
    cy = std::clamp(cy, 0, rows_ - 1);
    return cy * cols_ + cx;
}

void SpatialHash::build(const std::vector<Vec2>& positions, uint32_t count) {
    const int cells = cols_ * rows_;

    std::fill(counts_.begin(), counts_.end(), 0);
    for (uint32_t i = 0; i < count; ++i) {
        ++counts_[static_cast<size_t>(cellOf(positions[i]))];
    }

    starts_[0] = 0;
    for (int c = 0; c < cells; ++c) {
        starts_[static_cast<size_t>(c) + 1] =
            starts_[static_cast<size_t>(c)] + counts_[static_cast<size_t>(c)];
    }

    for (int c = 0; c < cells; ++c) {
        cursor_[static_cast<size_t>(c)] = starts_[static_cast<size_t>(c)];
    }

    for (uint32_t i = 0; i < count; ++i) {
        const int cell = cellOf(positions[i]);
        sorted_[static_cast<size_t>(cursor_[static_cast<size_t>(cell)]++)] = i;
    }
}

SpatialQuery SpatialHash::query(const std::vector<Vec2>& positions,
                                Vec2 center, float radius) const {
    const float rSq = radius * radius;
    uint32_t n = 0u;

    int minCx =
        std::clamp(static_cast<int>(std::floor((center.x - radius) / cellSize_)),
                   0, cols_ - 1);
    int maxCx =
        std::clamp(static_cast<int>(std::floor((center.x + radius) / cellSize_)),
                   0, cols_ - 1);
    int minCy =
        std::clamp(static_cast<int>(std::floor((center.y - radius) / cellSize_)),
                   0, rows_ - 1);
    int maxCy =
        std::clamp(static_cast<int>(std::floor((center.y + radius) / cellSize_)),
                   0, rows_ - 1);

    for (int cy = minCy; cy <= maxCy; ++cy) {
        for (int cx = minCx; cx <= maxCx; ++cx) {
            const int cell = cy * cols_ + cx;
            const int begin = starts_[static_cast<size_t>(cell)];
            const int end = starts_[static_cast<size_t>(cell) + 1];
            for (int k = begin; k < end; ++k) {
                const uint32_t idx = sorted_[static_cast<size_t>(k)];
                if (distanceSq(positions[idx], center) <= rSq) {
                    queryBuf_[n++] = idx;
                }
            }
        }
    }

    SpatialQuery q;
    q.indices = queryBuf_.data();
    q.count = n;
    return q;
}

}  // namespace ls
