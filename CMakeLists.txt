cmake_minimum_required(VERSION 3.21)
project(laststand LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

include(FetchContent)

# CMake 4.0 removed compatibility with project files declaring a minimum
# below 3.5. doctest v2.4.11 still declares one, so a stock CMake 4.x
# refuses to configure it. Setting the floor here keeps the dependency
# pinned AND keeps "one command from a clean clone" true, rather than
# pushing a -D flag onto everyone who builds this.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

# --- Dependencies (pinned) -------------------------------------------------
# Warnings are NEVER applied to these targets; raylib does not build clean
# under -Werror and that is not our problem to fix.

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.5
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.11
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(raylib doctest)

# --- First-party warning policy --------------------------------------------
function(laststand_set_warnings target)
    target_compile_options(${target} PRIVATE
        -Wall -Wextra -Werror
        -Wshadow -Wconversion -Wsign-conversion
        -Wno-error=unused-parameter
    )
endfunction()

# Determinism (GDD 14.6). Without this the compiler is free to contract
# a*b+c into a single fused multiply-add, which rounds ONCE instead of twice
# — so the same source produces different floats at -O0 and -O2, and the
# golden-hash regression test would only ever pass in the configuration it
# was recorded in. Turning contraction off costs a little arithmetic
# throughput and buys a simulation that replays across build types.
function(laststand_set_determinism target)
    target_compile_options(${target} PRIVATE -ffp-contract=off)
endfunction()

# --- Core library ----------------------------------------------------------
add_library(laststand_core STATIC
    src/math/Rng.cpp
    src/core/FixedTimestep.cpp
    src/sim/Grid.cpp
    src/sim/LevelMap.cpp
    src/ai/FlowField.cpp
    src/sim/EnemyPool.cpp
    src/sim/MovementSystem.cpp
    src/sim/Base.cpp
    src/sim/EnemyType.cpp
    src/sim/SpatialHash.cpp
    src/sim/Targeting.cpp
    src/sim/CombatSystem.cpp
    src/sim/World.cpp
    src/gameplay/Level.cpp
    src/gameplay/SpawnDirector.cpp
    src/gameplay/UpgradeTree.cpp
    src/gameplay/Progression.cpp
    src/gameplay/Settings.cpp
    src/gameplay/Telemetry.cpp
    src/persist/SaveGame.cpp
    src/audio/Synth.cpp
    src/audio/Mixer.cpp
    src/audio/AudioEngine.cpp
    src/fx/Juice.cpp
    src/fx/Particles.cpp
    src/fx/Corpses.cpp
    src/fx/DamageNumbers.cpp
    src/render/Renderer.cpp
    src/render/Viewport.cpp
    src/render/Icons.cpp
    src/ui/Widgets.cpp
    src/ui/Screens.cpp
    src/app/Cli.cpp
    src/app/Balance.cpp
    src/app/Bench.cpp
    src/app/Session.cpp
)
target_include_directories(laststand_core PUBLIC src)
laststand_set_warnings(laststand_core)
laststand_set_determinism(laststand_core)
target_link_libraries(laststand_core PUBLIC raylib)

# --- Executable ------------------------------------------------------------
add_executable(laststand src/main.cpp)
target_link_libraries(laststand PRIVATE laststand_core)
laststand_set_warnings(laststand)
laststand_set_determinism(laststand)

# --- Tests -----------------------------------------------------------------
enable_testing()
add_executable(laststand_tests
    tests/test_main.cpp
    tests/test_vec2.cpp
    tests/test_rng.cpp
    tests/test_fixedtimestep.cpp
    tests/test_grid.cpp
    tests/test_levelmap.cpp
    tests/test_flowfield.cpp
    tests/test_enemypool.cpp
    tests/test_movement.cpp
    tests/test_base.cpp
    tests/test_world.cpp
    tests/test_cli.cpp
    tests/test_determinism.cpp
    tests/test_spatialhash.cpp
    tests/test_targeting.cpp
    tests/test_combat.cpp
    tests/test_rect.cpp
    tests/test_enemytype.cpp
    tests/test_level.cpp
    tests/test_spawndirector.cpp
    tests/test_upgradetree.cpp
    tests/test_progression.cpp
    tests/test_savegame.cpp
    tests/test_session.cpp
    tests/test_lod.cpp
    tests/test_fx.cpp
    tests/test_audio.cpp
    tests/test_telemetry.cpp
    tests/test_balance.cpp
    tests/test_viewport.cpp
    tests/test_golden.cpp
    tests/test_noalloc.cpp
)
target_link_libraries(laststand_tests PRIVATE laststand_core doctest::doctest)
laststand_set_warnings(laststand_tests)
laststand_set_determinism(laststand_tests)

add_test(NAME unit COMMAND laststand_tests)

add_test(NAME layering
         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_layering.sh)

add_test(NAME glyphs
         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_glyphs.sh)
