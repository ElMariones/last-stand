#pragma once
#include <limits>
#include <vector>

#include "math/Vec2.h"
#include "sim/Grid.h"
#include "sim/LevelMap.h"

namespace ls {

// Precomputed pathing for an entire map. One Dijkstra pass from the base at
// load time; per-agent movement is then an O(1) grid sample. This is the
// decision that makes tens of thousands of agents tractable — per-agent A*
// at that count is not slow, it is impossible.
class FlowField {
public:
    static constexpr float kUnreachable = std::numeric_limits<float>::infinity();

    void build(const LevelMap& map);

    Vec2  sample(Vec2 worldPos) const;   // {0,0} if outside or unreachable
    float costAt(int cx, int cy) const;
    Vec2  dirAt(int cx, int cy) const;
    bool  isReachable(int cx, int cy) const;

private:
    Grid               grid_{1, 1, 1.0f};
    std::vector<float> cost_;
    std::vector<Vec2>  dir_;
};

}  // namespace ls
