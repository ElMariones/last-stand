#include "sim/LevelMap.h"

#include <cmath>

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

Vec2 slideAlongWalls(const LevelMap& map, Vec2 from, Vec2 to) {
    int cx = 0;
    int cy = 0;
    if (!map.grid.worldToCell(from, cx, cy) || !map.isWalkable(cx, cy)) {
        return to;
    }
    const auto open = [&](Vec2 p) {
        int tx = 0;
        int ty = 0;
        return map.grid.worldToCell(p, tx, ty) && map.isWalkable(tx, ty);
    };
    if (open(to)) return to;

    const Vec2 alongX{to.x, from.y};
    if (open(alongX)) return alongX;
    const Vec2 alongY{from.x, to.y};
    if (open(alongY)) return alongY;
    return from;
}

namespace {

// A tiny authoring vocabulary. Eight maps written by hand need it; without it
// they are eight pages of index arithmetic and one of them is wrong.
struct Builder {
    LevelMap map;

    explicit Builder() {
        map.walkable.assign(static_cast<size_t>(map.grid.cellCount()), 1u);
    }

    void wall(int x0, int y0, int x1, int y1) {
        for (int cy = y0; cy <= y1; ++cy) {
            for (int cx = x0; cx <= x1; ++cx) {
                if (!map.grid.inBounds(cx, cy)) continue;
                map.walkable[static_cast<size_t>(map.grid.index(cx, cy))] = 0u;
            }
        }
    }

    void spawnColumn(int x, int y0, int y1) {
        for (int cy = y0; cy <= y1; ++cy) {
            if (!map.grid.inBounds(x, cy)) continue;
            map.spawnCells.push_back(map.grid.index(x, cy));
        }
    }

    void spawnRow(int y, int x0, int x1) {
        for (int cx = x0; cx <= x1; ++cx) {
            if (!map.grid.inBounds(cx, y)) continue;
            map.spawnCells.push_back(map.grid.index(cx, y));
        }
    }

    LevelMap finish(int baseX, int baseY, int anchorX, int anchorY) {
        map.baseCell = map.grid.index(baseX, baseY);
        map.deployAnchor = map.grid.cellCenter(anchorX, anchorY);
        // The base tile itself is always walkable, or the flow field has no
        // source and nothing can path anywhere.
        map.walkable[static_cast<size_t>(map.baseCell)] = 1u;

        // Drop any spawn a wall was later laid over. A spawn inside geometry
        // produces an enemy that cannot path anywhere, stands still for the
        // whole battle, and stops the victory condition from ever firing —
        // and it is invisible when reading an author's wall list.
        std::vector<int> reachable;
        reachable.reserve(map.spawnCells.size());
        for (const int cell : map.spawnCells) {
            if (map.isWalkableIndex(cell)) reachable.push_back(cell);
        }
        map.spawnCells = reachable;
        return map;
    }
};

}  // namespace

// TIER 1 -- THE OUTSKIRTS. One lane, one chokepoint. Teaches turret, target, base.
LevelMap makeOutskirtsMap() {
    Builder b;
    b.wall(22, 2, 28, 15);
    b.wall(22, 21, 28, 34);
    b.spawnColumn(1, 4, 32);
    return b.finish(58, 18, 40, 18);
}

// TIER 2 -- REFINERY GATE. Two lanes converging on the approach: punishes stacking
// everything in one place, which is the whole lesson of the second sector.
LevelMap makeRefineryMap() {
    Builder b;
    b.wall(18, 15, 46, 20);         // the spine that splits north from south
    b.wall(30, 2, 34, 12);
    b.wall(30, 23, 34, 33);
    b.spawnColumn(1, 4, 13);
    b.spawnColumn(1, 22, 31);
    return b.finish(58, 18, 50, 17);
}

// TIER 2 -- THE NARROWS. Open approach into a hard funnel: the first sector where
// area damage placed well beats raw single-target damage.
LevelMap makeNarrowsMap() {
    Builder b;
    b.wall(34, 0, 40, 16);
    b.wall(34, 20, 40, 35);
    b.wall(44, 8, 48, 13);
    b.wall(44, 23, 48, 28);
    b.spawnColumn(1, 2, 33);
    return b.finish(59, 18, 44, 18);
}

// TIER 3 -- THE SPLIT. Two paths that never meet, so a single kill-box cannot
// cover both. Coverage, not concentration.
LevelMap makeSplitMap() {
    Builder b;
    b.wall(10, 16, 52, 19);         // full-length divider
    b.wall(20, 4, 24, 14);
    b.wall(38, 21, 42, 31);
    b.spawnColumn(1, 3, 12);
    b.spawnColumn(1, 23, 32);
    return b.finish(58, 13, 46, 11);
}

