#include "persist/SaveGame.h"

#include <cstdio>
#include <string>
#include <unistd.h>

namespace ls {

namespace {
// 'L','S','T','D' as a little-endian uint32.
constexpr uint32_t kMagic = 0x4454534Cu;
constexpr uint32_t kVersion = 1u;
}  // namespace

std::vector<uint8_t> serialize(const SaveData& data) {
    std::vector<uint8_t> out;
    out.reserve(4u * (3u + kNodeCount + 2u * kSaveLevels));

    const auto put = [&](uint32_t v) {
        out.push_back(static_cast<uint8_t>(v & 0xFFu));
        out.push_back(static_cast<uint8_t>((v >> 8u) & 0xFFu));
        out.push_back(static_cast<uint8_t>((v >> 16u) & 0xFFu));
        out.push_back(static_cast<uint8_t>((v >> 24u) & 0xFFu));
    };

    put(kMagic);
    put(data.version);
    put(data.scrap);
    for (const uint32_t v : data.nodeLevels) put(v);
    for (const uint32_t v : data.bestKills) put(v);
    for (const uint32_t v : data.clearCounts) put(v);
    return out;
}

bool deserialize(const uint8_t* bytes, size_t size, SaveData& out) {
    const size_t expected = 4u * (3u + kNodeCount + 2u * kSaveLevels);
    if (bytes == nullptr || size < expected) return false;

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
    if (version != kVersion) {
        out.version = version;   // leave a hint but fail
        return false;
    }

    SaveData d;
    d.version = version;
    d.scrap = get();
    for (auto& v : d.nodeLevels) v = get();
    for (auto& v : d.bestKills) v = get();
    for (auto& v : d.clearCounts) v = get();
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
    bytes.reserve(4u * (3u + kNodeCount + 2u * kSaveLevels) + 1u);
    uint8_t buf[4096];
    size_t n = 0u;
    while ((n = std::fread(buf, 1u, sizeof(buf), f)) > 0u) {
        bytes.insert(bytes.end(), buf, buf + n);
    }
    std::fclose(f);

    return deserialize(bytes.data(), bytes.size(), out);
}

}  // namespace ls
