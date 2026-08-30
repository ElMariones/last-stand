#include "gameplay/Settings.h"

#include <algorithm>

namespace ls {

namespace {
constexpr uint32_t kHitstop       = 1u << 0;
constexpr uint32_t kDamageNumbers = 1u << 1;
constexpr uint32_t kLod           = 1u << 2;
constexpr uint32_t kDebugOverlay  = 1u << 3;
constexpr uint32_t kFullscreen    = 1u << 4;

// The window sizes the options screen offers. Anything else a window manager
// hands us is kept as-is; this is a convenience list, not a restriction.
constexpr int kSizes[][2] = {
    {1280, 720}, {1440, 810}, {1600, 900}, {1920, 1080}, {2560, 1440},
};
}  // namespace

void clampSettings(Settings& s) {
    s.masterVolume = std::clamp(s.masterVolume, 0, 100);
    s.sfxVolume    = std::clamp(s.sfxVolume, 0, 100);
    s.musicVolume  = std::clamp(s.musicVolume, 0, 100);
    s.shakeScale   = std::clamp(s.shakeScale, 0, 200);
    // The only three the battle loop knows how to run.
    if (s.defaultTimeScale != 1 && s.defaultTimeScale != 2 &&
        s.defaultTimeScale != 4) {
        s.defaultTimeScale = 1;
    }
    s.uiScale = std::clamp(s.uiScale, 75, 150);
    // A window smaller than this cannot fit the report panel.
    s.windowWidth = std::clamp(s.windowWidth, 1024, 7680);
    s.windowHeight = std::clamp(s.windowHeight, 600, 4320);
}

int windowSizeCount() {
    return static_cast<int>(sizeof(kSizes) / sizeof(kSizes[0]));
}

void windowSizeAt(int index, int& width, int& height) {
    const int i = std::clamp(index, 0, windowSizeCount() - 1);
    width = kSizes[i][0];
    height = kSizes[i][1];
}

int nearestWindowSize(int width, int height) {
    int best = 0;
    long bestDelta = -1;
    for (int i = 0; i < windowSizeCount(); ++i) {
        const long dw = kSizes[i][0] - width;
        const long dh = kSizes[i][1] - height;
        const long delta = dw * dw + dh * dh;
        if (bestDelta < 0 || delta < bestDelta) {
            bestDelta = delta;
            best = i;
        }
    }
    return best;
}

uint32_t packSettingFlags(const Settings& s) {
    uint32_t bits = 0u;
    if (s.hitstop) bits |= kHitstop;
    if (s.damageNumbers) bits |= kDamageNumbers;
    if (s.levelOfDetail) bits |= kLod;
    if (s.debugOverlay) bits |= kDebugOverlay;
    if (s.fullscreen) bits |= kFullscreen;
    return bits;
}

void unpackSettingFlags(uint32_t bits, Settings& s) {
    s.hitstop       = (bits & kHitstop) != 0u;
    s.damageNumbers = (bits & kDamageNumbers) != 0u;
    s.levelOfDetail = (bits & kLod) != 0u;
    s.debugOverlay  = (bits & kDebugOverlay) != 0u;
    s.fullscreen    = (bits & kFullscreen) != 0u;
}

}  // namespace ls
