#include <doctest/doctest.h>
#include "persist/SaveGame.h"

#include <cstdio>
#include <string>
#include <vector>

using ls::SaveData;

namespace {
SaveData makeData() {
    SaveData d;
    d.scrap = 12345u;
    d.nodeLevels = {3u, 0u, 2u, 1u, 0u, 4u};
    for (size_t i = 0; i < ls::kSaveLevels; ++i) {
        d.bestKills[i] = static_cast<uint32_t>(100u + i);
        d.clearCounts[i] = static_cast<uint32_t>(i);
    }
    d.settings.masterVolume = 42;
    d.settings.sfxVolume = 17;
    d.settings.musicVolume = 3;
    d.settings.shakeScale = 175;
    d.settings.defaultTimeScale = 4;
    d.settings.hitstop = false;
    d.settings.damageNumbers = true;
    d.settings.levelOfDetail = false;
    d.settings.debugOverlay = true;
    d.settings.uiScale = 125;
    d.settings.windowWidth = 1600;
    d.settings.windowHeight = 900;
    d.settings.fullscreen = true;
    d.arsenal = {5u, 2u, 1u};
    return d;
}

// An older payload: the current bytes with the version word rewritten and
// everything that version predates chopped off. Both of these are what a save
// written by an earlier build looks like, and both must still load.
std::vector<uint8_t> makeOldBytes(uint8_t version) {
    auto bytes = ls::serialize(makeData());
    bytes[4] = version;
    const size_t core = 2u + 1u + ls::kNodeCount + 2u * ls::kSaveLevels;
    size_t words = core;
    if (version >= 2u) words += 6u;   // settings block
    if (version >= 3u) words += 3u;   // window size and ui scale
    bytes.resize(4u * words);
    return bytes;
}
}  // namespace

TEST_CASE("serialize then deserialize round-trips every field") {
    const SaveData in = makeData();
    const auto bytes = ls::serialize(in);

    SaveData out;
    REQUIRE(ls::deserialize(bytes.data(), bytes.size(), out));

    CHECK(out.version == in.version);
    CHECK(out.scrap == in.scrap);
    CHECK(out.nodeLevels == in.nodeLevels);
    CHECK(out.bestKills == in.bestKills);
    CHECK(out.clearCounts == in.clearCounts);

    CHECK(out.settings.masterVolume == 42);
    CHECK(out.settings.sfxVolume == 17);
    CHECK(out.settings.musicVolume == 3);
    CHECK(out.settings.shakeScale == 175);
    CHECK(out.settings.defaultTimeScale == 4);
    CHECK_FALSE(out.settings.hitstop);
    CHECK(out.settings.damageNumbers);
    CHECK_FALSE(out.settings.levelOfDetail);
    CHECK(out.settings.debugOverlay);
    CHECK(out.settings.uiScale == in.settings.uiScale);
    CHECK(out.settings.windowWidth == in.settings.windowWidth);
    CHECK(out.settings.fullscreen == in.settings.fullscreen);
    CHECK(out.arsenal == in.arsenal);
}

TEST_CASE("deserialize rejects a bad magic") {
    SaveData in = makeData();
    auto bytes = ls::serialize(in);
    bytes[0] ^= 0xFFu;

    SaveData out;
    CHECK_FALSE(ls::deserialize(bytes.data(), bytes.size(), out));
}

TEST_CASE("deserialize rejects a truncated buffer") {
    const auto bytes = ls::serialize(makeData());
    SaveData out;
    CHECK_FALSE(ls::deserialize(bytes.data(), bytes.size() - 1u, out));
}

TEST_CASE("deserialize rejects an unknown version") {
    auto bytes = ls::serialize(makeData());
    bytes[4] = 99u;               // forge the version word

    SaveData out;
    CHECK_FALSE(ls::deserialize(bytes.data(), bytes.size(), out));
    CHECK(out.version == 99u);    // hint left behind
}

