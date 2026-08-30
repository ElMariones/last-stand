#include "gameplay/Settings.h"

#include <algorithm>

namespace ls {

namespace {
constexpr uint32_t kHitstop       = 1u << 0;
constexpr uint32_t kDamageNumbers = 1u << 1;
constexpr uint32_t kLod           = 1u << 2;
constexpr uint32_t kDebugOverlay  = 1u << 3;
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
}

uint32_t packSettingFlags(const Settings& s) {
    uint32_t bits = 0u;
    if (s.hitstop) bits |= kHitstop;
    if (s.damageNumbers) bits |= kDamageNumbers;
    if (s.levelOfDetail) bits |= kLod;
    if (s.debugOverlay) bits |= kDebugOverlay;
    return bits;
}

void unpackSettingFlags(uint32_t bits, Settings& s) {
    s.hitstop       = (bits & kHitstop) != 0u;
    s.damageNumbers = (bits & kDamageNumbers) != 0u;
    s.levelOfDetail = (bits & kLod) != 0u;
    s.debugOverlay  = (bits & kDebugOverlay) != 0u;
}

}  // namespace ls
