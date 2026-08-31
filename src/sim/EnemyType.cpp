#include "sim/EnemyType.h"

namespace ls {

namespace {

// Indexed by EnemyType. Order must match the enum - the hot loops subscript
// this with the raw type byte rather than switching on it.
constexpr EnemyStats kTable[kEnemyTypeCount] = {
    // hp     speed  armor burnRes arrMul regen crowd weave scale
    { 100.0f,  40.0f, 0.0f, 0.00f, 1.0f,  0.0f, 1.0f,  0.0f, 1.00f},  // Grunt
    {  40.0f, 100.0f, 0.0f, 0.00f, 1.0f,  0.0f, 1.0f,  0.0f, 0.85f},  // Runner
    {2000.0f,  12.0f, 3.0f, 0.00f, 1.0f,  0.0f, 1.4f,  0.0f, 1.70f},  // Tank
    {  18.0f, 130.0f, 0.0f, 0.00f, 1.0f,  0.0f, 0.25f, 0.0f, 0.55f},  // Swarmer
    { 900.0f,  30.0f, 7.0f, 0.35f, 1.0f,  0.0f, 1.2f,  0.0f, 1.30f},  // Brute
    { 260.0f,  62.0f, 0.0f, 0.85f, 1.0f,  0.0f, 0.6f, 34.0f, 1.05f},  // Phantom
    {9000.0f,   9.0f, 5.0f, 0.20f, 2.5f, 55.0f, 1.6f,  0.0f, 2.40f},  // Behemoth
};

}  // namespace

const EnemyStats* enemyStatsTable() { return kTable; }

const EnemyStats& statsFor(EnemyType type) {
    const auto i = static_cast<uint8_t>(type);
    return kTable[(i < kEnemyTypeCount) ? i : 0u];
}

}  // namespace ls
