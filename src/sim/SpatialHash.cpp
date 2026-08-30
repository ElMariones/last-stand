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
      sortedPos_(static_cast<size_t>(maxEntities), Vec2{0.0f, 0.0f}) {}

int SpatialHash::cellOf(Vec2 p) const {
    int cx = static_cast<int>(p.x / cellSize_);
    int cy = static_cast<int>(p.y / cellSize_);
    cx = std::clamp(cx, 0, cols_ - 1);
    cy = std::clamp(cy, 0, rows_ - 1);
    return cy * cols_ + cx;
}

SpatialHash::CellRange SpatialHash::cellRange(Vec2 center, float radius) const {
    CellRange r;
    r.minCx = std::clamp(
        static_cast<int>(std::floor((center.x - radius) / cellSize_)), 0,
        cols_ - 1);
    r.maxCx = std::clamp(
        static_cast<int>(std::floor((center.x + radius) / cellSize_)), 0,
        cols_ - 1);
    r.minCy = std::clamp(
        static_cast<int>(std::floor((center.y - radius) / cellSize_)), 0,
        rows_ - 1);
    r.maxCy = std::clamp(
        static_cast<int>(std::floor((center.y + radius) / cellSize_)), 0,
        rows_ - 1);
    return r;
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
        const size_t slot =
            static_cast<size_t>(cursor_[static_cast<size_t>(cell)]++);
        sorted_[slot] = i;
        sortedPos_[slot] = positions[i];
    }
}

}  // namespace ls
