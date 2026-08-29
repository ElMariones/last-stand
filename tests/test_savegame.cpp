#include <doctest/doctest.h>
#include "persist/SaveGame.h"

#include <cstdio>
#include <string>

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
    return d;
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

TEST_CASE("deserialize rejects a version mismatch") {
    SaveData in = makeData();
    in.version = 999u;
    const auto bytes = ls::serialize(in);

    SaveData out;
    CHECK_FALSE(ls::deserialize(bytes.data(), bytes.size(), out));
    CHECK(out.version == 999u);   // hint left behind
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
