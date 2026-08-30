#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "audio/Mixer.h"
#include "audio/Synth.h"

using ls::Mixer;
using ls::MixerParams;
using ls::SoundId;
using ls::SynthSpec;

// ---------------------------------------------------------------- synth ----

TEST_CASE("a rendered sound has the requested length") {
    SynthSpec spec = ls::specGunshot();
    std::vector<int16_t> pcm;
    ls::renderSynth(spec, pcm, 44100);
    CHECK(pcm.size() ==
          static_cast<size_t>(spec.seconds * 44100.0f));
}

TEST_CASE("every sound starts and ends in silence") {
    // A voice that is stolen, restarted or looped must never click.
    const SynthSpec specs[] = {
        ls::specGunshot(), ls::specCannon(),   ls::specFlame(),
        ls::specDeath(),   ls::specBaseHit(),  ls::specAirstrike(),
        ls::specOvercharge(), ls::specUiMove(), ls::specUiSelect(),
        ls::specUiBack(),  ls::specVictory(),  ls::specDefeat(),
    };
    std::vector<int16_t> pcm;
    for (const SynthSpec& s : specs) {
        ls::renderSynth(s, pcm);
        REQUIRE(pcm.size() > 2u);
        CHECK(pcm.front() == 0);
        CHECK(pcm.back() == 0);
    }
}

TEST_CASE("nothing in the mix clips") {
    const SynthSpec specs[] = {ls::specAirstrike(), ls::specBaseHit(),
                               ls::specCannon(), ls::specCrowdBed()};
    std::vector<int16_t> pcm;
    for (const SynthSpec& s : specs) {
        ls::renderSynth(s, pcm);
        for (const int16_t v : pcm) {
            REQUIRE(v > -32768);
            REQUIRE(v < 32767);
        }
    }
}

TEST_CASE("synthesis is deterministic for a seed") {
    SynthSpec spec = ls::specFlame();
    std::vector<int16_t> a;
    std::vector<int16_t> b;
    ls::renderSynth(spec, a);
    ls::renderSynth(spec, b);
    CHECK(a == b);

    spec.seed += 1u;
    std::vector<int16_t> c;
    ls::renderSynth(spec, c);
    CHECK(a != c);
}

TEST_CASE("a bed loop fades both ends so the seam is inaudible") {
    std::vector<int16_t> pcm;
    ls::renderLoop(ls::specCrowdBed(), pcm);
    REQUIRE(pcm.size() > 1000u);

    const auto peak = [&](size_t from, size_t to) {
        int16_t m = 0;
        for (size_t i = from; i < to; ++i) {
            m = std::max<int16_t>(m, static_cast<int16_t>(std::abs(pcm[i])));
        }
        return m;
    };
    const size_t n = pcm.size();
    const int16_t edge = std::max(peak(0u, 40u), peak(n - 40u, n));
    const int16_t middle = peak(n / 2u - 400u, n / 2u + 400u);
    CHECK(edge < middle / 4);
}

TEST_CASE("a sound with no glide still renders") {
    SynthSpec spec;
    spec.startFreq = 0.0f;
    spec.endFreq = 0.0f;
    std::vector<int16_t> pcm;
    ls::renderSynth(spec, pcm);
    CHECK(pcm.size() > 0u);
}

// ---------------------------------------------------------------- mixer ----

TEST_CASE("the early game is individual pops, not a bed") {
    Mixer m;
    for (int i = 0; i < 120; ++i) m.update(1.0f / 60.0f, 0u, 0u);
    CHECK_FALSE(m.aggregatingDeaths());
    CHECK_FALSE(m.aggregatingGunfire());
    CHECK(m.crowdBedGain() == doctest::Approx(0.0f));
    CHECK(m.requestVoice(SoundId::Death, 0));
}

TEST_CASE("a heavy kill rate switches deaths over to the bed") {
    Mixer m;
    // 60 kills a second, sustained.
    for (int i = 0; i < 240; ++i) m.update(1.0f / 60.0f, 1u, 0u);
    CHECK(m.aggregatingDeaths());
    CHECK(m.crowdBedGain() > 0.0f);
    // Individual death pops are suppressed: the bed is already saying it.
    CHECK_FALSE(m.requestVoice(SoundId::Death, 0));
}

TEST_CASE("bed gain rises with the rate and saturates at one") {
    Mixer quiet;
    Mixer loud;
    for (int i = 0; i < 300; ++i) {
        quiet.update(1.0f / 60.0f, 1u, 0u);     // 60/s
        loud.update(1.0f / 60.0f, 5u, 0u);      // 300/s
    }
    CHECK(loud.crowdBedGain() > quiet.crowdBedGain());
    CHECK(loud.crowdBedGain() <= 1.0f);
    CHECK(quiet.crowdBedGain() > 0.0f);
}

TEST_CASE("gunfire aggregates on its own threshold") {
    Mixer m;
    for (int i = 0; i < 240; ++i) m.update(1.0f / 60.0f, 0u, 1u);
    CHECK(m.aggregatingGunfire());
    CHECK_FALSE(m.requestVoice(SoundId::Gunshot, 0));
    // ...but the base being hit is never aggregated away.
    CHECK(m.requestVoice(SoundId::BaseHit, 0));
}

TEST_CASE("a per-sound cooldown stops one source eating the mixer") {
    Mixer m;
    m.update(1.0f / 60.0f, 0u, 0u);
    CHECK(m.requestVoice(SoundId::Gunshot, 0));
    CHECK_FALSE(m.requestVoice(SoundId::Gunshot, 0));   // same instant

    for (int i = 0; i < 10; ++i) m.update(1.0f / 60.0f, 0u, 0u);
    CHECK(m.requestVoice(SoundId::Gunshot, 0));
}

TEST_CASE("at the voice cap only what matters gets through") {
    Mixer m;
    const MixerParams p;
    m.update(1.0f / 60.0f, 0u, 0u);

    CHECK_FALSE(m.requestVoice(SoundId::Gunshot, p.maxVoices));
    CHECK_FALSE(m.requestVoice(SoundId::Death, p.maxVoices));
    CHECK(m.requestVoice(SoundId::BaseHit, p.maxVoices));
    CHECK(m.requestVoice(SoundId::Airstrike, p.maxVoices));
    CHECK(m.requestVoice(SoundId::UiSelect, p.maxVoices));
}

TEST_CASE("priorities encode what the player must not miss") {
    CHECK(ls::priorityOf(SoundId::BaseHit) > ls::priorityOf(SoundId::Cannon));
    CHECK(ls::priorityOf(SoundId::Airstrike) > ls::priorityOf(SoundId::Gunshot));
    CHECK(ls::priorityOf(SoundId::UiSelect) > ls::priorityOf(SoundId::Death));
}

TEST_CASE("reset silences the rates and clears the cooldowns") {
    Mixer m;
    for (int i = 0; i < 240; ++i) m.update(1.0f / 60.0f, 2u, 2u);
    REQUIRE(m.aggregatingDeaths());
    m.reset();
    CHECK(m.killRate() == doctest::Approx(0.0f));
    CHECK(m.shotRate() == doctest::Approx(0.0f));
    CHECK_FALSE(m.aggregatingDeaths());
}
