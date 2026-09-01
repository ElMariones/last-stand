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

// Moves `from` toward `to` without ending up inside geometry: the whole step
// if it is clear, otherwise each axis alone, so a body slides along a wall
// rather than stopping dead against it.
//
// Anything that MOVES an enemy has to go through this, not just the movement
// system. The flow field is zero inside a wall, so an enemy that ends up in
// one stands there for the rest of the battle - alive, which means the
// victory condition never fires and the run hangs. Cannon knockback learned
// that the hard way.
//
// An enemy already inside geometry is allowed to move freely, so it can get
// out rather than being sealed in.
Vec2 slideAlongWalls(const LevelMap& map, Vec2 from, Vec2 to);

// Where a default defence goes: `count` positions arranged outward from the
// map's deploy anchor, skipping walls and the base. Auto-deploy, the
// benchmark and the golden scenarios all use it, so "the standard defence"
// means one thing everywhere rather than four hand-written lists.
// `spacing` is the minimum gap; the arrangement deliberately spreads wider
// than the legal minimum so a default defence covers ground instead of
// huddling in one spot.
std::vector<Vec2> defaultDeployPositions(const LevelMap& map, int count,
                                         float spacing = 76.0f);

// The eighteen sectors of the campaign. Each is a distinct flow problem
// rather than a reskin (GDD 9.3): where the horde compresses, where it thins,
// and where it leaks are different questions on each one. Sectors on the same
// difficulty tier are alternatives, not a sequence - which is the whole point
// of a campaign shaped like a graph instead of a corridor.
LevelMap makeOutskirtsMap();     // T1 single lane through a chokepoint
LevelMap makeRefineryMap();      // T2 two lanes converging
LevelMap makeNarrowsMap();       // T2 open field into a hard funnel
LevelMap makeCulvertMap();       // T2 one corridor folded into an S
LevelMap makeScrapyardMap();     // T2 scattered cover, broad front
LevelMap makeSplitMap();         // T3 two independent paths, no overlap
LevelMap makeFoundryMap();       // T3 one huge block, two ways round
LevelMap makeAqueductMap();      // T3 three sealed lanes
LevelMap makeHollowMap();        // T3 an open bowl with nowhere to funnel
LevelMap makeSpiralMap();        // T4 one long switchback
LevelMap makeCrossroadsMap();    // T4 four entrances, central base
LevelMap makeCatacombsMap();     // T4 a maze of offset gaps
LevelMap makePitMap();           // T4 four corners, base dead centre
LevelMap makeGauntletMap();      // T5 three chokepoints in series
LevelMap makeMeatgrinderMap();   // T5 a ring corridor around a solid core
LevelMap makeCausewayMap();      // T5 two fast lanes onto one bridge
LevelMap makeOpenGroundMap();    // T6 no cover at all, pure density
LevelMap makeBreachMap();        // T6 broken wall lines, fed from three sides

// Back-compat alias for the map M1 shipped with.
LevelMap makeM1Map();

}  // namespace ls
