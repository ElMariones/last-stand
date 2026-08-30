#include "persist/SaveGame.h"

#include <cstdio>
#include <string>
#include <unistd.h>

namespace ls {

namespace {
// 'L','S','T','D' as a little-endian uint32.
constexpr uint32_t kMagic = 0x4454534Cu;
constexpr uint32_t kVersion = 5u;

// Words after magic+version, per version. v1: scrap + nodes + bests + clears.
// v2 adds five settings words plus the packed flag word.
constexpr size_t kBodyWordsV1 = 1u + kNodeCount + 2u * kSaveLevels;
constexpr size_t kSettingsWordsV2 = 6u;
constexpr size_t kDisplayWordsV3  = 3u;   // width, height, ui scale
constexpr size_t kBodyWordsV2 = kBodyWordsV1 + kSettingsWordsV2;
constexpr size_t kBodyWordsV3 = kBodyWordsV2 + kDisplayWordsV3;
constexpr size_t kArsenalWordsV4 = 3u;
constexpr size_t kBodyWordsV4 = kBodyWordsV3 + kArsenalWordsV4;
constexpr size_t kStatsWordsV5 = 8u;
constexpr size_t kBodyWordsV5 = kBodyWordsV4 + kStatsWordsV5;

size_t bytesFor(size_t bodyWords) { return 4u * (2u + bodyWords); }
}  // namespace

std::vector<uint8_t> serialize(const SaveData& data) {
    std::vector<uint8_t> out;
    out.reserve(bytesFor(kBodyWordsV5));

    const auto put = [&](uint32_t v) {
        out.push_back(static_cast<uint8_t>(v & 0xFFu));
        out.push_back(static_cast<uint8_t>((v >> 8u) & 0xFFu));
        out.push_back(static_cast<uint8_t>((v >> 16u) & 0xFFu));
        out.push_back(static_cast<uint8_t>((v >> 24u) & 0xFFu));
    };

    put(kMagic);
    put(kVersion);
    put(data.scrap);
    for (const uint32_t v : data.nodeLevels) put(v);
    for (const uint32_t v : data.bestKills) put(v);
    for (const uint32_t v : data.clearCounts) put(v);

    const Settings& s = data.settings;
    put(static_cast<uint32_t>(s.masterVolume));
    put(static_cast<uint32_t>(s.sfxVolume));
    put(static_cast<uint32_t>(s.musicVolume));
    put(static_cast<uint32_t>(s.shakeScale));
    put(static_cast<uint32_t>(s.defaultTimeScale));
    put(packSettingFlags(s));
    put(static_cast<uint32_t>(s.windowWidth));
    put(static_cast<uint32_t>(s.windowHeight));
    put(static_cast<uint32_t>(s.uiScale));
    for (const uint32_t v : data.arsenal) put(v);
    put(data.stats.runs);
    put(data.stats.victories);
    put(data.stats.kills);
    put(data.stats.scrapEarned);
    put(data.stats.secondsPlayed);
    put(data.stats.bestRunKills);
    put(data.stats.turretsBought);
    put(data.stats.nodesBought);
    return out;
}

bool deserialize(const uint8_t* bytes, size_t size, SaveData& out) {
    if (bytes == nullptr || size < bytesFor(kBodyWordsV1)) return false;

    size_t pos = 0u;
    const auto get = [&]() -> uint32_t {
        const uint32_t v = static_cast<uint32_t>(bytes[pos]) |
                           (static_cast<uint32_t>(bytes[pos + 1u]) << 8u) |
                           (static_cast<uint32_t>(bytes[pos + 2u]) << 16u) |
                           (static_cast<uint32_t>(bytes[pos + 3u]) << 24u);
        pos += 4u;
        return v;
    };

    if (get() != kMagic) return false;

    const uint32_t version = get();
    if (version == 0u || version > kVersion) {
        out.version = version;   // leave a hint but fail
        return false;
    }
    if (version >= 2u && size < bytesFor(kBodyWordsV2)) return false;
    if (version >= 3u && size < bytesFor(kBodyWordsV3)) return false;
    if (version >= 4u && size < bytesFor(kBodyWordsV4)) return false;
    if (version >= 5u && size < bytesFor(kBodyWordsV5)) return false;

    SaveData d;
    d.version = kVersion;        // upgraded on read; the next write is v2
    d.scrap = get();
    for (auto& v : d.nodeLevels) v = get();
    for (auto& v : d.bestKills) v = get();
    for (auto& v : d.clearCounts) v = get();

    if (version >= 2u) {
        d.settings.masterVolume = static_cast<int>(get());
        d.settings.sfxVolume = static_cast<int>(get());
        d.settings.musicVolume = static_cast<int>(get());
        d.settings.shakeScale = static_cast<int>(get());
        d.settings.defaultTimeScale = static_cast<int>(get());
        unpackSettingFlags(get(), d.settings);
    }
    if (version >= 3u) {
        d.settings.windowWidth = static_cast<int>(get());
        d.settings.windowHeight = static_cast<int>(get());
        d.settings.uiScale = static_cast<int>(get());
    }
    if (version >= 4u) {
        for (auto& v : d.arsenal) v = get();
    }
    if (version >= 5u) {
        d.stats.runs = get();
        d.stats.victories = get();
        d.stats.kills = get();
        d.stats.scrapEarned = get();
        d.stats.secondsPlayed = get();
        d.stats.bestRunKills = get();
        d.stats.turretsBought = get();
        d.stats.nodesBought = get();
    }
    // An older file leaves the fields it predates at their defaults, which is
    // the whole point of reading it at all. A pre-v4 save has a zero arsenal,
    // which Session reads as "give this commander their starting four".
    clampSettings(d.settings);

    out = d;
    return true;
}

bool save(const SaveData& data, const char* path) {
    const std::vector<uint8_t> bytes = serialize(data);
    const std::string tmp = std::string(path) + ".tmp";

    std::FILE* f = std::fopen(tmp.c_str(), "wb");
    if (f == nullptr) return false;
    const size_t written =
        std::fwrite(bytes.data(), 1u, bytes.size(), f);
    const int flushOk = (std::fflush(f) == 0) ? 1 : 0;
    const int fsyncOk = fsync(fileno(f));
    const int closeOk = std::fclose(f);
    if (written != bytes.size() || !flushOk || fsyncOk != 0 || closeOk != 0) {
        std::remove(tmp.c_str());
        return false;
    }

    if (std::rename(tmp.c_str(), path) != 0) {
        std::remove(tmp.c_str());
        return false;
    }
    return true;
}

bool load(SaveData& out, const char* path) {
    std::FILE* f = std::fopen(path, "rb");
    if (f == nullptr) return false;

    std::vector<uint8_t> bytes;
    bytes.reserve(bytesFor(kBodyWordsV5) + 1u);
    uint8_t buf[4096];
    size_t n = 0u;
    while ((n = std::fread(buf, 1u, sizeof(buf), f)) > 0u) {
        bytes.insert(bytes.end(), buf, buf + n);
    }
    std::fclose(f);

    return deserialize(bytes.data(), bytes.size(), out);
}

}  // namespace ls
