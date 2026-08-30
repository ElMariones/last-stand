#include "fx/Juice.h"

#include <cmath>

namespace ls {

namespace {

// A cheap deterministic hash. The shake has to look random and be reproducible
// for the same frame, which rules out both a call to rand() and any state that
// depends on how often offset() happens to be asked.
inline float hashToUnit(uint64_t x) {
    x ^= x >> 33u;
    x *= 0xFF51AFD7ED558CCDULL;
    x ^= x >> 33u;
    x *= 0xC4CEB9FE1A85EC53ULL;
    x ^= x >> 33u;
    // [-1, 1)
    return static_cast<float>(static_cast<int32_t>(x >> 32u)) * 0x1.0p-31f;
}

}  // namespace

void Juice::addShake(float amount) {
    shake_ += amount;
    if (shake_ > params_.shakeMax) shake_ = params_.shakeMax;
}

void Juice::onKills(uint32_t kills) {
    if (kills == 0u) return;
    // Amplitude tracks kills per tick, so the late game rumbles continuously
    // while an early single kill barely registers.
    addShake(params_.shakePerKill * static_cast<float>(kills));
}

void Juice::onBaseHit(uint32_t arrivals) {
    if (arrivals == 0u) return;
    addShake(params_.shakePerHit * static_cast<float>(arrivals));
}

void Juice::onDetonation(float shake) {
    addShake(shake);
    if (!hitstopEnabled_) return;
    hitstopLeft_ += params_.hitstopSeconds;
    if (hitstopLeft_ > params_.hitstopMax) hitstopLeft_ = params_.hitstopMax;
}

void Juice::update(float dt) {
    if (hitstopLeft_ > 0.0f) {
        hitstopLeft_ -= dt;
        if (hitstopLeft_ < 0.0f) hitstopLeft_ = 0.0f;
    }
    shake_ -= params_.shakeDecay * dt;
    if (shake_ < 0.0f) shake_ = 0.0f;
}

void Juice::reset() {
    shake_ = 0.0f;
    hitstopLeft_ = 0.0f;
}

Vec2 Juice::offset(uint64_t frame) const {
    const float a = amplitude();
    if (a <= 0.0f) return Vec2{0.0f, 0.0f};
    return Vec2{hashToUnit(frame * 2u + 1u) * a,
                hashToUnit(frame * 2u + 2u) * a};
}

}  // namespace ls
