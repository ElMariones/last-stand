#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "audio/Mixer.h"
#include "audio/Synth.h"
#include "gameplay/Settings.h"

namespace ls {

// The raylib half: it owns the device and the sample data, and it obeys the
// Mixer. All the interesting decisions — voice limiting, cooldowns,
// aggregation — live in Mixer, which has no audio device in it and is fully
// unit-tested. This class is the shell around them.
//
// Every sound is synthesised at startup (see Synth.h). Nothing is loaded from
// disk, so a missing asset cannot break the build or the game.
class AudioEngine {
public:
    // Voices per sound. A machine gun needs several in flight at once; a
    // battle-end sting needs one.
    static constexpr int kAliasesPerSound = 6;

    AudioEngine() = default;
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    // Safe to call when there is no audio device: every method then does
    // nothing, and the game runs silent rather than crashing.
    void init();
    void shutdown();
    bool ready() const { return ready_; }

    void applySettings(const Settings& settings);

    // Once per rendered frame, with what the simulation did since the last.
    void update(float dt, uint32_t kills, uint32_t shots, bool inBattle);

    void play(SoundId id);
    void stopBeds();

    const Mixer& mixer() const { return mixer_; }
    int activeVoices() const { return activeVoices_; }

private:
    struct Voice {
        std::vector<int16_t> samples;   // owned sample data, alive for the run
        void*                sound = nullptr;   // Sound*, hidden from callers
        std::array<void*, kAliasesPerSound> aliases{};
        int                  next = 0;
    };

    void buildVoice(size_t slot, const SynthSpec& spec);
    void buildBed(size_t index, const SynthSpec& spec);
    int  countActive() const;

    std::array<Voice, kSoundCount> voices_{};
    // 0 crowd, 1 gunfire, 2 music. The first two are aggregation (GDD 12.4);
    // the third is the ambient floor under everything.
    std::array<Voice, 3> beds_{};
    std::array<float, 3> bedGain_{{0.0f, 0.0f, 0.0f}};

    Settings settings_;
    Mixer    mixer_;
    int      activeVoices_ = 0;
    bool     ready_ = false;
};

}  // namespace ls
