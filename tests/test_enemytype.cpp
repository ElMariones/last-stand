#include <doctest/doctest.h>
#include "sim/EnemyType.h"

using ls::EnemyType;

TEST_CASE("enemy stats match the GDD 6.1 roster") {
    const ls::EnemyStats& grunt = ls::statsFor(EnemyType::Grunt);
    CHECK(grunt.hp == doctest::Approx(100.0f));
    CHECK(grunt.speed == doctest::Approx(40.0f));

    const ls::EnemyStats& runner = ls::statsFor(EnemyType::Runner);
    CHECK(runner.hp == doctest::Approx(40.0f));
    CHECK(runner.speed == doctest::Approx(100.0f));

    const ls::EnemyStats& tank = ls::statsFor(EnemyType::Tank);
    CHECK(tank.hp == doctest::Approx(2000.0f));
    CHECK(tank.speed == doctest::Approx(12.0f));
}
