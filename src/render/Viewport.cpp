#include "render/Viewport.h"

#include <algorithm>

namespace ls {

Viewport fitViewport(float worldW, float worldH, float screenW, float screenH) {
    Viewport v;
    if (worldW <= 0.0f || worldH <= 0.0f || screenW <= 0.0f || screenH <= 0.0f) {
        return v;
    }
    v.zoom = std::min(screenW / worldW, screenH / worldH);
    v.origin = Vec2{(screenW - worldW * v.zoom) * 0.5f,
                    (screenH - worldH * v.zoom) * 0.5f};
    return v;
}

float uiScaleForWindow(float screenW, float screenH) {
    // Tied to the same fit as the world, so the HUD and the battlefield grow
    // together and a resize never changes their relative size. Clamped so a
    // very small window keeps readable type and a very large one does not get
    // comically large chrome.
    const float fit = std::min(screenW / 1280.0f, screenH / 720.0f);
    return std::clamp(fit, 0.75f, 2.0f);
}

}  // namespace ls
