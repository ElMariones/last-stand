#pragma once
#include "math/Vec2.h"

namespace ls {

// The map is authored at a fixed size (64x36 cells of 20 units = 1280x720)
// and the window is whatever the player dragged it to. The Viewport is the
// one place that reconciles those two: it scales the world to fit, centres
// it, and converts in both directions.
//
// Everything that touches a screen coordinate goes through here — the
// renderer's camera, the mouse, the hardpoint overlay. Before this existed
// the world was drawn at 1:1 into a window of any size, so resizing moved the
// battlefield out from under the cursor and clipped the map.
struct Viewport {
    float zoom = 1.0f;
    Vec2  origin{0.0f, 0.0f};   // screen position of world (0,0)

    Vec2 worldToScreen(Vec2 p) const {
        return Vec2{origin.x + p.x * zoom, origin.y + p.y * zoom};
    }
    Vec2 screenToWorld(Vec2 p) const {
        if (zoom <= 0.0f) return Vec2{0.0f, 0.0f};
        return Vec2{(p.x - origin.x) / zoom, (p.y - origin.y) / zoom};
    }
    float scaled(float length) const { return length * zoom; }
};

// Fits `worldW x worldH` inside `screenW x screenH`, preserving aspect and
// centring the remainder. Uniform scale on both axes: a stretched battlefield
// would make range circles into ellipses and turn "is that in range" into a
// guess.
Viewport fitViewport(float worldW, float worldH, float screenW, float screenH);

// The interface scale a window of this size deserves, before the player's own
// preference multiplies it. A 4K window should not get 720p-sized type.
float uiScaleForWindow(float screenW, float screenH);

}  // namespace ls
