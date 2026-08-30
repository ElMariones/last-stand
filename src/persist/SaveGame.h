#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "gameplay/Settings.h"
#include "gameplay/UpgradeTree.h"

namespace ls {

// Number of level slots in the save (must cover GDD 9.1's eight levels).
constexpr size_t kSaveLevels = 8u;

// Everything that persists between sessions (GDD 14.8): Scrap, upgrade-tree
// levels, per-level bests / clear counts, and the player's options. Small and
// fixed-width so it can be written atomically and versioned.
//
// Version history:
//   1  scrap, node levels, bests, clear counts
//   2  + the settings block (M6)
//   3  + window size and UI scale
struct SaveData {
    uint32_t version = 3u;
    uint32_t scrap = 0u;
    std::array<uint32_t, kNodeCount> nodeLevels{};
    std::array<uint32_t, kSaveLevels> bestKills{};
    std::array<uint32_t, kSaveLevels> clearCounts{};
    Settings settings{};
};

// Fixed little-endian serialization: magic 'LSTD' + version + fields. Always
// writes the current version.
std::vector<uint8_t> serialize(const SaveData& data);

// Returns false (leaving `out` untouched) on bad magic, a version newer than
// this build understands, or truncation. Every OLDER version is accepted and
// upgraded in place, with the fields it predates left at their defaults: a
// save is the player's entire progress, and refusing to read one because the
// game grew a volume slider is the worst possible bug.
// Pure — no filesystem access, so it round-trips in a unit test.
bool deserialize(const uint8_t* bytes, size_t size, SaveData& out);

// Atomic write: serialize to path + ".tmp", fsync, rename over target (GDD
// 14.8 — a corrupted save in this game is the worst possible bug).
bool save(const SaveData& data, const char* path);
bool load(SaveData& out, const char* path);

}  // namespace ls
