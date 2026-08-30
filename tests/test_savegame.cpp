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
    return d;
}

// A version 1 payload: the v2 bytes with the version word rewritten and the
// settings block chopped off. This is what a save written before M6 looks
// like, and it must still load.
std::vector<uint8_t> makeV1Bytes() {
    auto bytes = ls::serialize(makeData());
    bytes[4] = 1u;
    bytes.resize(4u * (2u + 1u + ls::kNodeCount + 2u * ls::kSaveLevels));
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
    const auto bytes = makeV1Bytes();
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
    // Read as v1, written back as v2: the upgrade is one-way and silent.
    CHECK(out.version == 2u);
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
    CHECK(out.settings.defaultTimeScale == 1);
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
