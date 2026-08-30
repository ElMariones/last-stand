#pragma once
#include <cstdint>
#include <vector>

namespace ls {

constexpr int kSampleRate = 44100;

// Every sound in the game is generated at startup from one of these. There
// are no audio files in the repository: it keeps "one command from a clean
// clone" true, keeps a portfolio repo free of committed binaries, and makes
// the whole mix tunable by editing a constant instead of re-exporting a wav.
// Recorded audio is a drop-in replacement for the buffers this produces.
struct SynthSpec {
    float    seconds    = 0.15f;
    float    startFreq  = 440.0f;   // Hz at t=0
    float    endFreq    = 220.0f;   // Hz at t=end (exponential glide)
    float    noiseMix   = 0.0f;     // 0 pure tone .. 1 pure noise
    float    attack     = 0.005f;   // seconds
    float    release    = 0.10f;    // seconds of decay tail
    float    lowpass    = 1.0f;     // 1 = open, ->0 = darker
    float    drive      = 1.0f;     // pre-clip gain; >1 adds grit
    float    amplitude  = 0.7f;     // 0..1 peak
    uint64_t seed       = 1u;       // noise is deterministic
};

// Renders mono 16-bit PCM into `out`, resizing it. Always starts and ends at
// silence, so a sound can never click when it is triggered or stolen.
void renderSynth(const SynthSpec& spec, std::vector<int16_t>& out,
                 int sampleRate = kSampleRate);

// A seamless loop: the same spec without the attack/release envelope, faded
// across the seam so it can run continuously as an aggregation bed.
void renderLoop(const SynthSpec& spec, std::vector<int16_t>& out,
                int sampleRate = kSampleRate);

// The named voices. Each returns the spec, so they are inspectable and
// testable without touching an audio device.
SynthSpec specGunshot();
SynthSpec specCannon();
SynthSpec specFlame();
SynthSpec specDeath();
SynthSpec specBaseHit();
SynthSpec specAirstrike();
SynthSpec specOvercharge();
SynthSpec specUiMove();
SynthSpec specUiSelect();
SynthSpec specUiBack();
SynthSpec specVictory();
SynthSpec specDefeat();
SynthSpec specCrowdBed();
SynthSpec specGunBed();

}  // namespace ls
