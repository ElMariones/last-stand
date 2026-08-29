#include "ai/FlowField.h"

#include <array>
#include <functional>
#include <queue>
#include <utility>

namespace ls {

namespace {

constexpr float kCardinal = 1.0f;
constexpr float kDiagonal = 1.41421356f;

struct Neighbour {
    int   dx;
    int   dy;
    float cost;
};

constexpr std::array<Neighbour, 8> kNeighbours{{
    {1, 0, kCardinal},  {-1, 0, kCardinal}, {0, 1, kCardinal},  {0, -1, kCardinal},
    {1, 1, kDiagonal},  {1, -1, kDiagonal}, {-1, 1, kDiagonal}, {-1, -1, kDiagonal},
}};

}  // namespace

void FlowField::build(const LevelMap& map) {
    grid_ = map.grid;
    const size_t cells = static_cast<size_t>(grid_.cellCount());

    cost_.assign(cells, kUnreachable);
    dir_.assign(cells, Vec2{0.0f, 0.0f});

    if (!map.isWalkableIndex(map.baseCell)) return;

    using Entry = std::pair<float, int>;   // (cost, cellIndex)
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;

    cost_[static_cast<size_t>(map.baseCell)] = 0.0f;
    open.emplace(0.0f, map.baseCell);

    while (!open.empty()) {
        const Entry top = open.top();
        open.pop();
        const float c = top.first;
        const int idx = top.second;

        // Stale queue entry from a later, cheaper relaxation.
        if (c > cost_[static_cast<size_t>(idx)]) continue;

        const int cx = idx % grid_.cols();
        const int cy = idx / grid_.cols();

        for (const Neighbour& n : kNeighbours) {
            const int nx = cx + n.dx;
            const int ny = cy + n.dy;
            if (!map.isWalkable(nx, ny)) continue;

            // Forbid cutting a wall corner diagonally. Without this the horde
            // squeezes through the diagonal seams of the chokepoint.
            if (n.dx != 0 && n.dy != 0) {
                if (!map.isWalkable(cx + n.dx, cy)) continue;
                if (!map.isWalkable(cx, cy + n.dy)) continue;
            }

            const size_t ni = static_cast<size_t>(grid_.index(nx, ny));
            const float  candidate = c + n.cost;
            if (candidate < cost_[ni]) {
                cost_[ni] = candidate;
                open.emplace(candidate, grid_.index(nx, ny));
            }
        }
    }

    // Second pass: each reachable cell points at its cheapest walkable
    // neighbour. Done after Dijkstra completes so every cost is final.
    for (int cy = 0; cy < grid_.rows(); ++cy) {
        for (int cx = 0; cx < grid_.cols(); ++cx) {
            const size_t i = static_cast<size_t>(grid_.index(cx, cy));
            if (cost_[i] == kUnreachable || cost_[i] == 0.0f) continue;

            float bestCost = cost_[i];
            int   bestX = -1;
            int   bestY = -1;

            for (const Neighbour& n : kNeighbours) {
                const int nx = cx + n.dx;
                const int ny = cy + n.dy;
                if (!map.isWalkable(nx, ny)) continue;
                if (n.dx != 0 && n.dy != 0) {
                    if (!map.isWalkable(cx + n.dx, cy)) continue;
                    if (!map.isWalkable(cx, cy + n.dy)) continue;
                }
                const size_t ni = static_cast<size_t>(grid_.index(nx, ny));
                if (cost_[ni] < bestCost) {
                    bestCost = cost_[ni];
                    bestX = nx;
                    bestY = ny;
                }
            }

            if (bestX >= 0) {
                dir_[i] = normalized(grid_.cellCenter(bestX, bestY) -
                                     grid_.cellCenter(cx, cy));
            }
        }
    }
}

float FlowField::costAt(int cx, int cy) const {
    if (!grid_.inBounds(cx, cy)) return kUnreachable;
    return cost_[static_cast<size_t>(grid_.index(cx, cy))];
}

Vec2 FlowField::dirAt(int cx, int cy) const {
    if (!grid_.inBounds(cx, cy)) return Vec2{0.0f, 0.0f};
    return dir_[static_cast<size_t>(grid_.index(cx, cy))];
}

bool FlowField::isReachable(int cx, int cy) const {
    return costAt(cx, cy) != kUnreachable;
}

Vec2 FlowField::sample(Vec2 worldPos) const {
    int cx = 0;
    int cy = 0;
    if (!grid_.worldToCell(worldPos, cx, cy)) return Vec2{0.0f, 0.0f};
    return dir_[static_cast<size_t>(grid_.index(cx, cy))];
}

}  // namespace ls