// TIER 4 -- THE SPIRAL. One long switchback. Everything walks past every turret
// several times, which makes range and fire rate worth more than burst.
LevelMap makeSpiralMap() {
    Builder b;
    b.wall(8, 6, 52, 8);
    b.wall(8, 14, 52, 16);
    b.wall(8, 22, 52, 24);
    b.wall(8, 30, 52, 32);
    // Alternating gaps turn the corridors into one continuous switchback.
    b.wall(52, 6, 56, 16);
    b.wall(8, 14, 12, 24);
    b.wall(52, 22, 56, 32);
    b.spawnRow(1, 10, 50);
    return b.finish(30, 35, 30, 27);
}

// TIER 4 -- CROSSROADS. Four entrances onto a central base. Nothing can be
// defended by facing one way.
LevelMap makeCrossroadsMap() {
    Builder b;
    b.wall(12, 12, 24, 24);
    b.wall(40, 12, 52, 24);
    b.wall(28, 2, 36, 6);
    b.wall(28, 29, 36, 33);
    b.spawnColumn(1, 15, 21);
    b.spawnColumn(62, 15, 21);
    b.spawnRow(0, 28, 36);
    b.spawnRow(35, 28, 36);
    return b.finish(32, 18, 32, 18);
}

// TIER 5 -- THE GAUNTLET. Three chokepoints in series, offset so the horde
// re-forms between them. Compression, release, compression.
LevelMap makeGauntletMap() {
    Builder b;
    b.wall(16, 0, 20, 14);
    b.wall(16, 19, 20, 35);
    b.wall(32, 0, 36, 20);
    b.wall(32, 25, 36, 35);
    b.wall(46, 0, 50, 10);
    b.wall(46, 15, 50, 35);
    b.spawnColumn(1, 4, 32);
    return b.finish(60, 12, 42, 14);
}

// TIER 6 -- OPEN GROUND. No cover, no funnel, nothing to hide behind. The sector
// that is purely about how much you can kill per second.
LevelMap makeOpenGroundMap() {
    Builder b;
    b.spawnColumn(1, 2, 33);
    b.spawnRow(1, 4, 20);
    b.spawnRow(34, 4, 20);
    return b.finish(58, 18, 40, 18);
}

// TIER 2 -- CULVERT. One corridor folded into an S. Everything walks the full
// width of the map three times, so a gun placed on the middle leg is a gun
// that fires three times per enemy.
LevelMap makeCulvertMap() {
    Builder b;
    b.wall(6, 10, 50, 12);
    b.wall(14, 22, 58, 24);
    b.spawnColumn(1, 2, 8);
    return b.finish(58, 30, 30, 17);
}

// TIER 2 -- SCRAPYARD. Scattered cover and no funnel at all: the horde
// arrives on a broad front and the wrecks only break it up. The sector that
// teaches coverage before the campaign starts demanding it.
LevelMap makeScrapyardMap() {
    Builder b;
    b.wall(14, 4, 18, 8);
    b.wall(24, 14, 29, 18);
    b.wall(14, 24, 19, 29);
    b.wall(34, 6, 38, 11);
    b.wall(36, 22, 41, 27);
    b.wall(46, 12, 50, 17);
    b.spawnColumn(1, 2, 33);
    return b.finish(58, 18, 42, 18);
}

// TIER 3 -- FOUNDRY. One enormous block with the base directly behind it. Two
// ways round, both long, and whichever one you leave uncovered is the one
// that gets used.
LevelMap makeFoundryMap() {
    Builder b;
    b.wall(22, 10, 44, 26);
    b.wall(22, 0, 26, 4);
    b.wall(40, 31, 44, 35);
    b.spawnColumn(1, 6, 29);
    return b.finish(58, 18, 50, 18);
}

// TIER 3 -- AQUEDUCT. Three sealed lanes that only meet at the base. Nothing
// you build in one lane helps in another until the very last stretch.
LevelMap makeAqueductMap() {
    Builder b;
    b.wall(10, 11, 54, 13);
    b.wall(10, 22, 54, 24);
    b.spawnColumn(1, 2, 9);
    b.spawnColumn(1, 15, 20);
    b.spawnColumn(1, 26, 33);
    return b.finish(59, 18, 50, 18);
}

// TIER 3 -- THE HOLLOW. A bowl: the approach is wide open and the walls only
// close in behind you. Range matters here more than anywhere before it,
// because there is nowhere the horde has to be.
LevelMap makeHollowMap() {
    Builder b;
    b.wall(0, 0, 30, 3);
    b.wall(0, 32, 30, 35);
    b.wall(36, 0, 63, 6);
    b.wall(36, 29, 63, 35);
    b.wall(30, 14, 34, 21);
    b.spawnColumn(1, 5, 30);
    return b.finish(58, 17, 44, 17);
}

