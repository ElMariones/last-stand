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

// What a v1..v5 build actually wrote: EIGHT level slots, in the old sector
// order, followed by whatever blocks that version had grown by then. Built
// word by word rather than by truncating a current serialization - a v6 file
// carries twenty-four slots, so chopping the tail off one produces a byte
// stream no released build ever wrote, and a migration test fed a fake input
// proves nothing.
std::vector<uint8_t> makeOldBytes(uint8_t version) {
    std::vector<uint32_t> w;
    const SaveData d = makeData();

    w.push_back(0x4454534Cu);                  // 'LSTD'
    w.push_back(version);
    w.push_back(d.scrap);
    for (const uint32_t v : d.nodeLevels) w.push_back(v);
    // The legacy block, in the legacy order.
    for (size_t i = 0; i < ls::kLegacyLevels; ++i) {
        w.push_back(static_cast<uint32_t>(100u + i));
    }
    for (size_t i = 0; i < ls::kLegacyLevels; ++i) {
        w.push_back(static_cast<uint32_t>(i));
    }
    if (version >= 2u) {
        w.push_back(static_cast<uint32_t>(d.settings.masterVolume));
        w.push_back(static_cast<uint32_t>(d.settings.sfxVolume));
        w.push_back(static_cast<uint32_t>(d.settings.musicVolume));
        w.push_back(static_cast<uint32_t>(d.settings.shakeScale));
        w.push_back(static_cast<uint32_t>(d.settings.defaultTimeScale));
        w.push_back(ls::packSettingFlags(d.settings));
    }
    if (version >= 3u) {
        w.push_back(static_cast<uint32_t>(d.settings.windowWidth));
        w.push_back(static_cast<uint32_t>(d.settings.windowHeight));
        w.push_back(static_cast<uint32_t>(d.settings.uiScale));
    }
    if (version >= 4u) {
        for (const uint32_t v : d.arsenal) w.push_back(v);
    }
    if (version >= 5u) {
        w.push_back(d.stats.runs);
        w.push_back(d.stats.victories);
        w.push_back(d.stats.kills);
        w.push_back(d.stats.scrapEarned);
        w.push_back(d.stats.secondsPlayed);
        w.push_back(d.stats.bestRunKills);
        w.push_back(d.stats.turretsBought);
        w.push_back(d.stats.nodesBought);
    }

    std::vector<uint8_t> bytes;
    bytes.reserve(4u * w.size());
    for (const uint32_t v : w) {
        bytes.push_back(static_cast<uint8_t>(v & 0xFFu));
        bytes.push_back(static_cast<uint8_t>((v >> 8u) & 0xFFu));
        bytes.push_back(static_cast<uint8_t>((v >> 16u) & 0xFFu));
        bytes.push_back(static_cast<uint8_t>((v >> 24u) & 0xFFu));
    }
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
    CHECK(out.settings.masterVolume == defaults.masterVolume);
    CHECK(out.settings.hitstop == defaults.hitstop);
    CHECK(out.settings.uiScale == defaults.uiScale);
    // Read as v1, written back as the current version: the upgrade is
    // one-way and silent. Compared against SaveData's own default rather
    // than a literal, so bumping the format does not require editing this.
    CHECK(out.version == SaveData{}.version);
}

TEST_CASE("an old save's eight sectors land on the right eighteen") {
    // The campaign stopped being a corridor. "The Split" was slot 3 and is now
    // sector 5; copying the block straight across would have credited those
    // clears to Scrapyard and unlocked a branch the player never earned.
    const auto bytes = makeOldBytes(5u);
    SaveData out;
    REQUIRE(ls::deserialize(bytes.data(), bytes.size(), out));

    const size_t moved[ls::kLegacyLevels] = {0u, 1u, 2u, 5u, 9u, 10u, 13u, 16u};
    for (size_t i = 0; i < ls::kLegacyLevels; ++i) {
        CHECK(out.bestKills[moved[i]] == 100u + i);
        CHECK(out.clearCounts[moved[i]] == i);
    }

    // Every slot the old campaign had no sector for starts empty, so a
    // returning player finds the new branches locked rather than pre-cleared.
    size_t occupied = 0u;
    for (size_t i = 0; i < ls::kSaveLevels; ++i) {
        if (out.bestKills[i] != 0u || out.clearCounts[i] != 0u) ++occupied;
    }
    CHECK(occupied <= ls::kLegacyLevels);
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
