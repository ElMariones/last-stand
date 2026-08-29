#pragma once
#include "math/Vec2.h"

namespace ls {

// Axis-aligned rectangle. Deferred from M1 (the Grid works in cells and the
// renderer uses raylib's own rectangle call); it finally earns its keep in
// M3 with hardpoint hit-testing on the Prepare screen.
struct Rect {
    Vec2 min{0.0f, 0.0f};
    Vec2 max{0.0f, 0.0f};
};

constexpr bool contains(Rect r, Vec2 p) {
    return p.x >= r.min.x && p.x <= r.max.x && p.y >= r.min.y && p.y <= r.max.y;
}

constexpr float width(Rect r) { return r.max.x - r.min.x; }
constexpr float height(Rect r) { return r.max.y - r.min.y; }

constexpr Rect fromCenter(Vec2 center, float halfW, float halfH) {
    return Rect{{center.x - halfW, center.y - halfH},
                {center.x + halfW, center.y + halfH}};
}

}  // namespace ls