TEST_CASE("a version 1 save still loads, with settings defaulted") {
    // Refusing to read a player's entire progress because the game grew a
    // volume slider is the worst possible bug (GDD 14.8).
    const auto bytes = makeOldBytes(1u);
    const SaveData expected = makeData();
    const ls::Settings defaults;

    SaveData out;
    REQUIRE(ls::deserialize(bytes.data(), bytes.size(), out));

    CHECK(out.scrap == expected.scrap);
    CHECK(out.nodeLevels == expected.nodeLevels);
    CHECK(out.bestKills == expected.bestKills);
    CHECK(out.clearCounts == expected.clearCounts);
    CHECK(out.settings.masterVolume == defaults.masterVolume);
    CHECK(out.settings.hitstop == defaults.hitstop);
    CHECK(out.settings.uiScale == defaults.uiScale);
    // Read as v1, written back as the current version: the upgrade is
    // one-way and silent. Compared against SaveData's own default rather
    // than a literal, so bumping the format does not require editing this.
    CHECK(out.version == SaveData{}.version);
}

TEST_CASE("a version 2 save keeps its settings and defaults only the new ones") {
    const auto bytes = makeOldBytes(2u);
    const SaveData expected = makeData();
    const ls::Settings defaults;

    SaveData out;
    REQUIRE(ls::deserialize(bytes.data(), bytes.size(), out));

    CHECK(out.settings.masterVolume == expected.settings.masterVolume);
    CHECK(out.settings.shakeScale == expected.settings.shakeScale);
    CHECK(out.settings.debugOverlay == expected.settings.debugOverlay);
    // Later fields fall back rather than reading whatever followed.
    CHECK(out.settings.uiScale == defaults.uiScale);
    CHECK(out.settings.windowWidth == defaults.windowWidth);
    CHECK(out.arsenal[0] == 0u);
    CHECK(out.version == SaveData{}.version);
}

TEST_CASE("a version 3 save keeps its display settings and defaults the arsenal") {
    const auto bytes = makeOldBytes(3u);
    const SaveData expected = makeData();

    SaveData out;
    REQUIRE(ls::deserialize(bytes.data(), bytes.size(), out));
    CHECK(out.settings.uiScale == expected.settings.uiScale);
    CHECK(out.settings.windowWidth == expected.settings.windowWidth);
    // A zero arsenal is what Session reads as "hand this commander their
    // starting four machine guns".
    CHECK(out.arsenal[0] == 0u);
    CHECK(out.arsenal[1] == 0u);
}

TEST_CASE("a save from a NEWER build is refused rather than misread") {
    auto bytes = ls::serialize(makeData());
    bytes[4] = 99u;
    SaveData out;
    CHECK_FALSE(ls::deserialize(bytes.data(), bytes.size(), out));
}

TEST_CASE("a v2 file truncated inside the settings block is refused") {
    const auto bytes = ls::serialize(makeData());
    SaveData out;
    CHECK_FALSE(ls::deserialize(bytes.data(), bytes.size() - 8u, out));
}

TEST_CASE("out-of-range settings are clamped on load, never propagated") {
    SaveData in = makeData();
    in.settings.masterVolume = 5000;
    in.settings.shakeScale = -20;
    in.settings.defaultTimeScale = 7;
    const auto bytes = ls::serialize(in);

    SaveData out;
    REQUIRE(ls::deserialize(bytes.data(), bytes.size(), out));
    CHECK(out.settings.masterVolume == 100);
    CHECK(out.settings.shakeScale >= 0);
    // Speeds are a 1..4 range now, so an out-of-range value clamps into it
    // rather than snapping back to the slowest.
    CHECK(out.settings.defaultTimeScale >= 1);
    CHECK(out.settings.defaultTimeScale <= 4);
}

TEST_CASE("save then load returns an identical SaveData") {
    const SaveData in = makeData();
    const std::string path = "/tmp/laststand_test_save.bin";
    std::remove(path.c_str());

    REQUIRE(ls::save(in, path.c_str()));

    SaveData out;
    REQUIRE(ls::load(out, path.c_str()));
    CHECK(out.scrap == in.scrap);
    CHECK(out.nodeLevels == in.nodeLevels);
    CHECK(out.bestKills == in.bestKills);

    std::remove(path.c_str());
}

TEST_CASE("load on a missing file fails") {
    SaveData out;
    CHECK_FALSE(ls::load(out, "/tmp/laststand_definitely_missing.bin"));
}
