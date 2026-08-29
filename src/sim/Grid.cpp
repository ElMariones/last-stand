#include "sim/Grid.h"

namespace ls {

Grid::Grid(int cols, int rows, float cellSize)
    : cols_(cols), rows_(rows), cellSize_(cellSize) {}

bool Grid::inBounds(int cx, int cy) const {
    return cx >= 0 && cy >= 0 && cx < cols_ && cy < rows_;
}

Vec2 Grid::cellCenter(int cx, int cy) const {
    return Vec2{(static_cast<float>(cx) + 0.5f) * cellSize_,
                (static_cast<float>(cy) + 0.5f) * cellSize_};
}

Vec2 Grid::cellCenterAt(int idx) const {
    return cellCenter(idx % cols_, idx / cols_);
}

bool Grid::worldToCell(Vec2 p, int& cx, int& cy) const {
    if (p.x < 0.0f || p.y < 0.0f) return false;
    const int tx = static_cast<int>(p.x / cellSize_);
    const int ty = static_cast<int>(p.y / cellSize_);
    if (!inBounds(tx, ty)) return false;
    cx = tx;
    cy = ty;
    return true;
}

}  // namespace ls
