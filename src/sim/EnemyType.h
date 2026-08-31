#pragma once
#include <cstdint>

namespace ls {

// Enemy behaviour is a type byte driving a table lookup (GDD 14.4): never
// virtual, never per-entity indirection. Seven kinds; three shipped in M4 and
// four arrived with the branching campaign.
enum class EnemyType : uint8_t {
    Grunt    = 0,
    Runner   = 1,
    Tank     = 2,
    Swarmer  = 3,
    Brute    = 4,
    Phantom  = 5,
    Behemoth = 6,
};

constexpr uint8_t kEnemyTypeCount = 7u;

// Everything that makes one kind play differently from another. Flat data, so
// the hot loops index an array rather than branching on a switch.
struct EnemyStats {
    float hp     = 100.0f;
    float speed  = 40.0f;   // world units per second

    // ATTACK / DEFENCE ------------------------------------------------------
    // Flat damage subtracted from every hit before it lands. This is what
    // makes "buy more damage" stop being the universal answer: against armour
    // it is the damage PER SHOT that matters, so a fast weak gun is worthless
    // and Armor Piercing finally has a job. Never total immunity - a hit
    // always lands at least kMinDamageFraction of its face value.
    float armor       = 0.0f;
    // 0..1 fraction of Burn damage ignored.
    float burnResist  = 0.0f;
    // Damage dealt to the base on arrival, as a multiple of remaining health.
    float arrivalMult = 1.0f;
    // Self-repair, hp per second. Turns "enough DPS eventually" into "enough
    // DPS right now", which is the only way a late sector stays a threat once
    // the player owns every node.
    float regen       = 0.0f;

    // MOVEMENT --------------------------------------------------------------
    // Scales the separation force. Below 1 the kind packs tight instead of
    // spreading out, which reads on screen as a swarm rather than a queue.
    float crowding    = 1.0f;
    // Lateral oscillation, world units per second, perpendicular to the flow.
    // A weaving target slides out from under a turret that has already
    // committed its aim.
    float weave       = 0.0f;

    // PRESENTATION ----------------------------------------------------------
    float scale       = 1.0f;   // body size multiplier
};

// A hit never lands for less than this fraction of its face value, however
// heavy the armour. Total immunity is not a difficulty curve, it is a wall.
constexpr float kMinDamageFraction = 0.15f;

// The table itself, indexable by the raw type byte. Hot loops (movement,
// burn) take this once and subscript it.
const EnemyStats* enemyStatsTable();

// Grunt: baseline mass. Runner: fast, frail - punishes rear-only coverage.
// Tank: slow, huge, lightly armoured - punishes pure-AoE with no single-target
// DPS (GDD 6.1). Swarmer: a tide of nearly-free bodies that packs tight.
// Brute: heavily armoured, so per-shot damage is what kills it. Phantom:
// weaves and shrugs off fire. Behemoth: regenerates, and what it does to the
// base if it lands is the whole reason to stop it.
const EnemyStats& statsFor(EnemyType type);

}  // namespace ls
