#pragma once
#include "math/Vec2.h"

namespace ls {

// Uniform cell grid over the play area. Row-major indexing throughout.
class Grid {
public:
    Grid(int cols, int rows, float cellSize);

    int   cols() const { return cols_; }
    int   rows() const { return rows_; }
    float cellSize() const { return cellSize_; }
    int   cellCount() const { return cols_ * rows_; }

    float worldWidth() const { return static_cast<float>(cols_) * cellSize_; }
    float worldHeight() const { return static_cast<float>(rows_) * cellSize_; }

    bool inBounds(int cx, int cy) const;
    int  index(int cx, int cy) const { return cy * cols_ + cx; }

    Vec2 cellCenter(int cx, int cy) const;
    Vec2 cellCenterAt(int idx) const;

    // Returns false and leaves cx/cy untouched when p is outside the grid.
    bool worldToCell(Vec2 p, int& cx, int& cy) const;

private:
    int   cols_;
    int   rows_;
    float cellSize_;
};

}  // namespace ls
