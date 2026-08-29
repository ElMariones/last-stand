#pragma once
#include <cstdint>

namespace ls {

// Enemy behaviour is a type byte driving a branch (GDD 14.4): never virtual,
// never per-entity indirection. Five kinds at V1; three ship in M4.
enum class EnemyType : uint8_t { Grunt = 0, Runner = 1, Tank = 2 };

struct EnemyStats {
    float hp     = 100.0f;
    float speed  = 40.0f;   // world units per second
};

// Grunt: baseline mass. Runner: fast, frail — punishes rear-only coverage.
// Tank: slow, huge — punishes pure-AoE with no single-target DPS (GDD 6.1).
const EnemyStats& statsFor(EnemyType type);

}  // namespace ls
