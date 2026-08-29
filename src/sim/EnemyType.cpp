#include "sim/EnemyType.h"

namespace ls {

namespace {
constexpr EnemyStats kGrunt{100.0f, 40.0f};
constexpr EnemyStats kRunner{40.0f, 100.0f};
constexpr EnemyStats kTank{2000.0f, 12.0f};
}  // namespace

const EnemyStats& statsFor(EnemyType type) {
    switch (type) {
        case EnemyType::Grunt:  return kGrunt;
        case EnemyType::Runner: return kRunner;
        case EnemyType::Tank:   return kTank;
    }
    return kGrunt;
}

}  // namespace ls
