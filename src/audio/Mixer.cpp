#include "audio/Mixer.h"

#include <algorithm>
#include <cmath>

namespace ls {

namespace {

// Minimum seconds between two grants of the same sound. Without this, sixty
// machine-gun shots in a second become sixty voices and the mixer is gone
// before aggregation ever gets a chance to help.
float baseCooldown(SoundId id) {
    switch (id) {
        case SoundId::Gunshot:    return 0.055f;
        case SoundId::Death:      return 0.045f;
        case SoundId::Flame:      return 0.18f;
        case SoundId::Cannon:     return 0.09f;
        case SoundId::Overcharge: return 0.20f;
        case SoundId::BaseHit:    return 0.12f;
        case SoundId::Airstrike:  return 0.30f;
        case SoundId::UiMove:     return 0.03f;
        default:                  return 0.0f;
    }
}

float normalised(float value, float lo, float hi) {
    if (hi <= lo) return (value >= hi) ? 1.0f : 0.0f;
    return std::clamp((value - lo) / (hi - lo), 0.0f, 1.0f);
}

}  // namespace

void Mixer::update(float dt, uint32_t kills, uint32_t shots) {
    if (dt <= 0.0f) return;
    clock_ += dt;

    const float instantKills = static_cast<float>(kills) / dt;
    const float instantShots = static_cast<float>(shots) / dt;

    // Exponential smoothing: the bed should swell and fall with the battle,
    // not flicker with a single busy frame.
    const float k = std::clamp(params_.rateSmoothing * dt, 0.0f, 1.0f);
    killRate_ += (instantKills - killRate_) * k;
    shotRate_ += (instantShots - shotRate_) * k;
}

void Mixer::reset() {
    clock_ = 0.0f;
    killRate_ = 0.0f;
    shotRate_ = 0.0f;
    lastPlayed_.fill(kNeverPlayed);
}

float Mixer::crowdBedGain() const {
    if (!aggregatingDeaths()) return 0.0f;
    return normalised(killRate_, params_.killRateThreshold,
                      params_.killRateForFullBed);
}

float Mixer::gunBedGain() const {
    if (!aggregatingGunfire()) return 0.0f;
    return normalised(shotRate_, params_.shotRateThreshold,
                      params_.shotRateForFullBed);
}

float Mixer::cooldownFor(SoundId id) const {
    return baseCooldown(id);
}

bool Mixer::requestVoice(SoundId id, int activeVoices) {
    if (id == SoundId::Count) return false;

    // Aggregated sounds are already being represented by their bed. Playing
    // them individually as well is both louder and less informative.
    if (id == SoundId::Death && aggregatingDeaths()) return false;
    if ((id == SoundId::Gunshot || id == SoundId::Flame) &&
        aggregatingGunfire()) {
        return false;
    }

    const size_t slot = static_cast<size_t>(id);
    const float cooldown = cooldownFor(id);
    if (cooldown > 0.0f && (clock_ - lastPlayed_[slot]) < cooldown) {
        return false;
    }

    if (activeVoices >= params_.maxVoices) {
        // At the cap, only sounds the player must not miss get through, and
        // they take a voice from whatever is quietest.
        if (priorityOf(id) < priorityOf(SoundId::Overcharge)) return false;
    }

    lastPlayed_[slot] = clock_;
    return true;
}

}  // namespace ls
