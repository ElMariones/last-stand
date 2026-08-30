#include "audio/AudioEngine.h"

#include <algorithm>
#include <new>
#include <raylib.h>

namespace ls {

namespace {

// Wraps a PCM buffer as a raylib Wave without copying it. The buffer must
// outlive the Sound built from it, which is why Voice owns its samples for
// the lifetime of the engine.
Wave waveFor(std::vector<int16_t>& pcm) {
    Wave w{};
    w.frameCount = static_cast<unsigned int>(pcm.size());
    w.sampleRate = static_cast<unsigned int>(kSampleRate);
    w.sampleSize = 16u;
    w.channels = 1u;
    w.data = pcm.data();
    return w;
}

Sound* asSound(void* p) { return static_cast<Sound*>(p); }

SynthSpec specFor(SoundId id) {
    switch (id) {
        case SoundId::Gunshot:    return specGunshot();
        case SoundId::Cannon:     return specCannon();
        case SoundId::Flame:      return specFlame();
        case SoundId::Death:      return specDeath();
        case SoundId::BaseHit:    return specBaseHit();
        case SoundId::Airstrike:  return specAirstrike();
        case SoundId::Overcharge: return specOvercharge();
        case SoundId::UiMove:     return specUiMove();
        case SoundId::UiSelect:   return specUiSelect();
        case SoundId::UiBack:     return specUiBack();
        case SoundId::Victory:    return specVictory();
        case SoundId::Defeat:     return specDefeat();
        case SoundId::Count:      break;
    }
    return specUiMove();
}

}  // namespace

AudioEngine::~AudioEngine() { shutdown(); }

void AudioEngine::buildVoice(size_t slot, const SynthSpec& spec) {
    Voice& v = voices_[slot];
    renderSynth(spec, v.samples);
    Wave wave = waveFor(v.samples);

    auto* sound = new Sound(LoadSoundFromWave(wave));
    v.sound = sound;
    for (int a = 0; a < kAliasesPerSound; ++a) {
        v.aliases[static_cast<size_t>(a)] = new Sound(LoadSoundAlias(*sound));
    }
}

void AudioEngine::buildBed(size_t index, const SynthSpec& spec) {
    Voice& v = beds_[index];
    renderLoop(spec, v.samples);
    Wave wave = waveFor(v.samples);
    v.sound = new Sound(LoadSoundFromWave(wave));
}

void AudioEngine::init() {
    if (ready_) return;
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return;   // run silent rather than crash

    for (size_t i = 0; i < kSoundCount; ++i) {
        buildVoice(i, specFor(static_cast<SoundId>(i)));
    }
    buildBed(0u, specCrowdBed());
    buildBed(1u, specGunBed());
    ready_ = true;
    applySettings(settings_);
}

void AudioEngine::shutdown() {
    if (!ready_) return;
    stopBeds();

    for (Voice& v : voices_) {
        for (void*& alias : v.aliases) {
            if (alias != nullptr) {
                UnloadSoundAlias(*asSound(alias));
                delete asSound(alias);
                alias = nullptr;
            }
        }
        if (v.sound != nullptr) {
            UnloadSound(*asSound(v.sound));
            delete asSound(v.sound);
            v.sound = nullptr;
        }
    }
    for (Voice& v : beds_) {
        if (v.sound != nullptr) {
            UnloadSound(*asSound(v.sound));
            delete asSound(v.sound);
            v.sound = nullptr;
        }
    }
    CloseAudioDevice();
    ready_ = false;
}

void AudioEngine::applySettings(const Settings& settings) {
    settings_ = settings;
    if (!ready_) return;
    SetMasterVolume(settings_.master());
}

int AudioEngine::countActive() const {
    int active = 0;
    for (const Voice& v : voices_) {
        if (v.sound != nullptr && IsSoundPlaying(*asSound(v.sound))) ++active;
        for (void* const alias : v.aliases) {
            if (alias != nullptr && IsSoundPlaying(*asSound(alias))) ++active;
        }
    }
    return active;
}

void AudioEngine::update(float dt, uint32_t kills, uint32_t shots,
                         bool inBattle) {
    mixer_.update(dt, kills, shots);
    if (!ready_) return;

    activeVoices_ = countActive();

    // The two aggregation beds. Above the rate thresholds these carry the
    // horde instead of thousands of one-shots (GDD 12.4), and their gain
    // tracks the rate, so the audio itself reports progression: early game is
    // individual pops, late game is a roar.
    const float wanted[2] = {inBattle ? mixer_.crowdBedGain() : 0.0f,
                             inBattle ? mixer_.gunBedGain() : 0.0f};
    for (size_t b = 0; b < beds_.size(); ++b) {
        Voice& bed = beds_[b];
        if (bed.sound == nullptr) continue;

        // Ease toward the target so a rate spike does not click.
        bedGain_[b] += (wanted[b] - bedGain_[b]) *
                       std::clamp(4.0f * dt, 0.0f, 1.0f);

        const float gain = bedGain_[b] * settings_.sfx();
        if (gain <= 0.005f) {
            if (IsSoundPlaying(*asSound(bed.sound))) {
                StopSound(*asSound(bed.sound));
            }
            continue;
        }
        SetSoundVolume(*asSound(bed.sound), gain);
        // renderLoop fades both ends, so re-triggering at the seam is
        // inaudible and we get a loop out of a one-shot API.
        if (!IsSoundPlaying(*asSound(bed.sound))) {
            PlaySound(*asSound(bed.sound));
        }
    }
}

void AudioEngine::play(SoundId id) {
    if (id == SoundId::Count) return;
    if (!mixer_.requestVoice(id, activeVoices_)) return;
    if (!ready_) return;

    Voice& v = voices_[static_cast<size_t>(id)];
    if (v.sound == nullptr) return;

    const float gain = settings_.sfx();

    // Round-robin the aliases, preferring one that is not already busy, so a
    // burst of shots overlaps instead of cutting itself off.
    for (int attempt = 0; attempt <= kAliasesPerSound; ++attempt) {
        void* handle = (v.next == 0)
                           ? v.sound
                           : v.aliases[static_cast<size_t>(v.next - 1)];
        v.next = (v.next + 1) % (kAliasesPerSound + 1);
        if (handle == nullptr) continue;
        if (IsSoundPlaying(*asSound(handle)) && attempt < kAliasesPerSound) {
            continue;
        }
        SetSoundVolume(*asSound(handle), gain);
        PlaySound(*asSound(handle));
        ++activeVoices_;
        return;
    }
}

void AudioEngine::stopBeds() {
    if (!ready_) return;
    for (Voice& bed : beds_) {
        if (bed.sound != nullptr && IsSoundPlaying(*asSound(bed.sound))) {
            StopSound(*asSound(bed.sound));
        }
    }
    bedGain_ = {0.0f, 0.0f};
}

}  // namespace ls