// TIER 4 -- CATACOMBS. Eight staggered walls with the gaps deliberately
// offset, so the horde is never travelling in the direction it was a moment
// ago. Terrible for long sightlines, superb for anything with splash.
LevelMap makeCatacombsMap() {
    Builder b;
    b.wall(10, 0, 13, 12);
    b.wall(10, 18, 13, 35);
    b.wall(20, 6, 23, 29);
    b.wall(30, 0, 33, 16);
    b.wall(30, 22, 33, 35);
    b.wall(40, 4, 43, 31);
    b.wall(50, 0, 53, 10);
    b.wall(50, 16, 53, 35);
    b.spawnColumn(1, 2, 33);
    return b.finish(60, 13, 46, 13);
}

// TIER 4 -- THE PIT. Four corners, base dead centre, and a pinwheel of blocks
// that stops any one emplacement from covering two approaches at once.
LevelMap makePitMap() {
    Builder b;
    b.wall(16, 16, 24, 20);
    b.wall(40, 16, 48, 20);
    b.wall(28, 6, 36, 10);
    b.wall(28, 26, 36, 30);
    b.spawnRow(1, 2, 8);
    b.spawnRow(1, 55, 61);
    b.spawnRow(34, 2, 8);
    b.spawnRow(34, 55, 61);
    return b.finish(32, 18, 32, 13);
}

// TIER 5 -- MEATGRINDER. A four-cell-wide ring corridor around a solid core,
// with the base in a pocket outside the rail. Everything that lives arrives
// having walked the entire circuit under fire.
LevelMap makeMeatgrinderMap() {
    Builder b;
    b.wall(18, 10, 46, 26);      // the core
    b.wall(12, 4, 52, 5);        // top rail
    b.wall(12, 31, 52, 32);      // bottom rail
    b.spawnColumn(1, 12, 24);
    return b.finish(32, 34, 32, 29);
}

// TIER 5 -- CAUSEWAY. Two lanes wide enough to sprint down, joining at a
// bridge four cells across. Everything that has been spread out all campaign
// arrives here at once.
LevelMap makeCausewayMap() {
    Builder b;
    b.wall(0, 16, 40, 20);
    b.wall(44, 12, 63, 14);
    b.wall(44, 22, 63, 24);
    b.spawnColumn(1, 2, 14);
    b.spawnColumn(1, 22, 33);
    return b.finish(60, 18, 48, 18);
}

// TIER 6 -- THE BREACH. Two broken wall lines with their gaps offset, fed
// from three sides. Every lesson in the campaign, asked at once.
LevelMap makeBreachMap() {
    Builder b;
    b.wall(28, 0, 32, 8);
    b.wall(28, 13, 32, 17);
    b.wall(28, 22, 32, 27);
    b.wall(28, 32, 32, 35);
    b.wall(46, 5, 49, 14);
    b.wall(46, 21, 49, 30);
    b.spawnColumn(1, 2, 33);
    b.spawnRow(1, 6, 22);
    b.spawnRow(34, 6, 22);
    return b.finish(59, 18, 40, 18);
}

std::vector<Vec2> defaultDeployPositions(const LevelMap& map, int count,
                                         float spacing) {
    std::vector<Vec2> out;
    if (count <= 0) return out;
    out.reserve(static_cast<size_t>(count));

    const Vec2 base = map.baseCenter();
    const float baseClear = map.grid.cellSize() * 2.2f;

    const auto usable = [&](Vec2 p) {
        int cx = 0;
        int cy = 0;
        if (!map.grid.worldToCell(p, cx, cy)) return false;
        if (!map.isWalkable(cx, cy)) return false;
        if (distanceSq(p, base) < baseClear * baseClear) return false;
        for (const Vec2& taken : out) {
            if (distanceSq(p, taken) < spacing * spacing) return false;
        }
        return true;
    };

    if (usable(map.deployAnchor)) out.push_back(map.deployAnchor);

    // Widening rings around the anchor. Deterministic, and dense enough that
    // eight turrets always find room on every authored map.
    for (float radius = spacing; radius < 420.0f &&
                                 static_cast<int>(out.size()) < count;
         radius += spacing * 0.75f) {
        const int steps = 8 + static_cast<int>(radius / spacing) * 4;
        for (int i = 0; i < steps && static_cast<int>(out.size()) < count; ++i) {
            const float angle = 6.28318530718f *
                                static_cast<float>(i) / static_cast<float>(steps);
            const Vec2 p{map.deployAnchor.x + std::cos(angle) * radius,
                         map.deployAnchor.y + std::sin(angle) * radius};
            if (usable(p)) out.push_back(p);
        }
    }
    return out;
}

LevelMap makeM1Map() { return makeOutskirtsMap(); }

}  // namespace ls
