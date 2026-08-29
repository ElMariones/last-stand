#pragma once
#include <cstdint>
#include <vector>

#include "math/Vec2.h"
#include "sim/Grid.h"

namespace ls {

struct LevelMap {
    Grid                 grid{64, 36, 20.0f};
    std::vector<uint8_t> walkable;      // 1 = walkable, 0 = wall
    std::vector<int>     spawnCells;
    std::vector<Vec2>    hardpoints;    // authored turret placement positions
    int                  baseCell = 0;

    bool isWalkable(int cx, int cy) const;
    bool isWalkableIndex(int idx) const;
    Vec2 baseCenter() const;
};

// 64x36 open field, two wall blocks leaving a five-cell chokepoint at
// rows 16-20, spawns down column 1, base at cell (58, 18).
LevelMap makeM1Map();

}  // namespace ls
