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
    int                  baseCell = 0;

    // Where auto-deploy starts arranging turrets: roughly the spot the level
    // wants defended. Turrets are not restricted to it — placement is free
    // (the authored hardpoints are gone) — it is only the seed for the "just
    // put them somewhere sensible" button.
    Vec2                 deployAnchor{0.0f, 0.0f};

    bool isWalkable(int cx, int cy) const;
    bool isWalkableIndex(int idx) const;
    Vec2 baseCenter() const;
};

// Where a default defence goes: `count` positions arranged outward from the
// map's deploy anchor, skipping walls and the base. Auto-deploy, the
// benchmark and the golden scenarios all use it, so "the standard defence"
// means one thing everywhere rather than four hand-written lists.
// `spacing` is the minimum gap; the arrangement deliberately spreads wider
// than the legal minimum so a default defence covers ground instead of
// huddling in one spot.
std::vector<Vec2> defaultDeployPositions(const LevelMap& map, int count,
                                         float spacing = 76.0f);

// The eight sectors, in the order the campaign unlocks them. Each is a
// distinct flow problem rather than a reskin (GDD 9.3): where the horde
// compresses, where it thins, and where it leaks are different questions on
// each one.
LevelMap makeOutskirtsMap();    // 1 single lane through a chokepoint
LevelMap makeRefineryMap();     // 2 two lanes converging
LevelMap makeNarrowsMap();      // 3 open field into a hard funnel
LevelMap makeSplitMap();        // 4 two independent paths, no overlap
LevelMap makeSpiralMap();       // 5 one long switchback
LevelMap makeCrossroadsMap();   // 6 four entrances, central base
LevelMap makeGauntletMap();     // 7 three chokepoints in series
LevelMap makeOpenGroundMap();   // 8 no cover at all, pure density

// Back-compat alias for the map M1 shipped with.
LevelMap makeM1Map();

}  // namespace ls
