#include "gameplay/SpawnDirector.h"

namespace ls {

void SpawnDirector::update(World& world, const Level& level, float dt) {
    clock_ += dt;
    while (cursor_ < level.schedule.size() &&
           level.schedule[cursor_].timeSeconds <= clock_) {
        world.spawnWave(level.schedule[cursor_].count);
        ++cursor_;
    }
}

}  // namespace ls
