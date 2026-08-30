#include "audio/Synth.h"

#include <algorithm>
#include <cmath>

#include "math/Rng.h"

namespace ls {

namespace {

constexpr float kTwoPi = 6.28318530717958647692f;

float envelopeAt(float t, float total, float attack, float release) {
    if (t < attack) return (attack > 0.0f) ? (t / attack) : 1.0f;
    const float releaseStart = std::max(attack, total - release);
    if (t >= releaseStart) {
        const float span = total - releaseStart;
        if (span <= 0.0f) return 0.0f;
        const float k = 1.0f - ((t - releaseStart) / span);
        return k * k;   // quadratic tail reads as a natural decay
    }
    return 1.0f;
}

int16_t toPcm(float v) {
    // Soft clip, then quantise. Nothing in the mix is allowed to wrap.
    v = std::tanh(v);
    const float scaled = v * 32000.0f;
    return static_cast<int16_t>(std::lround(
        std::clamp(scaled, -32000.0f, 32000.0f)));
}

// Renders the raw oscillator + noise + one-pole lowpass, applying `envelope`
// per sample. Shared by the one-shot and loop paths.
template <typename EnvFn>
void renderCore(const SynthSpec& spec, std::vector<int16_t>& out,
                int sampleRate, EnvFn&& envelope) {
    const int total = std::max(
        1, static_cast<int>(spec.seconds * static_cast<float>(sampleRate)));
    out.resize(static_cast<size_t>(total));

    Pcg32 rng{spec.seed};
    const float invRate = 1.0f / static_cast<float>(sampleRate);
    const float ratio = (spec.startFreq > 0.0f)
                            ? (spec.endFreq / spec.startFreq)
                            : 1.0f;

    float phase = 0.0f;
    float filtered = 0.0f;
    const float cutoff = std::clamp(spec.lowpass, 0.0025f, 1.0f);

    for (int i = 0; i < total; ++i) {
        const float t = static_cast<float>(i) * invRate;
        const float u = static_cast<float>(i) / static_cast<float>(total);

        // Exponential glide reads as a pitch drop rather than a linear ramp.
        const float freq = spec.startFreq * std::pow(ratio, u);
        phase += kTwoPi * freq * invRate;
        if (phase > kTwoPi) phase -= kTwoPi;

        const float tone = std::sin(phase);
        const float noise = rng.nextRange(-1.0f, 1.0f);
        const float raw = tone * (1.0f - spec.noiseMix) + noise * spec.noiseMix;

        filtered += (raw - filtered) * cutoff;

        const float env = envelope(t, u);
        out[static_cast<size_t>(i)] =
            toPcm(filtered * env * spec.amplitude * spec.drive);
    }
}

}  // namespace

void renderSynth(const SynthSpec& spec, std::vector<int16_t>& out,
                 int sampleRate) {
    renderCore(spec, out, sampleRate, [&](float t, float) {
        return envelopeAt(t, spec.seconds, spec.attack, spec.release);
    });
    if (!out.empty()) {
        // Guarantee silence at both ends: a stolen or restarted voice must
        // never click.
        out.front() = 0;
        out.back() = 0;
    }
}

void renderLoop(const SynthSpec& spec, std::vector<int16_t>& out,
                int sampleRate) {
    // A short crossfade window at each end hides the seam when the buffer
    // wraps, which is what lets a bed run continuously under the battle.
    renderCore(spec, out, sampleRate, [](float, float u) {
        constexpr float kFade = 0.04f;
        if (u < kFade) return u / kFade;
        if (u > 1.0f - kFade) return (1.0f - u) / kFade;
        return 1.0f;
    });
}

// --- the voices ------------------------------------------------------------
// Tuned by ear against GDD 12.1's palette description: cold, industrial, dry.

SynthSpec specGunshot() {
    SynthSpec s;
    s.seconds = 0.07f;  s.startFreq = 900.0f; s.endFreq = 200.0f;
    s.noiseMix = 0.92f; s.attack = 0.001f;    s.release = 0.06f;
    s.lowpass = 0.55f;  s.drive = 1.3f;       s.amplitude = 0.35f;
    s.seed = 11u;
    return s;
}

SynthSpec specCannon() {
    SynthSpec s;
    s.seconds = 0.42f;  s.startFreq = 190.0f; s.endFreq = 42.0f;
    s.noiseMix = 0.55f; s.attack = 0.002f;    s.release = 0.34f;
    s.lowpass = 0.18f;  s.drive = 1.8f;       s.amplitude = 0.72f;
    s.seed = 22u;
    return s;
}

SynthSpec specFlame() {
    SynthSpec s;
    s.seconds = 0.34f;  s.startFreq = 260.0f; s.endFreq = 150.0f;
    s.noiseMix = 1.0f;  s.attack = 0.05f;     s.release = 0.24f;
    s.lowpass = 0.30f;  s.drive = 1.1f;       s.amplitude = 0.30f;
    s.seed = 33u;
    return s;
}

SynthSpec specDeath() {
    SynthSpec s;
    s.seconds = 0.11f;  s.startFreq = 420.0f; s.endFreq = 90.0f;
    s.noiseMix = 0.65f; s.attack = 0.002f;    s.release = 0.09f;
    s.lowpass = 0.42f;  s.drive = 1.0f;       s.amplitude = 0.26f;
    s.seed = 44u;
    return s;
}

SynthSpec specBaseHit() {
    SynthSpec s;
    s.seconds = 0.55f;  s.startFreq = 120.0f; s.endFreq = 30.0f;
    s.noiseMix = 0.30f; s.attack = 0.003f;    s.release = 0.45f;
    s.lowpass = 0.12f;  s.drive = 2.0f;       s.amplitude = 0.85f;
    s.seed = 55u;
    return s;
}

SynthSpec specAirstrike() {
    SynthSpec s;
    s.seconds = 0.85f;  s.startFreq = 320.0f; s.endFreq = 28.0f;
    s.noiseMix = 0.70f; s.attack = 0.004f;    s.release = 0.70f;
    s.lowpass = 0.16f;  s.drive = 2.2f;       s.amplitude = 0.95f;
    s.seed = 66u;
    return s;
}

SynthSpec specOvercharge() {
    SynthSpec s;
    s.seconds = 0.30f;  s.startFreq = 240.0f; s.endFreq = 1500.0f;
    s.noiseMix = 0.20f; s.attack = 0.02f;     s.release = 0.16f;
    s.lowpass = 0.75f;  s.drive = 1.2f;       s.amplitude = 0.55f;
    s.seed = 77u;
    return s;
}

SynthSpec specUiMove() {
    SynthSpec s;
    s.seconds = 0.045f; s.startFreq = 620.0f; s.endFreq = 620.0f;
    s.noiseMix = 0.05f; s.attack = 0.002f;    s.release = 0.038f;
    s.lowpass = 0.85f;  s.drive = 1.0f;       s.amplitude = 0.22f;
    s.seed = 88u;
    return s;
}

SynthSpec specUiSelect() {
    SynthSpec s;
    s.seconds = 0.13f;  s.startFreq = 520.0f; s.endFreq = 1040.0f;
    s.noiseMix = 0.02f; s.attack = 0.003f;    s.release = 0.11f;
    s.lowpass = 0.9f;   s.drive = 1.0f;       s.amplitude = 0.32f;
    s.seed = 99u;
    return s;
}

SynthSpec specUiBack() {
    SynthSpec s;
    s.seconds = 0.13f;  s.startFreq = 520.0f; s.endFreq = 260.0f;
    s.noiseMix = 0.02f; s.attack = 0.003f;    s.release = 0.11f;
    s.lowpass = 0.9f;   s.drive = 1.0f;       s.amplitude = 0.30f;
    s.seed = 101u;
    return s;
}

SynthSpec specVictory() {
    SynthSpec s;
    s.seconds = 0.9f;   s.startFreq = 330.0f; s.endFreq = 660.0f;
    s.noiseMix = 0.02f; s.attack = 0.01f;     s.release = 0.7f;
    s.lowpass = 0.8f;   s.drive = 1.1f;       s.amplitude = 0.5f;
    s.seed = 111u;
    return s;
}

SynthSpec specDefeat() {
    SynthSpec s;
    s.seconds = 1.2f;   s.startFreq = 210.0f; s.endFreq = 52.0f;
    s.noiseMix = 0.10f; s.attack = 0.02f;     s.release = 1.0f;
    s.lowpass = 0.22f;  s.drive = 1.4f;       s.amplitude = 0.6f;
    s.seed = 121u;
    return s;
}

// The two aggregation beds. Above the kill/shot rate thresholds these replace
// the one-shots entirely, which is the only way 4,000 kills a minute is
// listenable (GDD 12.4).
SynthSpec specCrowdBed() {
    SynthSpec s;
    s.seconds = 1.6f;   s.startFreq = 70.0f;  s.endFreq = 55.0f;
    s.noiseMix = 0.80f; s.lowpass = 0.10f;    s.drive = 1.5f;
    s.amplitude = 0.55f;
    s.seed = 131u;
    return s;
}

SynthSpec specGunBed() {
    SynthSpec s;
    s.seconds = 1.1f;   s.startFreq = 600.0f; s.endFreq = 520.0f;
    s.noiseMix = 0.95f; s.lowpass = 0.50f;    s.drive = 1.1f;
    s.amplitude = 0.30f;
    s.seed = 141u;
    return s;
}

}  // namespace ls
