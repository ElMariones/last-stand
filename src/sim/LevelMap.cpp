#include "sim/LevelMap.h"

namespace ls {

bool LevelMap::isWalkable(int cx, int cy) const {
    if (!grid.inBounds(cx, cy)) return false;
    return walkable[static_cast<size_t>(grid.index(cx, cy))] != 0u;
}

bool LevelMap::isWalkableIndex(int idx) const {
    if (idx < 0 || idx >= grid.cellCount()) return false;
    return walkable[static_cast<size_t>(idx)] != 0u;
}

Vec2 LevelMap::baseCenter() const {
    return grid.cellCenterAt(baseCell);
}

LevelMap makeM1Map() {
    LevelMap m;
    m.walkable.assign(static_cast<size_t>(m.grid.cellCount()), 1u);

    const auto carveWall = [&m](int x0, int y0, int x1, int y1) {
        for (int cy = y0; cy <= y1; ++cy) {
            for (int cx = x0; cx <= x1; ++cx) {
                m.walkable[static_cast<size_t>(m.grid.index(cx, cy))] = 0u;
            }
        }
    };

    carveWall(22, 2, 28, 15);    // upper block
    carveWall(22, 21, 28, 34);   // lower block
    // Rows 16-20 between them stay open: the chokepoint.

    for (int cy = 4; cy <= 32; ++cy) {
        m.spawnCells.push_back(m.grid.index(1, cy));
    }

    // Four hardpoints forming a kill-box over the funnel behind the
    // chokepoint, covering the approach to the base at (58,18).
    m.hardpoints.push_back(m.grid.cellCenter(34, 12));
    m.hardpoints.push_back(m.grid.cellCenter(34, 22));
    m.hardpoints.push_back(m.grid.cellCenter(46, 10));
    m.hardpoints.push_back(m.grid.cellCenter(46, 26));

    m.baseCell = m.grid.index(58, 18);
    return m;
}

}  // namespace ls
