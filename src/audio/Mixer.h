#pragma once
#include <array>
#include <cstdint>

namespace ls {

enum class SoundId : uint8_t {
    Gunshot = 0,
    Cannon,
    Flame,
    Death,
    BaseHit,
    Airstrike,
    Overcharge,
    UiMove,
    UiSelect,
    UiBack,
    Victory,
    Defeat,
    Count,
};

constexpr size_t kSoundCount = static_cast<size_t>(SoundId::Count);

// Higher wins a voice when the cap is reached. The numbers encode a design
// claim: the player must always hear their base being hit and their abilities
// landing, and must never lose the UI's feedback to gunfire.
constexpr int priorityOf(SoundId id) {
    switch (id) {
        case SoundId::BaseHit:    return 100;
        case SoundId::Airstrike:  return 95;
        case SoundId::Victory:
        case SoundId::Defeat:     return 90;
        case SoundId::UiSelect:
        case SoundId::UiBack:
        case SoundId::UiMove:     return 80;
        case SoundId::Overcharge: return 60;
        case SoundId::Cannon:     return 40;
        case SoundId::Flame:      return 20;
        case SoundId::Death:      return 15;
        case SoundId::Gunshot:    return 10;
        case SoundId::Count:      break;
    }
    return 0;
}

struct MixerParams {
    int   maxVoices          = 32;    // GDD 12.4
    float killRateThreshold  = 18.0f; // kills/sec above which deaths aggregate
    float shotRateThreshold  = 24.0f; // shots/sec above which gunfire does
    float killRateForFullBed = 90.0f;
    float shotRateForFullBed = 120.0f;
    float rateSmoothing      = 6.0f;  // exponential, per second
};

// The listening half of the audio system, with no audio device in it. The
// hard problem at 4,000 kills a minute is voice limiting and aggregation
// (GDD 12.4), and both are policy: which requests are granted, and when
// individual pops give way to a continuous bed. Keeping that here means it is
// unit-testable, and the raylib layer becomes a thin obedient shell.
class Mixer {
public:
    explicit Mixer(const MixerParams& params = MixerParams{}) : params_(params) {
        lastPlayed_.fill(kNeverPlayed);
    }

    void setParams(const MixerParams& p) { params_ = p; }

    // Feed once per rendered frame with what the simulation did since the
    // last one.
    void update(float dt, uint32_t kills, uint32_t shots);
    void reset();

    float killRate() const { return killRate_; }
    float shotRate() const { return shotRate_; }

    bool aggregatingDeaths() const {
        return killRate_ >= params_.killRateThreshold;
    }
    bool aggregatingGunfire() const {
        return shotRate_ >= params_.shotRateThreshold;
    }

    // 0..1 gain for the two continuous beds. Zero while below the threshold,
    // so the early game really is individual pops.
    float crowdBedGain() const;
    float gunBedGain() const;

    // Grants or refuses a one-shot. Refuses when the sound is aggregated into
    // a bed, when its own cooldown has not elapsed, or when the voice cap is
    // reached and nothing quieter is worth stealing from.
    bool requestVoice(SoundId id, int activeVoices);

private:
    // Far enough in the past that every sound's first request passes its
    // cooldown. Zero would silently swallow anything triggered in the first
    // few hundred milliseconds of a battle — including the first base hit.
    static constexpr float kNeverPlayed = -1.0e6f;

    float cooldownFor(SoundId id) const;

    MixerParams params_;
    std::array<float, kSoundCount> lastPlayed_{};
    float clock_    = 0.0f;
    float killRate_ = 0.0f;
    float shotRate_ = 0.0f;
};

}  // namespace ls
