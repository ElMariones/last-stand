# LAST STAND — Milestone 1: Simulation Foundation — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a deterministic, fixed-timestep simulation core in which 100 enemies flow along a precomputed flow field, route around obstacles, reach a base, and destroy it — with a headless benchmark mode and a Stage 0 (deliberately unoptimised) performance baseline recorded.

**Architecture:** A `laststand_core` static library holds all simulation and rendering code; a thin `laststand` executable owns `main()` and the raylib window; a `laststand_tests` binary runs doctest against the core. The simulation advances in fixed 60 Hz ticks decoupled from rendering, which interpolates between the previous and current tick state. Entities live in a fixed-capacity struct-of-arrays pool. Pathing is a flow field computed once per map by Dijkstra from the base, so per-enemy movement is an O(1) grid sample plus a separation force. `sim/` code never includes `render/`, `ui/` or raylib — this is what makes headless benchmarking and determinism possible.

**Tech Stack:** C++20 · raylib 5.5 · doctest 2.4.11 · CMake 3.21+ · Apple Clang (arm64) · CMake FetchContent for all dependencies

**Spec:** `docs/GDD.md` (see §14 C++ Technical Architecture, §15 Performance Strategy, §17 First Milestone)

---

## Deviation from spec, recorded

GDD §17.2 places **Dear ImGui / rlImGui in M1**. This plan **defers ImGui to M2** and uses raylib's built-in `DrawText` for the M1 debug overlay.

**Reason:** rlImGui has no tagged releases, so it cannot be version-pinned via FetchContent the way every other dependency here can. Putting an unpinnable dependency on the critical path of the foundation milestone risks burning a day of week one on build plumbing, and M1's value is the deterministic simulation core, not the tooling. The overlay's job in M1 is to show four numbers, which `DrawText` does. ImGui arrives in M2 as its own task, where it can fail without blocking anything.

**Second, smaller deviation:** GDD §17.2 lists `Rect` alongside `Vec2` and the RNG. No task creates it, because nothing in M1 needs one — the grid works in cells, the base is a circle, and the renderer uses raylib's own rectangle call. It arrives in M2 with turret range and hardpoint hit-testing. Building it now would be speculative.

Everything else in §17.2 is covered by Tasks 1–15 below.

---

## Global Constraints

Copied verbatim from the spec. Every task's requirements implicitly include this section.

- **Language:** C++20. No compiler extensions (`CXX_EXTENSIONS OFF`).
- **Warnings:** `-Wall -Wextra -Werror` on first-party targets only. Never apply these to fetched dependencies — raylib will not compile clean under them.
- **Platform:** macOS arm64 (Apple M5). Code must stay portable enough for the Linux CI added in a later milestone: no Apple-specific headers.
- **Layering (enforced, not aspirational):** nothing under `src/sim/`, `src/math/`, `src/ai/` or `src/core/` may include `raylib.h`, `src/render/*` or `src/ui/*`. Task 14 adds a build-failing check for this.
- **Determinism:** fixed timestep only. Never call `rand()`, `srand()`, `std::random_device`, or read wall-clock time inside `sim/`. All randomness flows through an explicitly seeded `Pcg32` instance passed in by the caller.
- **Allocation:** all pools reserve their full capacity at construction. No heap allocation inside a tick. (The asserting allocator hook is M5; the discipline starts now.)
- **Enemy pool capacity:** `100'000`.
- **Tick rate:** 60 Hz exactly. `kTickSeconds = 1.0 / 60.0`.
- **Max frame clamp:** 0.25 s (15 ticks maximum per frame).
- **Grid:** 64 cols × 36 rows, `cellSize = 20.0f` → 1280×720 world units.
- **Window:** 1280×720, resizable, vsync on.
- **Performance posture for M1:** Stage 0 from GDD §15. Separation is deliberately O(n²). **Do not optimise it.** It is the measured baseline for M5.

---

## File Structure

```
laststand/
├── CMakeLists.txt              Top-level: deps, targets, warning policy
├── src/
│   ├── main.cpp                Entry point, CLI dispatch, raylib window
│   ├── math/
│   │   ├── Vec2.h              2D vector, header-only constexpr
│   │   ├── Rng.h/.cpp          Pcg32 — the only randomness source
│   ├── core/
│   │   └── FixedTimestep.h/.cpp   Accumulator, tick count, interpolation alpha
│   ├── sim/
│   │   ├── Grid.h/.cpp         Cell/world conversions, bounds
│   │   ├── LevelMap.h/.cpp     Walkability, spawn cells, base cell
│   │   ├── EnemyPool.h/.cpp    SoA storage, spawn/kill/clear
│   │   ├── MovementSystem.h/.cpp  Flow sample + Stage 0 separation
│   │   ├── Base.h/.cpp         Base HP, arrival damage
│   │   └── World.h/.cpp        Owns everything, exposes tick()
│   ├── ai/
│   │   └── FlowField.h/.cpp    Dijkstra from base, direction sampling
│   ├── render/
│   │   └── Renderer.h/.cpp     raylib draw calls, interpolation, overlay
│   └── app/
│       ├── Bench.h/.cpp        Headless --bench runner
│       └── Cli.h/.cpp          Argument parsing
├── tests/
│   ├── test_main.cpp           doctest entry
│   ├── test_vec2.cpp
│   ├── test_rng.cpp
│   ├── test_fixedtimestep.cpp
│   ├── test_grid.cpp
│   ├── test_levelmap.cpp
│   ├── test_flowfield.cpp
│   ├── test_enemypool.cpp
│   ├── test_movement.cpp
│   ├── test_base.cpp
│   ├── test_world.cpp
│   ├── test_cli.cpp
│   └── test_determinism.cpp
├── tools/
│   └── check_layering.sh       Fails the build on a layering violation
└── docs/bench/m1.csv           Committed Stage 0 baseline
```

**Responsibility boundaries.** `math/` and `core/` depend on nothing. `sim/` depends on `math/` and `ai/`. `ai/` depends on `math/` and `sim/Grid` + `sim/LevelMap` only. `render/` and `app/` may depend on everything. Dependencies point one direction; Task 14 enforces it.

---

## Task 1: Project skeleton that builds and opens a window

**Files:**
- Create: `CMakeLists.txt`
- Create: `src/main.cpp`
- Create: `tests/test_main.cpp`
- Create: `.gitignore` (already exists — verify contents)

**Interfaces:**
- Consumes: nothing
- Produces: CMake targets `laststand_core` (STATIC), `laststand` (executable), `laststand_tests` (executable). A CMake function `laststand_set_warnings(<target>)` applying `-Wall -Wextra -Werror` — every later task calls this on any new first-party target.

- [ ] **Step 1: Write the top-level CMakeLists.txt**

```cmake
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

# --- Core library ----------------------------------------------------------
# Every subsequent task appends its .cpp files to this list.
add_library(laststand_core STATIC
    src/math/Rng.cpp
)
target_include_directories(laststand_core PUBLIC src)
laststand_set_warnings(laststand_core)

# raylib is linked to core for now; the layering check (Task 14) guarantees
# that sim/ math/ core/ ai/ never actually include it.
target_link_libraries(laststand_core PUBLIC raylib)

# --- Executable ------------------------------------------------------------
add_executable(laststand src/main.cpp)
target_link_libraries(laststand PRIVATE laststand_core)
laststand_set_warnings(laststand)

# --- Tests -----------------------------------------------------------------
enable_testing()
add_executable(laststand_tests
    tests/test_main.cpp
)
target_link_libraries(laststand_tests PRIVATE laststand_core doctest::doctest)
laststand_set_warnings(laststand_tests)

add_test(NAME unit COMMAND laststand_tests)
```

- [ ] **Step 2: Create the placeholder RNG source so the core library has a compilation unit**

Create `src/math/Rng.cpp` containing exactly:

```cpp
// Implementation arrives in Task 3.
namespace ls {}
```

- [ ] **Step 3: Write src/main.cpp**

```cpp
#include <raylib.h>

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "LAST STAND");
    SetTargetFPS(0);  // vsync governs; never cap with a sleep

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(Color{12, 10, 10, 255});
        DrawText("LAST STAND", 40, 40, 40, Color{220, 235, 255, 255});
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
```

- [ ] **Step 4: Write tests/test_main.cpp**

```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

TEST_CASE("test harness runs") {
    CHECK(1 + 1 == 2);
}
```

- [ ] **Step 5: Configure and build**

Run:
```bash
cd /Users/mariolandaburu/Desktop/laststand && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
```
Expected: configure downloads raylib and doctest, build succeeds with **zero warnings**. First configure takes 1–3 minutes.

- [ ] **Step 6: Run the tests and the app**

Run:
```bash
cd /Users/mariolandaburu/Desktop/laststand && ctest --test-dir build --output-on-failure
```
Expected: `100% tests passed, 0 tests failed out of 1`

Run `./build/laststand` — expect a 1280×720 dark window reading "LAST STAND". Close it.

- [ ] **Step 7: Commit**

```bash
cd /Users/mariolandaburu/Desktop/laststand
git add CMakeLists.txt src/ tests/ .gitignore
git commit -m "build: CMake skeleton with pinned raylib 5.5 and doctest 2.4.11

Three targets: laststand_core (static), laststand (exe), laststand_tests.
Warnings (-Wall -Wextra -Werror -Wconversion) applied to first-party
targets only via laststand_set_warnings(); fetched deps are exempt."
```

---

## Task 2: Vec2

**Files:**
- Create: `src/math/Vec2.h`
- Create: `tests/test_vec2.cpp`
- Modify: `CMakeLists.txt` (add `tests/test_vec2.cpp` to `laststand_tests` sources)

**Interfaces:**
- Consumes: nothing
- Produces: `namespace ls { struct Vec2 { float x, y; }; }` plus free functions `operator+`, `operator-`, `operator*(Vec2,float)`, `operator+=`, `operator-=`, `lengthSq(Vec2)->float`, `length(Vec2)->float`, `normalized(Vec2)->Vec2`, `distanceSq(Vec2,Vec2)->float`, `lerp(Vec2,Vec2,float)->Vec2`. `normalized` of a zero vector returns `{0,0}` — every later task relies on that being safe.

- [ ] **Step 1: Write the failing test**

Create `tests/test_vec2.cpp`:

```cpp
#include <doctest/doctest.h>
#include "math/Vec2.h"

using ls::Vec2;

TEST_CASE("Vec2 arithmetic") {
    Vec2 a{3.0f, 4.0f};
    Vec2 b{1.0f, 2.0f};

    CHECK((a + b).x == doctest::Approx(4.0f));
    CHECK((a + b).y == doctest::Approx(6.0f));
    CHECK((a - b).x == doctest::Approx(2.0f));
    CHECK((a * 2.0f).y == doctest::Approx(8.0f));
}

TEST_CASE("Vec2 length") {
    Vec2 a{3.0f, 4.0f};
    CHECK(ls::lengthSq(a) == doctest::Approx(25.0f));
    CHECK(ls::length(a) == doctest::Approx(5.0f));
}

TEST_CASE("Vec2 normalized produces a unit vector") {
    Vec2 n = ls::normalized(Vec2{3.0f, 4.0f});
    CHECK(ls::length(n) == doctest::Approx(1.0f));
    CHECK(n.x == doctest::Approx(0.6f));
}

TEST_CASE("Vec2 normalized of zero is zero, not NaN") {
    Vec2 n = ls::normalized(Vec2{0.0f, 0.0f});
    CHECK(n.x == 0.0f);
    CHECK(n.y == 0.0f);
}

TEST_CASE("Vec2 lerp") {
    Vec2 r = ls::lerp(Vec2{0.0f, 0.0f}, Vec2{10.0f, 20.0f}, 0.5f);
    CHECK(r.x == doctest::Approx(5.0f));
    CHECK(r.y == doctest::Approx(10.0f));
}

TEST_CASE("Vec2 distanceSq") {
    CHECK(ls::distanceSq(Vec2{0.0f, 0.0f}, Vec2{3.0f, 4.0f}) == doctest::Approx(25.0f));
}
```

Add `tests/test_vec2.cpp` to the `laststand_tests` source list in `CMakeLists.txt`.

- [ ] **Step 2: Run test to verify it fails**

Run:
```bash
cd /Users/mariolandaburu/Desktop/laststand && cmake --build build -j 2>&1 | tail -20
```
Expected: FAIL — `fatal error: 'math/Vec2.h' file not found`

- [ ] **Step 3: Write the implementation**

Create `src/math/Vec2.h`:

```cpp
#pragma once
#include <cmath>

namespace ls {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

constexpr Vec2 operator+(Vec2 a, Vec2 b) { return Vec2{a.x + b.x, a.y + b.y}; }
constexpr Vec2 operator-(Vec2 a, Vec2 b) { return Vec2{a.x - b.x, a.y - b.y}; }
constexpr Vec2 operator*(Vec2 a, float s) { return Vec2{a.x * s, a.y * s}; }
constexpr Vec2 operator*(float s, Vec2 a) { return a * s; }
constexpr Vec2 operator-(Vec2 a) { return Vec2{-a.x, -a.y}; }

constexpr Vec2& operator+=(Vec2& a, Vec2 b) { a.x += b.x; a.y += b.y; return a; }
constexpr Vec2& operator-=(Vec2& a, Vec2 b) { a.x -= b.x; a.y -= b.y; return a; }

constexpr float lengthSq(Vec2 a) { return a.x * a.x + a.y * a.y; }
inline float length(Vec2 a) { return std::sqrt(lengthSq(a)); }

constexpr float distanceSq(Vec2 a, Vec2 b) { return lengthSq(b - a); }

// Returns {0,0} for a zero-length input. Callers depend on this being safe.
inline Vec2 normalized(Vec2 a) {
    const float lenSq = lengthSq(a);
    if (lenSq <= 1e-12f) return Vec2{0.0f, 0.0f};
    const float inv = 1.0f / std::sqrt(lenSq);
    return Vec2{a.x * inv, a.y * inv};
}

constexpr Vec2 lerp(Vec2 a, Vec2 b, float t) { return a + (b - a) * t; }

}  // namespace ls
```

- [ ] **Step 4: Run tests to verify they pass**

Run:
```bash
cd /Users/mariolandaburu/Desktop/laststand && cmake --build build -j && ctest --test-dir build --output-on-failure
```
Expected: PASS, all Vec2 test cases green, zero warnings.

- [ ] **Step 5: Commit**

```bash
cd /Users/mariolandaburu/Desktop/laststand
git add src/math/Vec2.h tests/test_vec2.cpp CMakeLists.txt
git commit -m "feat(math): Vec2 with constexpr arithmetic and safe normalize

normalized() returns {0,0} for zero-length input rather than NaN;
movement and separation code both rely on that guarantee."
```

---

## Task 3: PCG32 — the only randomness source

**Files:**
- Create: `src/math/Rng.h`
- Modify: `src/math/Rng.cpp` (replace the Task 1 placeholder)
- Create: `tests/test_rng.cpp`
- Modify: `CMakeLists.txt` (add `tests/test_rng.cpp`)

**Interfaces:**
- Consumes: nothing
- Produces: `namespace ls { class Pcg32 }` with `explicit Pcg32(uint64_t seed, uint64_t stream = 1)`, `uint32_t nextU32()`, `float nextFloat()` returning `[0,1)`, `uint32_t nextBounded(uint32_t bound)` returning `[0,bound)`, `float nextRange(float lo, float hi)`. Every later task that needs randomness takes a `Pcg32&` parameter — no global instance is ever created.

**Note on test strategy:** these are property tests, not a hardcoded golden vector. A golden vector would need constants verified against a reference implementation, which cannot be done by inspection; asserting a wrong constant would be worse than asserting none. The properties below — reproducibility, stream independence, range correctness — are what the simulation actually depends on.

- [ ] **Step 1: Write the failing test**

Create `tests/test_rng.cpp`:

```cpp
#include <doctest/doctest.h>
#include "math/Rng.h"
#include <vector>
#include <cstdint>

using ls::Pcg32;

TEST_CASE("same seed produces the same sequence") {
    Pcg32 a{12345u};
    Pcg32 b{12345u};
    for (int i = 0; i < 1000; ++i) {
        CHECK(a.nextU32() == b.nextU32());
    }
}

TEST_CASE("different seeds diverge") {
    Pcg32 a{1u};
    Pcg32 b{2u};
    int same = 0;
    for (int i = 0; i < 1000; ++i) {
        if (a.nextU32() == b.nextU32()) ++same;
    }
    CHECK(same < 5);
}

TEST_CASE("different streams with the same seed diverge") {
    Pcg32 a{99u, 1u};
    Pcg32 b{99u, 2u};
    int same = 0;
    for (int i = 0; i < 1000; ++i) {
        if (a.nextU32() == b.nextU32()) ++same;
    }
    CHECK(same < 5);
}

TEST_CASE("nextFloat stays in [0,1)") {
    Pcg32 r{7u};
    for (int i = 0; i < 10000; ++i) {
        const float v = r.nextFloat();
        CHECK(v >= 0.0f);
        CHECK(v < 1.0f);
    }
}

TEST_CASE("nextBounded stays in [0,bound) and covers the range") {
    Pcg32 r{7u};
    std::vector<int> hits(6, 0);
    for (int i = 0; i < 60000; ++i) {
        const uint32_t v = r.nextBounded(6u);
        REQUIRE(v < 6u);
        hits[v]++;
    }
    for (int i = 0; i < 6; ++i) {
        CHECK(hits[i] > 8000);   // roughly uniform; 10000 expected
    }
}

TEST_CASE("nextBounded(1) always returns 0") {
    Pcg32 r{7u};
    for (int i = 0; i < 100; ++i) CHECK(r.nextBounded(1u) == 0u);
}

TEST_CASE("nextRange stays within bounds") {
    Pcg32 r{7u};
    for (int i = 0; i < 10000; ++i) {
        const float v = r.nextRange(-5.0f, 5.0f);
        CHECK(v >= -5.0f);
        CHECK(v <= 5.0f);
    }
}
```

Add `tests/test_rng.cpp` to the `laststand_tests` source list.

- [ ] **Step 2: Run test to verify it fails**

Run:
```bash
cd /Users/mariolandaburu/Desktop/laststand && cmake --build build -j 2>&1 | tail -20
```
Expected: FAIL — `'math/Rng.h' file not found`

- [ ] **Step 3: Write the implementation**

Create `src/math/Rng.h`:

```cpp
#pragma once
#include <cstdint>

namespace ls {

// PCG32 (O'Neill, 2014). Chosen over std::mt19937 for a smaller state,
// better statistical quality per byte, and — critically — a specified,
// portable algorithm. std::mt19937's *distributions* are implementation
// defined, which would silently break determinism across toolchains.
class Pcg32 {
public:
    explicit Pcg32(uint64_t seed, uint64_t stream = 1u);

    uint32_t nextU32();
    float    nextFloat();                       // [0, 1)
    uint32_t nextBounded(uint32_t bound);       // [0, bound), unbiased
    float    nextRange(float lo, float hi);     // [lo, hi]

private:
    uint64_t state_;
    uint64_t inc_;
};

}  // namespace ls
```

Replace `src/math/Rng.cpp` entirely:

```cpp
#include "math/Rng.h"

namespace ls {

namespace {
constexpr uint64_t kMultiplier = 6364136223846793005ULL;
}

Pcg32::Pcg32(uint64_t seed, uint64_t stream)
    : state_(0u), inc_((stream << 1u) | 1u) {
    nextU32();
    state_ += seed;
    nextU32();
}

uint32_t Pcg32::nextU32() {
    const uint64_t old = state_;
    state_ = old * kMultiplier + inc_;
    const uint32_t xorshifted = static_cast<uint32_t>(((old >> 18u) ^ old) >> 27u);
    const uint32_t rot = static_cast<uint32_t>(old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((~rot + 1u) & 31u));
}

float Pcg32::nextFloat() {
    // 24 mantissa bits scaled by 2^-24 — exactly representable, never 1.0f.
    return static_cast<float>(nextU32() >> 8u) * 0x1.0p-24f;
}

uint32_t Pcg32::nextBounded(uint32_t bound) {
    if (bound == 0u) return 0u;
    // Rejection sampling removes modulo bias.
    const uint32_t threshold = (~bound + 1u) % bound;
    for (;;) {
        const uint32_t r = nextU32();
        if (r >= threshold) return r % bound;
    }
}

float Pcg32::nextRange(float lo, float hi) {
    return lo + (hi - lo) * nextFloat();
}

}  // namespace ls
```

- [ ] **Step 4: Run tests to verify they pass**

Run:
```bash
cd /Users/mariolandaburu/Desktop/laststand && cmake --build build -j && ctest --test-dir build --output-on-failure
```
Expected: PASS, all RNG cases green.

- [ ] **Step 5: Commit**

```bash
cd /Users/mariolandaburu/Desktop/laststand
git add src/math/Rng.h src/math/Rng.cpp tests/test_rng.cpp CMakeLists.txt
git commit -m "feat(math): PCG32 deterministic RNG

Explicit algorithm rather than std::mt19937 + std::uniform_*, whose
distributions are implementation-defined and would break cross-toolchain
determinism. nextBounded uses rejection sampling to remove modulo bias."
```

---

## Task 4: FixedTimestep accumulator

**Files:**
- Create: `src/core/FixedTimestep.h`
- Create: `src/core/FixedTimestep.cpp`
- Create: `tests/test_fixedtimestep.cpp`
- Modify: `CMakeLists.txt` (add both to their target lists)

**Interfaces:**
- Consumes: nothing
- Produces: `namespace ls { class FixedTimestep }` with `explicit FixedTimestep(double tickRate = 60.0, double maxFrameSeconds = 0.25)`, `int advance(double frameSeconds)` returning how many ticks to run this frame, `double alpha() const` returning the `[0,1)` interpolation factor, `double tickSeconds() const`, `uint64_t totalTicks() const`. `Renderer` (Task 12) consumes `alpha()`; `World::tick` (Task 11) is called `advance()` times per frame.

**Why this is a separate, headless class:** the accumulator is the piece most likely to be subtly wrong (spiral of death, drift, alpha out of range) and it is trivially testable when it does not touch raylib. Testing it through the game loop would be untestable by construction.

- [ ] **Step 1: Write the failing test**

Create `tests/test_fixedtimestep.cpp`:

```cpp
#include <doctest/doctest.h>
#include "core/FixedTimestep.h"

using ls::FixedTimestep;

TEST_CASE("exactly one tick per 1/60s frame") {
    FixedTimestep ts{60.0, 0.25};
    for (int i = 0; i < 100; ++i) {
        CHECK(ts.advance(1.0 / 60.0) == 1);
    }
}

TEST_CASE("a half-tick frame produces no tick, two produce one") {
    FixedTimestep ts{60.0, 0.25};
    CHECK(ts.advance(1.0 / 120.0) == 0);
    CHECK(ts.advance(1.0 / 120.0) == 1);
}

TEST_CASE("a slow frame produces multiple ticks") {
    FixedTimestep ts{60.0, 0.25};
    CHECK(ts.advance(0.05) == 3);   // 0.05 / 0.01667 = 3.0
}

TEST_CASE("a very slow frame is clamped, preventing the spiral of death") {
    FixedTimestep ts{60.0, 0.25};
    // 10 seconds of stall would be 600 ticks; the clamp caps it at 15.
    CHECK(ts.advance(10.0) == 15);
}

TEST_CASE("alpha stays in [0,1)") {
    FixedTimestep ts{60.0, 0.25};
    for (int i = 0; i < 500; ++i) {
        ts.advance(0.00713);   // deliberately not a tick multiple
        CHECK(ts.alpha() >= 0.0);
        CHECK(ts.alpha() < 1.0);
    }
}

TEST_CASE("alpha is zero immediately after an exact tick boundary") {
    FixedTimestep ts{60.0, 0.25};
    ts.advance(1.0 / 60.0);
    CHECK(ts.alpha() == doctest::Approx(0.0).epsilon(1e-9));
}

TEST_CASE("totalTicks accumulates") {
    FixedTimestep ts{60.0, 0.25};
    ts.advance(0.05);
    ts.advance(0.05);
    CHECK(ts.totalTicks() == 6u);
}

TEST_CASE("tickSeconds reflects the configured rate") {
    FixedTimestep ts{60.0, 0.25};
    CHECK(ts.tickSeconds() == doctest::Approx(1.0 / 60.0));
}

TEST_CASE("no drift over many frames") {
    FixedTimestep ts{60.0, 0.25};
    int ticks = 0;
    for (int i = 0; i < 6000; ++i) ticks += ts.advance(1.0 / 60.0);
    CHECK(ticks == 6000);
}
```

Add `tests/test_fixedtimestep.cpp` to `laststand_tests` and `src/core/FixedTimestep.cpp` to `laststand_core`.

- [ ] **Step 2: Run test to verify it fails**

Run:
```bash
cd /Users/mariolandaburu/Desktop/laststand && cmake --build build -j 2>&1 | tail -20
```
Expected: FAIL — `'core/FixedTimestep.h' file not found`

- [ ] **Step 3: Write the implementation**

Create `src/core/FixedTimestep.h`:

```cpp
#pragma once
#include <cstdint>

namespace ls {

// Decouples simulation rate from frame rate. The renderer interpolates
// between the previous and current tick using alpha().
class FixedTimestep {
public:
    explicit FixedTimestep(double tickRate = 60.0, double maxFrameSeconds = 0.25);

    // Feed the real elapsed frame time. Returns how many fixed ticks to run.
    int advance(double frameSeconds);

    double   alpha() const;          // [0,1) progress toward the next tick
    double   tickSeconds() const { return tickSeconds_; }
    uint64_t totalTicks() const { return totalTicks_; }

private:
    double   tickSeconds_;
    double   maxFrameSeconds_;
    double   accumulator_ = 0.0;
    uint64_t totalTicks_  = 0u;
};

}  // namespace ls
```

Create `src/core/FixedTimestep.cpp`:

```cpp
#include "core/FixedTimestep.h"

namespace ls {

FixedTimestep::FixedTimestep(double tickRate, double maxFrameSeconds)
    : tickSeconds_(1.0 / tickRate), maxFrameSeconds_(maxFrameSeconds) {}

int FixedTimestep::advance(double frameSeconds) {
    if (frameSeconds < 0.0) frameSeconds = 0.0;
    // Clamping is what prevents the spiral of death: if a frame stalls,
    // we drop simulation time rather than queue an unbounded tick backlog
    // that guarantees the next frame stalls worse.
    if (frameSeconds > maxFrameSeconds_) frameSeconds = maxFrameSeconds_;

    accumulator_ += frameSeconds;

    int ticks = 0;
    while (accumulator_ >= tickSeconds_) {
        accumulator_ -= tickSeconds_;
        ++ticks;
        ++totalTicks_;
    }
    return ticks;
}

double FixedTimestep::alpha() const {
    return accumulator_ / tickSeconds_;
}

}  // namespace ls
```

- [ ] **Step 4: Run tests to verify they pass**

Run:
```bash
cd /Users/mariolandaburu/Desktop/laststand && cmake --build build -j && ctest --test-dir build --output-on-failure
```
Expected: PASS. If "no drift" fails, the accumulator is being reset instead of decremented.

- [ ] **Step 5: Commit**

```bash
cd /Users/mariolandaburu/Desktop/laststand
git add src/core/FixedTimestep.h src/core/FixedTimestep.cpp tests/test_fixedtimestep.cpp CMakeLists.txt
git commit -m "feat(core): fixed 60Hz timestep with interpolation alpha

Clamps frame time to 0.25s (15 ticks) to prevent the spiral of death.
Kept free of raylib so the accumulator is unit-testable headlessly."
```

---

## Task 5: Grid

**Files:**
- Create: `src/sim/Grid.h`
- Create: `src/sim/Grid.cpp`
- Create: `tests/test_grid.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ls::Vec2` (Task 2)
- Produces: `namespace ls { class Grid }` with `Grid(int cols, int rows, float cellSize)`, `int cols() const`, `int rows() const`, `float cellSize() const`, `int cellCount() const`, `bool inBounds(int cx, int cy) const`, `int index(int cx, int cy) const`, `Vec2 cellCenter(int cx, int cy) const`, `Vec2 cellCenterAt(int index) const`, `bool worldToCell(Vec2 p, int& cx, int& cy) const` returning false when outside the grid, `float worldWidth() const`, `float worldHeight() const`. `LevelMap`, `FlowField` and `Renderer` all consume this.

**Note on `-Wconversion`:** the warning policy set in Task 1 includes `-Wconversion -Wsign-conversion`. Mixed `int`/`float`/`size_t` arithmetic needs explicit `static_cast`. The code below is written to compile clean; keep that discipline in every later task rather than weakening the flags.

- [ ] **Step 1: Write the failing test**

Create `tests/test_grid.cpp`:

```cpp
#include <doctest/doctest.h>
#include "sim/Grid.h"

using ls::Grid;
using ls::Vec2;

TEST_CASE("grid dimensions") {
    Grid g{64, 36, 20.0f};
    CHECK(g.cols() == 64);
    CHECK(g.rows() == 36);
    CHECK(g.cellCount() == 64 * 36);
    CHECK(g.worldWidth() == doctest::Approx(1280.0f));
    CHECK(g.worldHeight() == doctest::Approx(720.0f));
}

TEST_CASE("index is row-major") {
    Grid g{64, 36, 20.0f};
    CHECK(g.index(0, 0) == 0);
    CHECK(g.index(1, 0) == 1);
    CHECK(g.index(0, 1) == 64);
    CHECK(g.index(63, 35) == 64 * 36 - 1);
}

TEST_CASE("inBounds rejects out-of-range cells") {
    Grid g{64, 36, 20.0f};
    CHECK(g.inBounds(0, 0));
    CHECK(g.inBounds(63, 35));
    CHECK_FALSE(g.inBounds(-1, 0));
    CHECK_FALSE(g.inBounds(0, -1));
    CHECK_FALSE(g.inBounds(64, 0));
    CHECK_FALSE(g.inBounds(0, 36));
}

TEST_CASE("cellCenter sits at the middle of the cell") {
    Grid g{64, 36, 20.0f};
    CHECK(g.cellCenter(0, 0).x == doctest::Approx(10.0f));
    CHECK(g.cellCenter(0, 0).y == doctest::Approx(10.0f));
    CHECK(g.cellCenter(3, 2).x == doctest::Approx(70.0f));
    CHECK(g.cellCenter(3, 2).y == doctest::Approx(50.0f));
}

TEST_CASE("cellCenterAt matches cellCenter") {
    Grid g{64, 36, 20.0f};
    const Vec2 a = g.cellCenter(5, 7);
    const Vec2 b = g.cellCenterAt(g.index(5, 7));
    CHECK(a.x == doctest::Approx(b.x));
    CHECK(a.y == doctest::Approx(b.y));
}

TEST_CASE("worldToCell round-trips through cellCenter") {
    Grid g{64, 36, 20.0f};
    int cx = -1, cy = -1;
    REQUIRE(g.worldToCell(g.cellCenter(12, 9), cx, cy));
    CHECK(cx == 12);
    CHECK(cy == 9);
}

TEST_CASE("worldToCell reports failure outside the grid") {
    Grid g{64, 36, 20.0f};
    int cx = 0, cy = 0;
    CHECK_FALSE(g.worldToCell(Vec2{-1.0f, 10.0f}, cx, cy));
    CHECK_FALSE(g.worldToCell(Vec2{10.0f, -1.0f}, cx, cy));
    CHECK_FALSE(g.worldToCell(Vec2{1280.0f, 10.0f}, cx, cy));
    CHECK_FALSE(g.worldToCell(Vec2{10.0f, 720.0f}, cx, cy));
}
```

Add `tests/test_grid.cpp` to `laststand_tests` and `src/sim/Grid.cpp` to `laststand_core`.

- [ ] **Step 2: Run test to verify it fails**

Run:
```bash
cd /Users/mariolandaburu/Desktop/laststand && cmake --build build -j 2>&1 | tail -20
```
Expected: FAIL — `'sim/Grid.h' file not found`

- [ ] **Step 3: Write the implementation**

Create `src/sim/Grid.h`:

```cpp
#pragma once
#include "math/Vec2.h"

namespace ls {

// Uniform cell grid over the play area. Row-major indexing throughout.
class Grid {
public:
    Grid(int cols, int rows, float cellSize);

    int   cols() const { return cols_; }
    int   rows() const { return rows_; }
    float cellSize() const { return cellSize_; }
    int   cellCount() const { return cols_ * rows_; }

    float worldWidth() const { return static_cast<float>(cols_) * cellSize_; }
    float worldHeight() const { return static_cast<float>(rows_) * cellSize_; }

    bool inBounds(int cx, int cy) const;
    int  index(int cx, int cy) const { return cy * cols_ + cx; }

    Vec2 cellCenter(int cx, int cy) const;
    Vec2 cellCenterAt(int idx) const;

    // Returns false and leaves cx/cy untouched when p is outside the grid.
    bool worldToCell(Vec2 p, int& cx, int& cy) const;

private:
    int   cols_;
    int   rows_;
    float cellSize_;
};

}  // namespace ls
```

Create `src/sim/Grid.cpp`:

```cpp
#include "sim/Grid.h"

namespace ls {

Grid::Grid(int cols, int rows, float cellSize)
    : cols_(cols), rows_(rows), cellSize_(cellSize) {}

bool Grid::inBounds(int cx, int cy) const {
    return cx >= 0 && cy >= 0 && cx < cols_ && cy < rows_;
}

Vec2 Grid::cellCenter(int cx, int cy) const {
    return Vec2{(static_cast<float>(cx) + 0.5f) * cellSize_,
                (static_cast<float>(cy) + 0.5f) * cellSize_};
}

Vec2 Grid::cellCenterAt(int idx) const {
    return cellCenter(idx % cols_, idx / cols_);
}

bool Grid::worldToCell(Vec2 p, int& cx, int& cy) const {
    if (p.x < 0.0f || p.y < 0.0f) return false;
    const int tx = static_cast<int>(p.x / cellSize_);
    const int ty = static_cast<int>(p.y / cellSize_);
    if (!inBounds(tx, ty)) return false;
    cx = tx;
    cy = ty;
    return true;
}

}  // namespace ls
```

- [ ] **Step 4: Run tests to verify they pass**

Run:
```bash
cd /Users/mariolandaburu/Desktop/laststand && cmake --build build -j && ctest --test-dir build --output-on-failure
```
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
cd /Users/mariolandaburu/Desktop/laststand
git add src/sim/Grid.h src/sim/Grid.cpp tests/test_grid.cpp CMakeLists.txt
git commit -m "feat(sim): uniform Grid with world/cell conversions"
```

---

## Task 6: LevelMap and the M1 chokepoint map

**Files:**
- Create: `src/sim/LevelMap.h`
- Create: `src/sim/LevelMap.cpp`
- Create: `tests/test_levelmap.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ls::Grid` (Task 5), `ls::Vec2` (Task 2)
- Produces: `namespace ls { struct LevelMap }` with members `Grid grid`, `std::vector<uint8_t> walkable` (1 = walkable), `std::vector<int> spawnCells`, `int baseCell`; methods `bool isWalkable(int cx, int cy) const` (false when out of bounds), `bool isWalkableIndex(int idx) const`, `Vec2 baseCenter() const`. Free function `LevelMap makeM1Map()`. `FlowField`, `World` and `Renderer` all consume this.

**Map design.** 64×36 open field with two wall blocks in columns 22–28, one spanning rows 2–15 and one rows 21–34. That leaves a five-cell gap at rows 16–20: a chokepoint. Enemies spawn along column 1 and must funnel through it to reach the base at cell (58, 18). This directly exercises the flow field's obstacle routing and produces the density compression GDD §5.1 is built around — it is the smallest map that makes the pathing visibly non-trivial.

- [ ] **Step 1: Write the failing test**

Create `tests/test_levelmap.cpp`:

```cpp
#include <doctest/doctest.h>
#include "sim/LevelMap.h"

using ls::LevelMap;

TEST_CASE("M1 map has the expected shape") {
    const LevelMap m = ls::makeM1Map();
    CHECK(m.grid.cols() == 64);
    CHECK(m.grid.rows() == 36);
    CHECK(m.walkable.size() == static_cast<size_t>(64 * 36));
}

TEST_CASE("M1 map walls block the upper and lower blocks") {
    const LevelMap m = ls::makeM1Map();
    CHECK_FALSE(m.isWalkable(25, 5));    // inside upper wall
    CHECK_FALSE(m.isWalkable(22, 2));    // upper wall corner
    CHECK_FALSE(m.isWalkable(28, 15));   // upper wall corner
    CHECK_FALSE(m.isWalkable(25, 30));   // inside lower wall
}

TEST_CASE("M1 map leaves a five-cell chokepoint") {
    const LevelMap m = ls::makeM1Map();
    for (int cy = 16; cy <= 20; ++cy) {
        CHECK(m.isWalkable(25, cy));
    }
    CHECK_FALSE(m.isWalkable(25, 15));
    CHECK_FALSE(m.isWalkable(25, 21));
}

TEST_CASE("M1 map open areas are walkable") {
    const LevelMap m = ls::makeM1Map();
    CHECK(m.isWalkable(1, 18));    // spawn side
    CHECK(m.isWalkable(58, 18));   // base side
    CHECK(m.isWalkable(40, 4));    // past the wall, upper
}

TEST_CASE("isWalkable is false out of bounds rather than crashing") {
    const LevelMap m = ls::makeM1Map();
    CHECK_FALSE(m.isWalkable(-1, 18));
    CHECK_FALSE(m.isWalkable(64, 18));
    CHECK_FALSE(m.isWalkable(18, -1));
    CHECK_FALSE(m.isWalkable(18, 36));
}

TEST_CASE("base and spawns are placed on walkable cells") {
    const LevelMap m = ls::makeM1Map();
    REQUIRE(m.baseCell >= 0);
    CHECK(m.isWalkableIndex(m.baseCell));
    CHECK(m.baseCell == m.grid.index(58, 18));

    REQUIRE_FALSE(m.spawnCells.empty());
    for (const int c : m.spawnCells) {
        CHECK(m.isWalkableIndex(c));
    }
}

TEST_CASE("baseCenter matches the base cell centre") {
    const LevelMap m = ls::makeM1Map();
    CHECK(m.baseCenter().x == doctest::Approx(m.grid.cellCenter(58, 18).x));
    CHECK(m.baseCenter().y == doctest::Approx(m.grid.cellCenter(58, 18).y));
}
```

Add `tests/test_levelmap.cpp` and `src/sim/LevelMap.cpp` to their target lists.

- [ ] **Step 2: Run test to verify it fails**

Run:
```bash
cd /Users/mariolandaburu/Desktop/laststand && cmake --build build -j 2>&1 | tail -20
```
Expected: FAIL — `'sim/LevelMap.h' file not found`

- [ ] **Step 3: Write the implementation**

Create `src/sim/LevelMap.h`:

```cpp
#pragma once
#include <cstdint>
#include <vector>

#include "math/Vec2.h"
#include "sim/Grid.h"

namespace ls {

struct LevelMap {
    Grid                 grid{64, 36, 20.0f};
    std::vector<uint8_t> walkable;      // 1 = walkable, 0 = wall
    std::vector<int>     spawnCells;
    int                  baseCell = 0;

    bool isWalkable(int cx, int cy) const;
    bool isWalkableIndex(int idx) const;
    Vec2 baseCenter() const;
};

// 64x36 open field, two wall blocks leaving a five-cell chokepoint at
// rows 16-20, spawns down column 1, base at cell (58, 18).
LevelMap makeM1Map();

}  // namespace ls
```

Create `src/sim/LevelMap.cpp`:

```cpp
#include "sim/LevelMap.h"

namespace ls {

bool LevelMap::isWalkable(int cx, int cy) const {
    if (!grid.inBounds(cx, cy)) return false;
    return walkable[static_cast<size_t>(grid.index(cx, cy))] != 0u;
}

bool LevelMap::isWalkableIndex(int idx) const {
    if (idx < 0 || idx >= grid.cellCount()) return false;
    return walkable[static_cast<size_t>(idx)] != 0u;
}

Vec2 LevelMap::baseCenter() const {
    return grid.cellCenterAt(baseCell);
}

LevelMap makeM1Map() {
    LevelMap m;
    m.walkable.assign(static_cast<size_t>(m.grid.cellCount()), 1u);

    const auto carveWall = [&m](int x0, int y0, int x1, int y1) {
        for (int cy = y0; cy <= y1; ++cy) {
            for (int cx = x0; cx <= x1; ++cx) {
                m.walkable[static_cast<size_t>(m.grid.index(cx, cy))] = 0u;
            }
        }
    };

    carveWall(22, 2, 28, 15);    // upper block
    carveWall(22, 21, 28, 34);   // lower block
    // Rows 16-20 between them stay open: the chokepoint.

    for (int cy = 4; cy <= 32; ++cy) {
        m.spawnCells.push_back(m.grid.index(1, cy));
    }

    m.baseCell = m.grid.index(58, 18);
    return m;
}

}  // namespace ls
```

- [ ] **Step 4: Run tests to verify they pass**

Run:
```bash
cd /Users/mariolandaburu/Desktop/laststand && cmake --build build -j && ctest --test-dir build --output-on-failure
```
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
cd /Users/mariolandaburu/Desktop/laststand
git add src/sim/LevelMap.h src/sim/LevelMap.cpp tests/test_levelmap.cpp CMakeLists.txt
git commit -m "feat(sim): LevelMap and the M1 chokepoint map

Two wall blocks leave a five-cell gap, forcing the horde to funnel.
Smallest map that makes flow-field routing visibly non-trivial and
produces the density compression the turret design depends on."
```

---

## Task 7: FlowField — Dijkstra pathing

**Files:**
- Create: `src/ai/FlowField.h`
- Create: `src/ai/FlowField.cpp`
- Create: `tests/test_flowfield.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ls::LevelMap` (Task 6), `ls::Grid` (Task 5), `ls::Vec2` (Task 2)
- Produces: `namespace ls { class FlowField }` with `void build(const LevelMap& map)`, `Vec2 sample(Vec2 worldPos) const` returning `{0,0}` for out-of-bounds/unreachable, `float costAt(int cx, int cy) const`, `Vec2 dirAt(int cx, int cy) const`, `bool isReachable(int cx, int cy) const`, and `static constexpr float kUnreachable`. `MovementSystem` (Task 9) and `Renderer` (Task 12) consume this.

**This is the load-bearing task of the milestone.** Per GDD §14.5, flow-field pathing is what makes 50,000 entities possible: one Dijkstra pass at load, then O(1) per enemy per tick with no search and no per-agent state. Get it right here and the rest of the project inherits it.

Diagonal moves cost √2 and are forbidden when either adjacent cardinal cell is a wall — without that rule, agents cut through wall corners diagonally, which looks broken and lets the horde leak through the chokepoint's edges.

- [ ] **Step 1: Write the failing test**

Create `tests/test_flowfield.cpp`:

```cpp
#include <doctest/doctest.h>
#include "ai/FlowField.h"
#include "sim/LevelMap.h"

using ls::FlowField;
using ls::LevelMap;
using ls::Vec2;

namespace {
// Fully open 20x20 map, base at (10,10), no walls.
LevelMap makeOpenMap() {
    LevelMap m;
    m.grid = ls::Grid{20, 20, 10.0f};
    m.walkable.assign(static_cast<size_t>(m.grid.cellCount()), 1u);
    m.baseCell = m.grid.index(10, 10);
    m.spawnCells.push_back(m.grid.index(0, 10));
    return m;
}
}  // namespace

TEST_CASE("cost at the base is zero") {
    FlowField f;
    f.build(makeOpenMap());
    CHECK(f.costAt(10, 10) == doctest::Approx(0.0f));
}

TEST_CASE("cost increases with distance from the base") {
    FlowField f;
    f.build(makeOpenMap());
    CHECK(f.costAt(9, 10) < f.costAt(5, 10));
    CHECK(f.costAt(5, 10) < f.costAt(0, 10));
}

TEST_CASE("direction points toward the base on an open map") {
    FlowField f;
    f.build(makeOpenMap());
    const Vec2 d = f.dirAt(5, 10);   // base is to the +x side
    CHECK(d.x > 0.5f);
    CHECK(d.y == doctest::Approx(0.0f).epsilon(0.01));
}

TEST_CASE("the base cell itself has no direction") {
    FlowField f;
    f.build(makeOpenMap());
    CHECK(f.dirAt(10, 10).x == doctest::Approx(0.0f));
    CHECK(f.dirAt(10, 10).y == doctest::Approx(0.0f));
}

TEST_CASE("directions are unit length wherever reachable and not the base") {
    FlowField f;
    f.build(makeOpenMap());
    CHECK(ls::length(f.dirAt(3, 3)) == doctest::Approx(1.0f));
    CHECK(ls::length(f.dirAt(19, 0)) == doctest::Approx(1.0f));
}

TEST_CASE("walled-off cells are unreachable") {
    LevelMap m = makeOpenMap();
    // Seal cell (0,0) behind walls.
    m.walkable[static_cast<size_t>(m.grid.index(1, 0))] = 0u;
    m.walkable[static_cast<size_t>(m.grid.index(0, 1))] = 0u;
    m.walkable[static_cast<size_t>(m.grid.index(1, 1))] = 0u;

    FlowField f;
    f.build(m);
    CHECK_FALSE(f.isReachable(0, 0));
    CHECK(f.costAt(0, 0) == FlowField::kUnreachable);
    CHECK(f.dirAt(0, 0).x == 0.0f);
    CHECK(f.dirAt(0, 0).y == 0.0f);
}

TEST_CASE("wall cells are unreachable") {
    LevelMap m = makeOpenMap();
    m.walkable[static_cast<size_t>(m.grid.index(4, 4))] = 0u;
    FlowField f;
    f.build(m);
    CHECK_FALSE(f.isReachable(4, 4));
}

TEST_CASE("paths route around obstacles rather than through them") {
    const LevelMap m = ls::makeM1Map();
    FlowField f;
    f.build(m);

    // A spawn-side cell level with the wall must pay more than the straight
    // line distance, because the only route is via the chokepoint.
    const float cost = f.costAt(5, 5);
    CHECK(f.isReachable(5, 5));
    CHECK(cost > 53.0f);   // straight-line cell distance from (5,5) to (58,18)
}

TEST_CASE("the whole M1 spawn line can reach the base") {
    const LevelMap m = ls::makeM1Map();
    FlowField f;
    f.build(m);
    for (const int c : m.spawnCells) {
        const int cx = c % m.grid.cols();
        const int cy = c / m.grid.cols();
        CHECK(f.isReachable(cx, cy));
    }
}

TEST_CASE("sample returns zero outside the grid") {
    FlowField f;
    f.build(makeOpenMap());
    CHECK(f.sample(Vec2{-5.0f, -5.0f}).x == 0.0f);
    CHECK(f.sample(Vec2{9999.0f, 9999.0f}).y == 0.0f);
}

TEST_CASE("sample agrees with dirAt for the containing cell") {
    FlowField f;
    const LevelMap m = makeOpenMap();
    f.build(m);
    const Vec2 s = f.sample(m.grid.cellCenter(5, 10));
    const Vec2 d = f.dirAt(5, 10);
    CHECK(s.x == doctest::Approx(d.x));
    CHECK(s.y == doctest::Approx(d.y));
}

TEST_CASE("build is idempotent") {
    FlowField f;
    const LevelMap m = ls::makeM1Map();
    f.build(m);
    const float first = f.costAt(5, 5);
    f.build(m);
    CHECK(f.costAt(5, 5) == doctest::Approx(first));
}
```

Add `tests/test_flowfield.cpp` and `src/ai/FlowField.cpp` to their target lists.

- [ ] **Step 2: Run test to verify it fails**

Run:
```bash
cd /Users/mariolandaburu/Desktop/laststand && cmake --build build -j 2>&1 | tail -20
```
Expected: FAIL — `'ai/FlowField.h' file not found`

- [ ] **Step 3: Write the implementation**

Create `src/ai/FlowField.h`:

```cpp
#pragma once
#include <limits>
#include <vector>

#include "math/Vec2.h"
#include "sim/Grid.h"
#include "sim/LevelMap.h"

namespace ls {

// Precomputed pathing for an entire map. One Dijkstra pass from the base at
// load time; per-agent movement is then an O(1) grid sample. This is the
// decision that makes tens of thousands of agents tractable — per-agent A*
// at that count is not slow, it is impossible.
class FlowField {
public:
    static constexpr float kUnreachable = std::numeric_limits<float>::infinity();

    void build(const LevelMap& map);

    Vec2  sample(Vec2 worldPos) const;   // {0,0} if outside or unreachable
    float costAt(int cx, int cy) const;
    Vec2  dirAt(int cx, int cy) const;
    bool  isReachable(int cx, int cy) const;

private:
    Grid               grid_{1, 1, 1.0f};
    std::vector<float> cost_;
    std::vector<Vec2>  dir_;
};

}  // namespace ls
```

Create `src/ai/FlowField.cpp`:

```cpp
#include "ai/FlowField.h"

#include <array>
#include <functional>
#include <queue>
#include <utility>

namespace ls {

namespace {

constexpr float kCardinal = 1.0f;
constexpr float kDiagonal = 1.41421356f;

struct Neighbour {
    int   dx;
    int   dy;
    float cost;
};

constexpr std::array<Neighbour, 8> kNeighbours{{
    {1, 0, kCardinal},  {-1, 0, kCardinal}, {0, 1, kCardinal},  {0, -1, kCardinal},
    {1, 1, kDiagonal},  {1, -1, kDiagonal}, {-1, 1, kDiagonal}, {-1, -1, kDiagonal},
}};

}  // namespace

void FlowField::build(const LevelMap& map) {
    grid_ = map.grid;
    const size_t cells = static_cast<size_t>(grid_.cellCount());

    cost_.assign(cells, kUnreachable);
    dir_.assign(cells, Vec2{0.0f, 0.0f});

    if (!map.isWalkableIndex(map.baseCell)) return;

    using Entry = std::pair<float, int>;   // (cost, cellIndex)
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;

    cost_[static_cast<size_t>(map.baseCell)] = 0.0f;
    open.emplace(0.0f, map.baseCell);

    while (!open.empty()) {
        const auto [c, idx] = open.top();
        open.pop();

        // Stale queue entry from a later, cheaper relaxation.
        if (c > cost_[static_cast<size_t>(idx)]) continue;

        const int cx = idx % grid_.cols();
        const int cy = idx / grid_.cols();

        for (const Neighbour& n : kNeighbours) {
            const int nx = cx + n.dx;
            const int ny = cy + n.dy;
            if (!map.isWalkable(nx, ny)) continue;

            // Forbid cutting a wall corner diagonally. Without this the horde
            // squeezes through the diagonal seams of the chokepoint.
            if (n.dx != 0 && n.dy != 0) {
                if (!map.isWalkable(cx + n.dx, cy)) continue;
                if (!map.isWalkable(cx, cy + n.dy)) continue;
            }

            const size_t ni = static_cast<size_t>(grid_.index(nx, ny));
            const float  candidate = c + n.cost;
            if (candidate < cost_[ni]) {
                cost_[ni] = candidate;
                open.emplace(candidate, grid_.index(nx, ny));
            }
        }
    }

    // Second pass: each reachable cell points at its cheapest walkable
    // neighbour. Done after Dijkstra completes so every cost is final.
    for (int cy = 0; cy < grid_.rows(); ++cy) {
        for (int cx = 0; cx < grid_.cols(); ++cx) {
            const size_t i = static_cast<size_t>(grid_.index(cx, cy));
            if (cost_[i] == kUnreachable || cost_[i] == 0.0f) continue;

            float bestCost = cost_[i];
            int   bestX = -1;
            int   bestY = -1;

            for (const Neighbour& n : kNeighbours) {
                const int nx = cx + n.dx;
                const int ny = cy + n.dy;
                if (!map.isWalkable(nx, ny)) continue;
                if (n.dx != 0 && n.dy != 0) {
                    if (!map.isWalkable(cx + n.dx, cy)) continue;
                    if (!map.isWalkable(cx, cy + n.dy)) continue;
                }
                const size_t ni = static_cast<size_t>(grid_.index(nx, ny));
                if (cost_[ni] < bestCost) {
                    bestCost = cost_[ni];
                    bestX = nx;
                    bestY = ny;
                }
            }

            if (bestX >= 0) {
                dir_[i] = normalized(grid_.cellCenter(bestX, bestY) -
                                     grid_.cellCenter(cx, cy));
            }
        }
    }
}

float FlowField::costAt(int cx, int cy) const {
    if (!grid_.inBounds(cx, cy)) return kUnreachable;
    return cost_[static_cast<size_t>(grid_.index(cx, cy))];
}

Vec2 FlowField::dirAt(int cx, int cy) const {
    if (!grid_.inBounds(cx, cy)) return Vec2{0.0f, 0.0f};
    return dir_[static_cast<size_t>(grid_.index(cx, cy))];
}

bool FlowField::isReachable(int cx, int cy) const {
    return costAt(cx, cy) != kUnreachable;
}

Vec2 FlowField::sample(Vec2 worldPos) const {
    int cx = 0;
    int cy = 0;
    if (!grid_.worldToCell(worldPos, cx, cy)) return Vec2{0.0f, 0.0f};
    return dir_[static_cast<size_t>(grid_.index(cx, cy))];
}

}  // namespace ls
```

- [ ] **Step 4: Run tests to verify they pass**

Run:
```bash
cd /Users/mariolandaburu/Desktop/laststand && cmake --build build -j && ctest --test-dir build --output-on-failure
```
Expected: PASS. If "paths route around obstacles" fails, the corner-cutting guard is likely inverted. If directions are not unit length, `normalized` is being applied to a zero vector — check that the base cell is excluded from the direction pass.

- [ ] **Step 5: Commit**

```bash
cd /Users/mariolandaburu/Desktop/laststand
git add src/ai/FlowField.h src/ai/FlowField.cpp tests/test_flowfield.cpp CMakeLists.txt
git commit -m "feat(ai): flow-field pathing via Dijkstra from the base

One O(cells) pass at load; per-agent movement is then an O(1) sample.
This is the decision that makes 50k agents feasible at all. Diagonal
moves cost sqrt(2) and are forbidden around wall corners, so the horde
cannot leak through the diagonal seams of a chokepoint."
```

---

## Task 8: EnemyPool — struct-of-arrays storage

**Files:**
- Create: `src/sim/EnemyPool.h`
- Create: `src/sim/EnemyPool.cpp`
- Create: `tests/test_enemypool.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ls::Vec2` (Task 2)
- Produces: `namespace ls { class EnemyPool }` with `static constexpr uint32_t kCapacity = 100'000u`, `static constexpr uint32_t kInvalid = 0xFFFFFFFFu`, `EnemyPool()`, `uint32_t spawn(Vec2 pos, float hp, uint8_t type)` returning the new index or `kInvalid` when full, `void kill(uint32_t i)` (swap-remove), `void clear()`, `uint32_t count() const`. Public SoA arrays `position`, `prevPosition`, `velocity`, `health`, `type`. `MovementSystem`, `Base` and `Renderer` all consume these arrays directly.

**Why the arrays are public.** Systems iterate them in tight loops; wrapping each in a getter would add noise without adding safety, since the pool's invariant (`count_` elements are live) is enforced by `spawn`/`kill` and nothing else. Per GDD §14.3 this is a typed pool, deliberately not a generic ECS — five entity types, all known at compile time, none needing runtime composition.

All arrays are sized to `kCapacity` in the constructor and never resized. This is the "no allocation during a tick" invariant from the Global Constraints, established on day one because it cannot be retrofitted.

- [ ] **Step 1: Write the failing test**

Create `tests/test_enemypool.cpp`:

```cpp
#include <doctest/doctest.h>
#include "sim/EnemyPool.h"

using ls::EnemyPool;
using ls::Vec2;

TEST_CASE("a new pool is empty but fully reserved") {
    EnemyPool p;
    CHECK(p.count() == 0u);
    CHECK(p.position.size() == EnemyPool::kCapacity);
    CHECK(p.health.size() == EnemyPool::kCapacity);
}

TEST_CASE("spawn returns sequential indices and stores the payload") {
    EnemyPool p;
    CHECK(p.spawn(Vec2{1.0f, 2.0f}, 100.0f, 0u) == 0u);
    CHECK(p.spawn(Vec2{3.0f, 4.0f}, 40.0f, 1u) == 1u);
    CHECK(p.count() == 2u);

    CHECK(p.position[0].x == doctest::Approx(1.0f));
    CHECK(p.health[1] == doctest::Approx(40.0f));
    CHECK(p.type[1] == 1u);
}

TEST_CASE("spawn initialises prevPosition to position so interpolation is stable") {
    EnemyPool p;
    p.spawn(Vec2{7.0f, 9.0f}, 100.0f, 0u);
    CHECK(p.prevPosition[0].x == doctest::Approx(7.0f));
    CHECK(p.prevPosition[0].y == doctest::Approx(9.0f));
}

TEST_CASE("spawn zeroes velocity") {
    EnemyPool p;
    p.spawn(Vec2{7.0f, 9.0f}, 100.0f, 0u);
    CHECK(p.velocity[0].x == 0.0f);
    CHECK(p.velocity[0].y == 0.0f);
}

TEST_CASE("kill swap-removes: the last element fills the hole") {
    EnemyPool p;
    p.spawn(Vec2{0.0f, 0.0f}, 10.0f, 0u);   // index 0
    p.spawn(Vec2{1.0f, 1.0f}, 20.0f, 1u);   // index 1
    p.spawn(Vec2{2.0f, 2.0f}, 30.0f, 2u);   // index 2

    p.kill(1u);

    CHECK(p.count() == 2u);
    CHECK(p.health[1] == doctest::Approx(30.0f));   // former index 2
    CHECK(p.type[1] == 2u);
    CHECK(p.position[1].x == doctest::Approx(2.0f));
    CHECK(p.health[0] == doctest::Approx(10.0f));   // untouched
}

TEST_CASE("killing the last element just shrinks the count") {
    EnemyPool p;
    p.spawn(Vec2{0.0f, 0.0f}, 10.0f, 0u);
    p.spawn(Vec2{1.0f, 1.0f}, 20.0f, 1u);
    p.kill(1u);
    CHECK(p.count() == 1u);
    CHECK(p.health[0] == doctest::Approx(10.0f));
}

TEST_CASE("killing out of range is a no-op rather than corruption") {
    EnemyPool p;
    p.spawn(Vec2{0.0f, 0.0f}, 10.0f, 0u);
    p.kill(5u);
    CHECK(p.count() == 1u);
}

TEST_CASE("clear empties the pool") {
    EnemyPool p;
    for (int i = 0; i < 10; ++i) p.spawn(Vec2{0.0f, 0.0f}, 10.0f, 0u);
    p.clear();
    CHECK(p.count() == 0u);
}

TEST_CASE("spawning past capacity returns kInvalid instead of growing") {
    EnemyPool p;
    for (uint32_t i = 0; i < EnemyPool::kCapacity; ++i) {
        REQUIRE(p.spawn(Vec2{0.0f, 0.0f}, 1.0f, 0u) != EnemyPool::kInvalid);
    }
    CHECK(p.spawn(Vec2{0.0f, 0.0f}, 1.0f, 0u) == EnemyPool::kInvalid);
    CHECK(p.count() == EnemyPool::kCapacity);
    CHECK(p.position.size() == EnemyPool::kCapacity);   // never reallocated
}
```

Add `tests/test_enemypool.cpp` and `src/sim/EnemyPool.cpp` to their target lists.

- [ ] **Step 2: Run test to verify it fails**

Run:
```bash
cd /Users/mariolandaburu/Desktop/laststand && cmake --build build -j 2>&1 | tail -20
```
Expected: FAIL — `'sim/EnemyPool.h' file not found`

- [ ] **Step 3: Write the implementation**

Create `src/sim/EnemyPool.h`:

```cpp
#pragma once
#include <cstdint>
#include <vector>

#include "math/Vec2.h"

namespace ls {

// Fixed-capacity struct-of-arrays storage. Hot fields (position, velocity,
// health) are separate arrays so movement touches only what it needs and
// walks contiguous memory. Deliberately NOT a generic ECS: see GDD 14.3.
//
// Arrays are sized once at construction and never resized, which is what
// gives us the "no heap allocation inside a tick" invariant.
class EnemyPool {
public:
    static constexpr uint32_t kCapacity = 100'000u;
    static constexpr uint32_t kInvalid  = 0xFFFFFFFFu;

    EnemyPool();

    uint32_t spawn(Vec2 pos, float hp, uint8_t enemyType);
    void     kill(uint32_t i);       // swap-remove; O(1), does not preserve order
    void     clear();

    uint32_t count() const { return count_; }

    // Public by design: systems iterate these directly in tight loops.
    // The live-range invariant (indices [0, count_) ) is owned by
    // spawn()/kill()/clear() and by nothing else.
    std::vector<Vec2>    position;
    std::vector<Vec2>    prevPosition;
    std::vector<Vec2>    velocity;
    std::vector<float>   health;
    std::vector<uint8_t> type;

private:
    uint32_t count_ = 0u;
};

}  // namespace ls
```

Create `src/sim/EnemyPool.cpp`:

```cpp
#include "sim/EnemyPool.h"

namespace ls {

EnemyPool::EnemyPool() {
    const size_t cap = static_cast<size_t>(kCapacity);
    position.resize(cap);
    prevPosition.resize(cap);
    velocity.resize(cap);
    health.resize(cap, 0.0f);
    type.resize(cap, 0u);
}

uint32_t EnemyPool::spawn(Vec2 pos, float hp, uint8_t enemyType) {
    if (count_ >= kCapacity) return kInvalid;

    const uint32_t i = count_++;
    const size_t   s = static_cast<size_t>(i);

    position[s]     = pos;
    prevPosition[s] = pos;          // so the first interpolated frame is stable
    velocity[s]     = Vec2{0.0f, 0.0f};
    health[s]       = hp;
    type[s]         = enemyType;
    return i;
}

void EnemyPool::kill(uint32_t i) {
    if (i >= count_) return;

    const uint32_t last = count_ - 1u;
    if (i != last) {
        const size_t d = static_cast<size_t>(i);
        const size_t l = static_cast<size_t>(last);
        position[d]     = position[l];
        prevPosition[d] = prevPosition[l];
        velocity[d]     = velocity[l];
        health[d]       = health[l];
        type[d]         = type[l];
    }
    count_ = last;
}

void EnemyPool::clear() {
    count_ = 0u;
}

}  // namespace ls
```

- [ ] **Step 4: Run tests to verify they pass**

Run:
```bash
cd /Users/mariolandaburu/Desktop/laststand && cmake --build build -j && ctest --test-dir build --output-on-failure
```
Expected: PASS. The capacity test spawns 100,000 entities and takes a moment; that is expected.

- [ ] **Step 5: Commit**

```bash
cd /Users/mariolandaburu/Desktop/laststand
git add src/sim/EnemyPool.h src/sim/EnemyPool.cpp tests/test_enemypool.cpp CMakeLists.txt
git commit -m "feat(sim): fixed-capacity SoA EnemyPool with swap-remove

100k capacity reserved at construction, never resized, establishing the
no-allocation-inside-a-tick invariant from hour one. Typed pool rather
than a generic ECS per GDD 14.3."
```

---

## Task 9: MovementSystem — Stage 0, deliberately unoptimised

**Files:**
- Create: `src/sim/MovementSystem.h`
- Create: `src/sim/MovementSystem.cpp`
- Create: `tests/test_movement.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ls::EnemyPool` (Task 8), `ls::FlowField` (Task 7), `ls::Vec2` (Task 2)
- Produces: `namespace ls { struct MovementParams { float speed; float separationRadius; float separationStrength; }; void updateMovement(EnemyPool&, const FlowField&, float dt, const MovementParams&); }`. `World::tick` (Task 11) calls this once per tick.

> ### ⚠️ STAGE 0 — DO NOT OPTIMISE THIS TASK
>
> The separation loop below is O(n²) **on purpose**. Per GDD §15, it is the
> measured baseline that Milestone 5's spatial-hash work is compared against,
> and the git history showing the before/after is the single most valuable
> artifact this project produces. An agent that "helpfully" adds a spatial
> hash here destroys that. Write it naive, measure it, commit it.

- [ ] **Step 1: Write the failing test**

Create `tests/test_movement.cpp`:

```cpp
#include <doctest/doctest.h>
#include "sim/MovementSystem.h"
#include "sim/LevelMap.h"
#include "sim/EnemyPool.h"
#include "ai/FlowField.h"

using ls::EnemyPool;
using ls::FlowField;
using ls::LevelMap;
using ls::MovementParams;
using ls::Vec2;

namespace {
LevelMap makeOpenMap() {
    LevelMap m;
    m.grid = ls::Grid{20, 20, 10.0f};
    m.walkable.assign(static_cast<size_t>(m.grid.cellCount()), 1u);
    m.baseCell = m.grid.index(19, 10);
    return m;
}
}  // namespace

TEST_CASE("an enemy moves toward the base") {
    const LevelMap m = makeOpenMap();
    FlowField f;
    f.build(m);

    EnemyPool p;
    p.spawn(m.grid.cellCenter(2, 10), 100.0f, 0u);
    const float startX = p.position[0].x;

    MovementParams params;
    for (int i = 0; i < 10; ++i) ls::updateMovement(p, f, 1.0f / 60.0f, params);

    CHECK(p.position[0].x > startX);
}

TEST_CASE("prevPosition captures the position before the move") {
    const LevelMap m = makeOpenMap();
    FlowField f;
    f.build(m);

    EnemyPool p;
    const Vec2 start = m.grid.cellCenter(2, 10);
    p.spawn(start, 100.0f, 0u);

    MovementParams params;
    ls::updateMovement(p, f, 1.0f / 60.0f, params);

    CHECK(p.prevPosition[0].x == doctest::Approx(start.x));
    CHECK(p.position[0].x != doctest::Approx(start.x));
}

TEST_CASE("distant enemies do not push each other") {
    const LevelMap m = makeOpenMap();
    FlowField f;
    f.build(m);

    EnemyPool p;
    p.spawn(m.grid.cellCenter(2, 2), 100.0f, 0u);
    p.spawn(m.grid.cellCenter(2, 18), 100.0f, 0u);

    MovementParams params;
    params.separationRadius = 5.0f;
    ls::updateMovement(p, f, 1.0f / 60.0f, params);

    // Velocity should be pure flow: magnitude equals speed.
    CHECK(ls::length(p.velocity[0]) == doctest::Approx(params.speed).epsilon(0.01));
}

TEST_CASE("overlapping enemies push apart") {
    const LevelMap m = makeOpenMap();
    FlowField f;
    f.build(m);

    EnemyPool p;
    const Vec2 at = m.grid.cellCenter(5, 10);
    p.spawn(at, 100.0f, 0u);
    p.spawn(at + Vec2{1.0f, 0.0f}, 100.0f, 0u);

    MovementParams params;
    const float before = ls::distanceSq(p.position[0], p.position[1]);
    for (int i = 0; i < 20; ++i) ls::updateMovement(p, f, 1.0f / 60.0f, params);
    const float after = ls::distanceSq(p.position[0], p.position[1]);

    CHECK(after > before);
}

TEST_CASE("perfectly coincident enemies separate deterministically") {
    const LevelMap m = makeOpenMap();
    FlowField f;
    f.build(m);

    EnemyPool p;
    const Vec2 at = m.grid.cellCenter(5, 10);
    p.spawn(at, 100.0f, 0u);
    p.spawn(at, 100.0f, 0u);   // exactly the same position

    MovementParams params;
    for (int i = 0; i < 20; ++i) ls::updateMovement(p, f, 1.0f / 60.0f, params);

    CHECK(ls::distanceSq(p.position[0], p.position[1]) > 0.01f);
}

TEST_CASE("an enemy on an unreachable cell does not drift") {
    LevelMap m = makeOpenMap();
    // Seal (0,0) off completely.
    m.walkable[static_cast<size_t>(m.grid.index(1, 0))] = 0u;
    m.walkable[static_cast<size_t>(m.grid.index(0, 1))] = 0u;
    m.walkable[static_cast<size_t>(m.grid.index(1, 1))] = 0u;

    FlowField f;
    f.build(m);

    EnemyPool p;
    const Vec2 start = m.grid.cellCenter(0, 0);
    p.spawn(start, 100.0f, 0u);

    MovementParams params;
    for (int i = 0; i < 10; ++i) ls::updateMovement(p, f, 1.0f / 60.0f, params);

    CHECK(p.position[0].x == doctest::Approx(start.x));
    CHECK(p.position[0].y == doctest::Approx(start.y));
}

TEST_CASE("an empty pool is a no-op") {
    const LevelMap m = makeOpenMap();
    FlowField f;
    f.build(m);
    EnemyPool p;
    MovementParams params;
    ls::updateMovement(p, f, 1.0f / 60.0f, params);   // must not crash
    CHECK(p.count() == 0u);
}
```

Add `tests/test_movement.cpp` and `src/sim/MovementSystem.cpp` to their target lists.

- [ ] **Step 2: Run test to verify it fails**

Run:
```bash
cd /Users/mariolandaburu/Desktop/laststand && cmake --build build -j 2>&1 | tail -20
```
Expected: FAIL — `'sim/MovementSystem.h' file not found`

- [ ] **Step 3: Write the implementation**

Create `src/sim/MovementSystem.h`:

```cpp
#pragma once
#include "ai/FlowField.h"
#include "sim/EnemyPool.h"

namespace ls {

struct MovementParams {
    float speed              = 40.0f;   // world units per second
    float separationRadius   = 12.0f;
    float separationStrength = 50.0f;
};

// Advances every live enemy by one tick: sample the flow field for
// direction, add a local separation force, integrate.
void updateMovement(EnemyPool& pool,
                    const FlowField& field,
                    float dt,
                    const MovementParams& params);

}  // namespace ls
```

Create `src/sim/MovementSystem.cpp`:

```cpp
#include "sim/MovementSystem.h"

#include <cmath>

namespace ls {

void updateMovement(EnemyPool& pool,
                    const FlowField& field,
                    float dt,
                    const MovementParams& params) {
    const uint32_t n = pool.count();
    if (n == 0u) return;

    for (uint32_t i = 0; i < n; ++i) {
        pool.prevPosition[i] = pool.position[i];
    }

    const float radius   = params.separationRadius;
    const float radiusSq = radius * radius;

    for (uint32_t i = 0; i < n; ++i) {
        const Vec2 flow = field.sample(pool.position[i]);
        const Vec2 desired = flow * params.speed;

        // ------------------------------------------------------------------
        // STAGE 0 (GDD 15): O(n^2) separation. DELIBERATELY UNOPTIMISED.
        // This is the baseline the M5 spatial hash is measured against.
        // Do not replace it before that measurement is committed.
        // ------------------------------------------------------------------
        Vec2 push{0.0f, 0.0f};
        for (uint32_t j = 0; j < n; ++j) {
            if (j == i) continue;

            const Vec2  delta = pool.position[i] - pool.position[j];
            const float dSq   = lengthSq(delta);
            if (dSq > radiusSq) continue;

            if (dSq <= 1e-6f) {
                // Perfectly coincident. Nudge by index order rather than
                // randomly, so the simulation stays reproducible.
                push += Vec2{(i < j) ? -1.0f : 1.0f, 0.0f};
                continue;
            }

            const float dist = std::sqrt(dSq);
            const float falloff = (radius - dist) / radius;   // 1 at 0, 0 at radius
            push += (delta * (1.0f / dist)) * falloff;
        }

        pool.velocity[i] = desired + push * params.separationStrength;
        pool.position[i] += pool.velocity[i] * dt;
    }
}

}  // namespace ls
```

- [ ] **Step 4: Run tests to verify they pass**

Run:
```bash
cd /Users/mariolandaburu/Desktop/laststand && cmake --build build -j && ctest --test-dir build --output-on-failure
```
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
cd /Users/mariolandaburu/Desktop/laststand
git add src/sim/MovementSystem.h src/sim/MovementSystem.cpp tests/test_movement.cpp CMakeLists.txt
git commit -m "feat(sim): movement via flow-field sample plus separation

STAGE 0 baseline: separation is O(n^2) on purpose (GDD 15). This is the
measurement the M5 spatial-hash work is compared against; do not optimise
before that baseline is recorded. Coincident agents separate by index
order rather than randomly, keeping the sim reproducible."
```

---

## Task 10: Base and arrival damage

**Files:**
- Create: `src/sim/Base.h`
- Create: `src/sim/Base.cpp`
- Create: `tests/test_base.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ls::EnemyPool` (Task 8), `ls::Vec2` (Task 2)
- Produces: `namespace ls { struct Base { Vec2 position; float radius; float health; float maxHealth; bool isDestroyed() const; }; uint32_t applyArrivals(EnemyPool&, Base&); }` returning the number of enemies that arrived this tick. `World::tick` (Task 11) calls this after `updateMovement`.

Per GDD §4.5, an arriving enemy deals damage **equal to its remaining health** and then despawns. There is no separate "leak counter" abstraction — a Tank that survives hurts far more than a Grunt, which is what makes the failure mode legible.

- [ ] **Step 1: Write the failing test**

Create `tests/test_base.cpp`:

```cpp
#include <doctest/doctest.h>
#include "sim/Base.h"
#include "sim/EnemyPool.h"

using ls::Base;
using ls::EnemyPool;
using ls::Vec2;

TEST_CASE("an enemy inside the radius damages the base and despawns") {
    Base b{Vec2{100.0f, 100.0f}, 30.0f, 1000.0f, 1000.0f};
    EnemyPool p;
    p.spawn(Vec2{110.0f, 100.0f}, 250.0f, 0u);

    CHECK(ls::applyArrivals(p, b) == 1u);
    CHECK(b.health == doctest::Approx(750.0f));
    CHECK(p.count() == 0u);
}

TEST_CASE("an enemy outside the radius is untouched") {
    Base b{Vec2{100.0f, 100.0f}, 30.0f, 1000.0f, 1000.0f};
    EnemyPool p;
    p.spawn(Vec2{200.0f, 100.0f}, 250.0f, 0u);

    CHECK(ls::applyArrivals(p, b) == 0u);
    CHECK(b.health == doctest::Approx(1000.0f));
    CHECK(p.count() == 1u);
}

TEST_CASE("damage scales with the arriving enemy's remaining health") {
    Base b{Vec2{0.0f, 0.0f}, 30.0f, 5000.0f, 5000.0f};
    EnemyPool p;
    p.spawn(Vec2{0.0f, 0.0f}, 2000.0f, 2u);   // a Tank

    ls::applyArrivals(p, b);
    CHECK(b.health == doctest::Approx(3000.0f));
}

TEST_CASE("multiple simultaneous arrivals are all processed") {
    Base b{Vec2{0.0f, 0.0f}, 50.0f, 1000.0f, 1000.0f};
    EnemyPool p;
    for (int i = 0; i < 5; ++i) p.spawn(Vec2{0.0f, 0.0f}, 100.0f, 0u);

    CHECK(ls::applyArrivals(p, b) == 5u);
    CHECK(p.count() == 0u);
    CHECK(b.health == doctest::Approx(500.0f));
}

TEST_CASE("mixed arrivals leave the distant enemies alive") {
    Base b{Vec2{0.0f, 0.0f}, 50.0f, 1000.0f, 1000.0f};
    EnemyPool p;
    p.spawn(Vec2{0.0f, 0.0f},     100.0f, 0u);   // arrives
    p.spawn(Vec2{500.0f, 0.0f},   100.0f, 0u);   // far
    p.spawn(Vec2{10.0f, 10.0f},   100.0f, 0u);   // arrives
    p.spawn(Vec2{600.0f, 0.0f},   100.0f, 0u);   // far

    CHECK(ls::applyArrivals(p, b) == 2u);
    CHECK(p.count() == 2u);
    for (uint32_t i = 0; i < p.count(); ++i) {
        CHECK(ls::length(p.position[i]) > 100.0f);
    }
}

TEST_CASE("health floors at zero and the base reports destroyed") {
    Base b{Vec2{0.0f, 0.0f}, 30.0f, 100.0f, 100.0f};
    EnemyPool p;
    p.spawn(Vec2{0.0f, 0.0f}, 5000.0f, 0u);

    ls::applyArrivals(p, b);
    CHECK(b.health == doctest::Approx(0.0f));
    CHECK(b.isDestroyed());
}

TEST_CASE("a healthy base does not report destroyed") {
    Base b{Vec2{0.0f, 0.0f}, 30.0f, 1.0f, 100.0f};
    CHECK_FALSE(b.isDestroyed());
}

TEST_CASE("an empty pool is a no-op") {
    Base b{Vec2{0.0f, 0.0f}, 30.0f, 100.0f, 100.0f};
    EnemyPool p;
    CHECK(ls::applyArrivals(p, b) == 0u);
    CHECK(b.health == doctest::Approx(100.0f));
}
```

Add `tests/test_base.cpp` and `src/sim/Base.cpp` to their target lists.

- [ ] **Step 2: Run test to verify it fails**

Run:
```bash
cd /Users/mariolandaburu/Desktop/laststand && cmake --build build -j 2>&1 | tail -20
```
Expected: FAIL — `'sim/Base.h' file not found`

- [ ] **Step 3: Write the implementation**

Create `src/sim/Base.h`:

```cpp
#pragma once
#include <cstdint>

#include "math/Vec2.h"
#include "sim/EnemyPool.h"

namespace ls {

struct Base {
    Vec2  position{0.0f, 0.0f};
    float radius    = 30.0f;
    float health    = 1000.0f;
    float maxHealth = 1000.0f;

    bool isDestroyed() const { return health <= 0.0f; }
};

// Despawns every enemy within the base radius, each dealing damage equal to
// its remaining health (GDD 4.5). Returns how many arrived.
uint32_t applyArrivals(EnemyPool& pool, Base& base);

}  // namespace ls
```

Create `src/sim/Base.cpp`:

```cpp
#include "sim/Base.h"

namespace ls {

uint32_t applyArrivals(EnemyPool& pool, Base& base) {
    const float radiusSq = base.radius * base.radius;
    uint32_t arrived = 0u;

    // Iterate downward: kill() swap-removes the last element into the hole,
    // and walking down means that element has already been examined.
    for (uint32_t i = pool.count(); i-- > 0u;) {
        if (distanceSq(pool.position[i], base.position) > radiusSq) continue;

        base.health -= pool.health[i];
        if (base.health < 0.0f) base.health = 0.0f;
        pool.kill(i);
        ++arrived;
    }
    return arrived;
}

}  // namespace ls
```

- [ ] **Step 4: Run tests to verify they pass**

Run:
```bash
cd /Users/mariolandaburu/Desktop/laststand && cmake --build build -j && ctest --test-dir build --output-on-failure
```
Expected: PASS. If "mixed arrivals" fails, the loop is iterating forward and skipping the swapped-in element.

- [ ] **Step 5: Commit**

```bash
cd /Users/mariolandaburu/Desktop/laststand
git add src/sim/Base.h src/sim/Base.cpp tests/test_base.cpp CMakeLists.txt
git commit -m "feat(sim): base HP and arrival damage

Arriving enemies deal damage equal to remaining health rather than a flat
leak count, so a surviving Tank hurts far more than a Grunt and the
failure mode stays legible (GDD 4.5). Reverse iteration keeps the
swap-remove correct."
```

---

## Task 11: World — the tick

**Files:**
- Create: `src/sim/World.h`
- Create: `src/sim/World.cpp`
- Create: `tests/test_world.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: everything from Tasks 3, 6, 7, 8, 9, 10
- Produces: `namespace ls { class World }` with `World(LevelMap levelMap, uint64_t seed)`, `void spawnWave(uint32_t count)`, `void tick(float dt)`, `bool isOver() const`, `uint64_t ticks() const`, `uint32_t totalArrived() const`, `uint64_t stateHash() const`, and const accessors `map()`, `flowField()`, `enemies()`, `base()`. `Renderer` (Task 12) and `Bench` (Task 13) consume this.

`stateHash()` is FNV-1a over every live enemy's position and health plus the base health. It exists so Task 14 can assert that two identically seeded runs produce identical state, and it becomes the golden-hash source for the M5 regression test.

- [ ] **Step 1: Write the failing test**

Create `tests/test_world.cpp`:

```cpp
#include <doctest/doctest.h>
#include "sim/World.h"

using ls::World;

TEST_CASE("a new world is not over and has no enemies") {
    World w{ls::makeM1Map(), 1234u};
    CHECK_FALSE(w.isOver());
    CHECK(w.enemies().count() == 0u);
    CHECK(w.ticks() == 0u);
}

TEST_CASE("spawnWave creates the requested number of enemies") {
    World w{ls::makeM1Map(), 1234u};
    w.spawnWave(100u);
    CHECK(w.enemies().count() == 100u);
}

TEST_CASE("spawned enemies start on walkable, reachable cells") {
    World w{ls::makeM1Map(), 1234u};
    w.spawnWave(100u);
    for (uint32_t i = 0; i < w.enemies().count(); ++i) {
        int cx = -1, cy = -1;
        REQUIRE(w.map().grid.worldToCell(w.enemies().position[i], cx, cy));
        CHECK(w.map().isWalkable(cx, cy));
        CHECK(w.flowField().isReachable(cx, cy));
    }
}

TEST_CASE("tick advances the tick counter") {
    World w{ls::makeM1Map(), 1234u};
    w.tick(1.0f / 60.0f);
    w.tick(1.0f / 60.0f);
    CHECK(w.ticks() == 2u);
}

TEST_CASE("enemies eventually reach the base and damage it") {
    World w{ls::makeM1Map(), 1234u};
    w.spawnWave(100u);
    const float startHealth = w.base().health;

    for (int i = 0; i < 5000 && !w.isOver(); ++i) w.tick(1.0f / 60.0f);

    CHECK(w.totalArrived() > 0u);
    CHECK(w.base().health < startHealth);
}

TEST_CASE("enough enemies destroy the base and the world ends") {
    World w{ls::makeM1Map(), 1234u};
    w.spawnWave(100u);
    for (int i = 0; i < 5000 && !w.isOver(); ++i) w.tick(1.0f / 60.0f);

    CHECK(w.isOver());
    CHECK(w.base().isDestroyed());
}

TEST_CASE("ticking a finished world is a no-op") {
    World w{ls::makeM1Map(), 1234u};
    w.spawnWave(100u);
    for (int i = 0; i < 5000 && !w.isOver(); ++i) w.tick(1.0f / 60.0f);
    REQUIRE(w.isOver());

    const uint64_t before = w.stateHash();
    w.tick(1.0f / 60.0f);
    CHECK(w.stateHash() == before);
}

TEST_CASE("stateHash changes as the world evolves") {
    World w{ls::makeM1Map(), 1234u};
    w.spawnWave(50u);
    const uint64_t a = w.stateHash();
    for (int i = 0; i < 60; ++i) w.tick(1.0f / 60.0f);
    CHECK(w.stateHash() != a);
}
```

Add `tests/test_world.cpp` and `src/sim/World.cpp` to their target lists.

- [ ] **Step 2: Run test to verify it fails**

Run:
```bash
cd /Users/mariolandaburu/Desktop/laststand && cmake --build build -j 2>&1 | tail -20
```
Expected: FAIL — `'sim/World.h' file not found`

- [ ] **Step 3: Write the implementation**

Create `src/sim/World.h`:

```cpp
#pragma once
#include <cstdint>
#include <utility>

#include "ai/FlowField.h"
#include "math/Rng.h"
#include "sim/Base.h"
#include "sim/EnemyPool.h"
#include "sim/LevelMap.h"
#include "sim/MovementSystem.h"

namespace ls {

// Owns the whole simulation. Contains no rendering, no input, no wall-clock
// time — which is what lets it run headless in the benchmark and produce
// bit-identical results from the same seed.
class World {
public:
    World(LevelMap levelMap, uint64_t seed);

    void spawnWave(uint32_t count);
    void tick(float dt);

    bool     isOver() const { return base_.isDestroyed(); }
    uint64_t ticks() const { return ticks_; }
    uint32_t totalArrived() const { return totalArrived_; }

    // FNV-1a over live enemy state and base health. Used by the determinism
    // test and, from M5, by the golden-hash regression test.
    uint64_t stateHash() const;

    const LevelMap&  map() const { return map_; }
    const FlowField& flowField() const { return field_; }
    const EnemyPool& enemies() const { return enemies_; }
    EnemyPool&       enemies() { return enemies_; }
    const Base&      base() const { return base_; }

private:
    LevelMap       map_;
    FlowField      field_;
    EnemyPool      enemies_;
    Base           base_;
    Pcg32          rng_;
    MovementParams movement_;
    uint64_t       ticks_        = 0u;
    uint32_t       totalArrived_ = 0u;
};

}  // namespace ls
```

Create `src/sim/World.cpp`:

```cpp
#include "sim/World.h"

#include <cstring>

namespace ls {

namespace {

constexpr uint64_t kFnvOffset = 1469598103934665603ULL;
constexpr uint64_t kFnvPrime  = 1099511628211ULL;

inline void hashFloat(uint64_t& h, float v) {
    uint32_t bits = 0u;
    std::memcpy(&bits, &v, sizeof(bits));
    for (int b = 0; b < 4; ++b) {
        h ^= static_cast<uint64_t>((bits >> (b * 8)) & 0xFFu);
        h *= kFnvPrime;
    }
}

}  // namespace

World::World(LevelMap levelMap, uint64_t seed)
    : map_(std::move(levelMap)), rng_(seed) {
    field_.build(map_);
    base_.position = map_.baseCenter();
    base_.radius = map_.grid.cellSize() * 1.5f;
}

void World::spawnWave(uint32_t count) {
    if (map_.spawnCells.empty()) return;

    const float jitter = map_.grid.cellSize() * 0.4f;
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t pick =
            rng_.nextBounded(static_cast<uint32_t>(map_.spawnCells.size()));
        const Vec2 centre =
            map_.grid.cellCenterAt(map_.spawnCells[static_cast<size_t>(pick)]);
        const Vec2 pos{centre.x + rng_.nextRange(-jitter, jitter),
                       centre.y + rng_.nextRange(-jitter, jitter)};
        if (enemies_.spawn(pos, 100.0f, 0u) == EnemyPool::kInvalid) return;
    }
}

void World::tick(float dt) {
    if (isOver()) return;

    updateMovement(enemies_, field_, dt, movement_);
    totalArrived_ += applyArrivals(enemies_, base_);
    ++ticks_;
}

uint64_t World::stateHash() const {
    uint64_t h = kFnvOffset;
    const uint32_t n = enemies_.count();
    for (uint32_t i = 0; i < n; ++i) {
        hashFloat(h, enemies_.position[i].x);
        hashFloat(h, enemies_.position[i].y);
        hashFloat(h, enemies_.health[i]);
    }
    hashFloat(h, base_.health);
    return h;
}

}  // namespace ls
```

- [ ] **Step 4: Run tests to verify they pass**

Run:
```bash
cd /Users/mariolandaburu/Desktop/laststand && cmake --build build -j && ctest --test-dir build --output-on-failure
```
Expected: PASS. If "enough enemies destroy the base" fails, raise the tick budget in the test or confirm the flow field routes through the chokepoint — 100 enemies × 100 HP is exactly enough for a 1,000 HP base only if at least 10 arrive.

- [ ] **Step 5: Commit**

```bash
cd /Users/mariolandaburu/Desktop/laststand
git add src/sim/World.h src/sim/World.cpp tests/test_world.cpp CMakeLists.txt
git commit -m "feat(sim): World owning the tick, plus FNV-1a state hash

World contains no rendering, input or wall-clock time, so it runs
headless and reproduces bit-identically from a seed. stateHash() backs
the determinism test now and the M5 golden-hash regression later."
```

---

## Task 12: Renderer and the live game loop

**Files:**
- Create: `src/render/Renderer.h`
- Create: `src/render/Renderer.cpp`
- Modify: `src/main.cpp` (replace the Task 1 placeholder entirely)
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ls::World` (Task 11), `ls::FixedTimestep` (Task 4)
- Produces: `namespace ls { struct DebugFlags { bool showFlowField; bool showGrid; }; class Renderer { public: void draw(const World&, float alpha, const DebugFlags&, double frameMs, double tickMs); }; }`

**No test file.** Rendering correctness is verified by looking at it (Step 5), not by assertion. Writing tests against raylib draw calls would test the mock, not the picture. This is the only task in M1 without a test, and that is deliberate.

Positions are interpolated between `prevPosition` and `position` using the timestep's `alpha`, which is the whole point of having decoupled the two rates in Task 4.

- [ ] **Step 1: Write the renderer**

Create `src/render/Renderer.h`:

```cpp
#pragma once
#include "sim/World.h"

namespace ls {

struct DebugFlags {
    bool showFlowField = false;
    bool showGrid      = false;
};

class Renderer {
public:
    // alpha is FixedTimestep::alpha() — the interpolation factor between
    // the previous and current tick.
    void draw(const World& world,
              float alpha,
              const DebugFlags& flags,
              double frameMs,
              double tickMs);
};

}  // namespace ls
```

Create `src/render/Renderer.cpp`:

```cpp
#include "render/Renderer.h"

#include <cstdio>
#include <raylib.h>

namespace ls {

namespace {

// Palette from GDD 12.1: cold player, warm world, sickly enemies.
constexpr Color kBackground{12, 10, 10, 255};
constexpr Color kWall{46, 38, 34, 255};
constexpr Color kWallEdge{120, 70, 40, 255};
constexpr Color kEnemy{168, 200, 120, 255};
constexpr Color kBaseGood{150, 220, 255, 255};
constexpr Color kBaseBad{255, 90, 70, 255};
constexpr Color kText{200, 220, 240, 255};
constexpr Color kFlow{70, 90, 110, 255};

inline Vector2 toRl(Vec2 v) { return Vector2{v.x, v.y}; }

}  // namespace

void Renderer::draw(const World& world,
                    float alpha,
                    const DebugFlags& flags,
                    double frameMs,
                    double tickMs) {
    const LevelMap& map = world.map();
    const Grid&     grid = map.grid;
    const float     cs = grid.cellSize();

    BeginDrawing();
    ClearBackground(kBackground);

    // --- walls -------------------------------------------------------------
    for (int cy = 0; cy < grid.rows(); ++cy) {
        for (int cx = 0; cx < grid.cols(); ++cx) {
            if (map.isWalkable(cx, cy)) continue;
            const float x = static_cast<float>(cx) * cs;
            const float y = static_cast<float>(cy) * cs;
            DrawRectangleV(Vector2{x, y}, Vector2{cs, cs}, kWall);
        }
    }

    // --- optional debug overlays -------------------------------------------
    if (flags.showGrid) {
        for (int cx = 0; cx <= grid.cols(); ++cx) {
            const float x = static_cast<float>(cx) * cs;
            DrawLineV(Vector2{x, 0.0f}, Vector2{x, grid.worldHeight()},
                      Color{40, 40, 46, 120});
        }
        for (int cy = 0; cy <= grid.rows(); ++cy) {
            const float y = static_cast<float>(cy) * cs;
            DrawLineV(Vector2{0.0f, y}, Vector2{grid.worldWidth(), y},
                      Color{40, 40, 46, 120});
        }
    }

    if (flags.showFlowField) {
        for (int cy = 0; cy < grid.rows(); ++cy) {
            for (int cx = 0; cx < grid.cols(); ++cx) {
                if (!world.flowField().isReachable(cx, cy)) continue;
                const Vec2 c = grid.cellCenter(cx, cy);
                const Vec2 d = world.flowField().dirAt(cx, cy);
                DrawLineV(toRl(c), toRl(c + d * (cs * 0.4f)), kFlow);
            }
        }
    }

    // --- enemies (LOD tier 2: one directional shape each) -------------------
    const EnemyPool& e = world.enemies();
    const uint32_t   n = e.count();
    for (uint32_t i = 0; i < n; ++i) {
        const Vec2 p = lerp(e.prevPosition[i], e.position[i], alpha);
        const Vec2 fwd = normalized(e.velocity[i]);
        // A degenerate (stationary) enemy still needs an orientation.
        const Vec2 dir = (lengthSq(fwd) > 0.0f) ? fwd : Vec2{1.0f, 0.0f};
        const Vec2 side{-dir.y, dir.x};

        const Vec2 tip  = p + dir * 5.0f;
        const Vec2 back = p - dir * 3.0f;
        // If the triangles render invisible, raylib has culled them for
        // winding order — swap the last two vertices.
        DrawTriangle(toRl(tip),
                     toRl(back - side * 3.0f),
                     toRl(back + side * 3.0f),
                     kEnemy);
    }

    // --- base ---------------------------------------------------------------
    const Base& b = world.base();
    const float frac = (b.maxHealth > 0.0f) ? (b.health / b.maxHealth) : 0.0f;
    const Color baseColor = (frac > 0.35f) ? kBaseGood : kBaseBad;
    DrawCircleV(toRl(b.position), b.radius, baseColor);
    DrawCircleLinesV(toRl(b.position), b.radius + 4.0f, baseColor);

    // --- overlay (raylib text; ImGui arrives in M2) -------------------------
    char line[256];
    std::snprintf(line, sizeof(line),
                  "entities %u   frame %.2f ms   tick %.3f ms   fps %d",
                  n, frameMs, tickMs, GetFPS());
    DrawText(line, 12, 12, 18, kText);

    std::snprintf(line, sizeof(line), "base %.0f / %.0f    arrived %u    ticks %llu",
                  static_cast<double>(b.health),
                  static_cast<double>(b.maxHealth),
                  world.totalArrived(),
                  static_cast<unsigned long long>(world.ticks()));
    DrawText(line, 12, 34, 18, kText);

    DrawText("[F] flow field   [G] grid   [SPACE] spawn 100   [R] reset",
             12, 56, 16, Color{120, 130, 145, 255});

    if (world.isOver()) {
        DrawText("BASE DESTROYED", 12, 84, 32, kBaseBad);
    }

    EndDrawing();
}

}  // namespace ls
```

Add `src/render/Renderer.cpp` to `laststand_core`.

- [ ] **Step 2: Replace src/main.cpp with the real loop**

```cpp
#include <chrono>
#include <memory>
#include <raylib.h>

#include "core/FixedTimestep.h"
#include "render/Renderer.h"
#include "sim/World.h"

namespace {

constexpr uint64_t kSeed = 1234u;

std::unique_ptr<ls::World> makeWorld() {
    auto w = std::make_unique<ls::World>(ls::makeM1Map(), kSeed);
    w->spawnWave(100u);
    return w;
}

}  // namespace

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "LAST STAND");
    SetTargetFPS(0);   // vsync governs pacing; never cap with a sleep

    auto world = makeWorld();
    ls::FixedTimestep timestep{60.0, 0.25};
    ls::Renderer renderer;
    ls::DebugFlags flags;

    double lastTickMs = 0.0;

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_F)) flags.showFlowField = !flags.showFlowField;
        if (IsKeyPressed(KEY_G)) flags.showGrid = !flags.showGrid;
        if (IsKeyPressed(KEY_SPACE)) world->spawnWave(100u);
        if (IsKeyPressed(KEY_R)) world = makeWorld();

        const double frameSeconds = static_cast<double>(GetFrameTime());
        const int ticks = timestep.advance(frameSeconds);

        if (ticks > 0) {
            const auto t0 = std::chrono::steady_clock::now();
            for (int i = 0; i < ticks; ++i) {
                world->tick(static_cast<float>(timestep.tickSeconds()));
            }
            const auto t1 = std::chrono::steady_clock::now();
            const double totalMs =
                std::chrono::duration<double, std::milli>(t1 - t0).count();
            lastTickMs = totalMs / static_cast<double>(ticks);
        }

        renderer.draw(*world,
                      static_cast<float>(timestep.alpha()),
                      flags,
                      frameSeconds * 1000.0,
                      lastTickMs);
    }

    CloseWindow();
    return 0;
}
```

- [ ] **Step 3: Build**

Run:
```bash
cd /Users/mariolandaburu/Desktop/laststand && cmake --build build -j
```
Expected: builds clean, zero warnings.

- [ ] **Step 4: Run the tests to confirm nothing regressed**

Run:
```bash
cd /Users/mariolandaburu/Desktop/laststand && ctest --test-dir build --output-on-failure
```
Expected: all previous tests still pass.

- [ ] **Step 5: Verify visually — this is the acceptance gate**

Run `./build/laststand` and confirm every one of these:

1. 100 green triangles spawn on the left edge and stream to the right.
2. They **funnel through the chokepoint** between the two wall blocks rather than passing through walls.
3. Pressing `F` draws flow-field arrows, and the arrows visibly curve around the wall blocks toward the gap.
4. Pressing `G` toggles the grid.
5. On reaching the blue base circle, enemies disappear and `base` in the overlay drops.
6. Pressing `SPACE` adds 100 more; holding it turns the base red, then prints `BASE DESTROYED`.
7. Pressing `R` resets to a fresh 100.
8. Movement is **smooth**, not stepped — this confirms interpolation is working. If it looks juddery, `alpha` is not being passed through.

- [ ] **Step 6: Commit**

```bash
cd /Users/mariolandaburu/Desktop/laststand
git add src/render/ src/main.cpp CMakeLists.txt
git commit -m "feat(render): tier-2 enemy rendering and the live game loop

Positions interpolate between prevPosition and position using the
timestep alpha, so rendering is smooth at any refresh rate while the sim
stays locked to 60Hz. Debug overlay uses raylib text; ImGui lands in M2."
```

---

## Task 13: CLI parsing and headless benchmark

**Files:**
- Create: `src/app/Cli.h`
- Create: `src/app/Cli.cpp`
- Create: `src/app/Bench.h`
- Create: `src/app/Bench.cpp`
- Create: `tests/test_cli.cpp`
- Modify: `src/main.cpp` (dispatch on the parsed options)
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ls::World` (Task 11)
- Produces:
  - `namespace ls { struct Options { bool bench; bool noRender; bool help; uint64_t ticks; uint64_t seed; uint32_t spawn; }; Options parseArgs(int argc, const char* const* argv); const char* usageText(); }`
  - `namespace ls { struct BenchResult { uint64_t ticks; uint32_t peakEntities; uint32_t arrived; double minMs; double meanMs; double p99Ms; uint64_t stateHash; }; BenchResult runBench(const Options&); void printBench(const BenchResult&); bool writeBenchCsv(const BenchResult&, const char* path); }`

Per GDD §14.6, headless benchmarking is what makes the performance work in §15 measurable rather than vibes-based. It is built now, in M1, so that every later milestone has a number to compare against.

Only `parseArgs` is unit-tested — it is pure and it is where an off-by-one silently ruins a benchmark. `runBench` is verified by running it (Step 6).

- [ ] **Step 1: Write the failing test**

Create `tests/test_cli.cpp`:

```cpp
#include <doctest/doctest.h>
#include "app/Cli.h"

using ls::Options;

namespace {
Options parse(std::initializer_list<const char*> args) {
    std::vector<const char*> v{"laststand"};
    for (const char* a : args) v.push_back(a);
    return ls::parseArgs(static_cast<int>(v.size()), v.data());
}
}  // namespace

TEST_CASE("defaults") {
    const Options o = parse({});
    CHECK_FALSE(o.bench);
    CHECK_FALSE(o.noRender);
    CHECK_FALSE(o.help);
    CHECK(o.ticks == 10000u);
    CHECK(o.seed == 1234u);
    CHECK(o.spawn == 100u);
}

TEST_CASE("--bench sets the flag") {
    CHECK(parse({"--bench"}).bench);
}

TEST_CASE("--no-render sets the flag") {
    CHECK(parse({"--no-render"}).noRender);
}

TEST_CASE("--ticks takes a value") {
    CHECK(parse({"--ticks", "5000"}).ticks == 5000u);
}

TEST_CASE("--seed takes a value") {
    CHECK(parse({"--seed", "99"}).seed == 99u);
}

TEST_CASE("--spawn takes a value") {
    CHECK(parse({"--spawn", "5000"}).spawn == 5000u);
}

TEST_CASE("flags combine") {
    const Options o = parse({"--bench", "--no-render", "--ticks", "200", "--spawn", "42"});
    CHECK(o.bench);
    CHECK(o.noRender);
    CHECK(o.ticks == 200u);
    CHECK(o.spawn == 42u);
}

TEST_CASE("--help sets the flag") {
    CHECK(parse({"--help"}).help);
    CHECK(parse({"-h"}).help);
}

TEST_CASE("a value flag with no value keeps the default rather than crashing") {
    const Options o = parse({"--ticks"});
    CHECK(o.ticks == 10000u);
}

TEST_CASE("an unknown flag is ignored and does not disturb the rest") {
    const Options o = parse({"--wat", "--spawn", "7"});
    CHECK(o.spawn == 7u);
}

TEST_CASE("usageText is non-empty") {
    CHECK(ls::usageText() != nullptr);
    CHECK(ls::usageText()[0] != '\0');
}
```

Add `tests/test_cli.cpp`, `src/app/Cli.cpp` and `src/app/Bench.cpp` to their target lists.

- [ ] **Step 2: Run test to verify it fails**

Run:
```bash
cd /Users/mariolandaburu/Desktop/laststand && cmake --build build -j 2>&1 | tail -20
```
Expected: FAIL — `'app/Cli.h' file not found`

- [ ] **Step 3: Write Cli**

Create `src/app/Cli.h`:

```cpp
#pragma once
#include <cstdint>
#include <vector>

namespace ls {

struct Options {
    bool     bench    = false;
    bool     noRender = false;
    bool     help     = false;
    uint64_t ticks    = 10000u;
    uint64_t seed     = 1234u;
    uint32_t spawn    = 100u;
};

Options     parseArgs(int argc, const char* const* argv);
const char* usageText();

}  // namespace ls
```

Create `src/app/Cli.cpp`:

```cpp
#include "app/Cli.h"

#include <cstdlib>
#include <cstring>

namespace ls {

namespace {

// Returns true and writes the value when argv[i+1] exists; advances i.
bool takeValue(int argc, const char* const* argv, int& i, uint64_t& out) {
    if (i + 1 >= argc) return false;
    out = std::strtoull(argv[i + 1], nullptr, 10);
    ++i;
    return true;
}

}  // namespace

Options parseArgs(int argc, const char* const* argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (std::strcmp(a, "--bench") == 0) {
            o.bench = true;
        } else if (std::strcmp(a, "--no-render") == 0) {
            o.noRender = true;
        } else if (std::strcmp(a, "--help") == 0 || std::strcmp(a, "-h") == 0) {
            o.help = true;
        } else if (std::strcmp(a, "--ticks") == 0) {
            takeValue(argc, argv, i, o.ticks);
        } else if (std::strcmp(a, "--seed") == 0) {
            takeValue(argc, argv, i, o.seed);
        } else if (std::strcmp(a, "--spawn") == 0) {
            uint64_t v = o.spawn;
            if (takeValue(argc, argv, i, v)) o.spawn = static_cast<uint32_t>(v);
        }
        // Unknown flags are ignored rather than fatal: a benchmark run
        // should never die because of a stale argument in a script.
    }
    return o;
}

const char* usageText() {
    return
        "LAST STAND\n"
        "\n"
        "  laststand                       run the game\n"
        "  laststand --bench [options]     run the headless benchmark\n"
        "\n"
        "Options:\n"
        "  --bench             run the simulation headlessly and report timings\n"
        "  --no-render         alias for --bench (no window is opened)\n"
        "  --ticks <n>         ticks to simulate      (default 10000)\n"
        "  --seed <n>          simulation seed        (default 1234)\n"
        "  --spawn <n>         enemies to spawn       (default 100)\n"
        "  -h, --help          this text\n";
}

}  // namespace ls
```

- [ ] **Step 4: Write Bench**

Create `src/app/Bench.h`:

```cpp
#pragma once
#include <cstdint>

#include "app/Cli.h"

namespace ls {

struct BenchResult {
    uint64_t ticks         = 0u;
    uint32_t peakEntities  = 0u;
    uint32_t arrived       = 0u;
    double   minMs         = 0.0;
    double   meanMs        = 0.0;
    double   p99Ms         = 0.0;
    uint64_t stateHash     = 0u;
};

BenchResult runBench(const Options& options);
void        printBench(const BenchResult& r);
bool        writeBenchCsv(const BenchResult& r, const char* path);

}  // namespace ls
```

Create `src/app/Bench.cpp`:

```cpp
#include "app/Bench.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

#include "sim/World.h"

namespace ls {

BenchResult runBench(const Options& options) {
    World world{makeM1Map(), options.seed};
    world.spawnWave(options.spawn);

    BenchResult r;
    r.peakEntities = world.enemies().count();

    std::vector<double> samples;
    samples.reserve(static_cast<size_t>(options.ticks));

    const float dt = 1.0f / 60.0f;
    for (uint64_t i = 0; i < options.ticks; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        world.tick(dt);
        const auto t1 = std::chrono::steady_clock::now();
        samples.push_back(
            std::chrono::duration<double, std::milli>(t1 - t0).count());
    }

    r.ticks = world.ticks();
    r.arrived = world.totalArrived();
    r.stateHash = world.stateHash();

    if (!samples.empty()) {
        double sum = 0.0;
        for (const double s : samples) sum += s;
        r.meanMs = sum / static_cast<double>(samples.size());

        std::sort(samples.begin(), samples.end());
        r.minMs = samples.front();
        const size_t idx = static_cast<size_t>(
            static_cast<double>(samples.size() - 1) * 0.99);
        r.p99Ms = samples[idx];
    }
    return r;
}

void printBench(const BenchResult& r) {
    std::printf("ticks_run      %llu\n", static_cast<unsigned long long>(r.ticks));
    std::printf("peak_entities  %u\n", r.peakEntities);
    std::printf("arrived        %u\n", r.arrived);
    std::printf("tick_min_ms    %.6f\n", r.minMs);
    std::printf("tick_mean_ms   %.6f\n", r.meanMs);
    std::printf("tick_p99_ms    %.6f\n", r.p99Ms);
    std::printf("state_hash     %llu\n", static_cast<unsigned long long>(r.stateHash));
}

bool writeBenchCsv(const BenchResult& r, const char* path) {
    std::FILE* f = std::fopen(path, "w");
    if (f == nullptr) return false;
    std::fprintf(f, "stage,peak_entities,ticks,arrived,tick_min_ms,tick_mean_ms,tick_p99_ms\n");
    std::fprintf(f, "stage0,%u,%llu,%u,%.6f,%.6f,%.6f\n",
                 r.peakEntities,
                 static_cast<unsigned long long>(r.ticks),
                 r.arrived, r.minMs, r.meanMs, r.p99Ms);
    std::fclose(f);
    return true;
}

}  // namespace ls
```

- [ ] **Step 5: Wire the dispatch into main.cpp**

Replace the top of `main()` in `src/main.cpp`. Change the signature to `int main(int argc, char** argv)` and insert this before `SetConfigFlags`:

```cpp
    const ls::Options options = ls::parseArgs(argc, argv);

    if (options.help) {
        std::printf("%s", ls::usageText());
        return 0;
    }

    if (options.bench || options.noRender) {
        const ls::BenchResult r = ls::runBench(options);
        ls::printBench(r);
        return 0;
    }
```

Add these includes to `src/main.cpp`:

```cpp
#include <cstdio>
#include "app/Bench.h"
#include "app/Cli.h"
```

Also change `makeWorld()` to take the seed and spawn count from the options rather than the `kSeed` constant:

```cpp
std::unique_ptr<ls::World> makeWorld(uint64_t seed, uint32_t spawn) {
    auto w = std::make_unique<ls::World>(ls::makeM1Map(), seed);
    w->spawnWave(spawn);
    return w;
}
```

and update its two call sites in the loop to `makeWorld(options.seed, options.spawn)`.

- [ ] **Step 6: Build, test, and run the benchmark**

Run:
```bash
cd /Users/mariolandaburu/Desktop/laststand && cmake --build build -j && ctest --test-dir build --output-on-failure && ./build/laststand --bench --ticks 2000 --spawn 500
```
Expected: tests pass; the benchmark prints seven lines and exits **without opening a window**. `tick_mean_ms` for 500 entities should be well under 1 ms.

- [ ] **Step 7: Commit**

```bash
cd /Users/mariolandaburu/Desktop/laststand
git add src/app/ src/main.cpp tests/test_cli.cpp CMakeLists.txt
git commit -m "feat(app): CLI parsing and headless benchmark mode

--bench runs the sim with no window and reports min/mean/p99 tick time
plus a state hash. Built in M1 so every later milestone has a number to
compare against rather than an impression (GDD 14.6, 15)."
```

---

## Task 14: Determinism test and enforced layering

**Files:**
- Create: `tests/test_determinism.cpp`
- Create: `tools/check_layering.sh`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ls::World::stateHash()` (Task 11), `ls::runBench` (Task 13)
- Produces: a CTest case named `layering`, and determinism coverage in `laststand_tests`.

**Why self-consistency rather than a golden constant.** The determinism test runs the same seed twice in-process and compares hashes. A hardcoded golden hash would be a constant that cannot be derived by inspection, would need regenerating on every legitimate sim change, and would fail confusingly across compilers. The self-consistency form catches every bug that actually matters here — hidden global state, uninitialised memory, iteration-order dependence, wall-clock leakage into the sim. The golden-hash regression arrives in M5, once the simulation has stopped changing shape.

**Why the layering check is a build step, not a convention.** GDD §14.2 requires that `sim/`, `math/`, `core/` and `ai/` never include raylib or anything from `render/`/`ui/`. That property is what makes headless benchmarking and determinism possible, and it is exactly the kind of rule that decays silently the first time someone wants a quick `DrawCircle` inside a system. Enforced, it cannot decay.

- [ ] **Step 1: Write the failing test**

Create `tests/test_determinism.cpp`:

```cpp
#include <doctest/doctest.h>
#include "sim/World.h"

using ls::World;

namespace {
uint64_t runAndHash(uint64_t seed, uint32_t spawn, int ticks) {
    World w{ls::makeM1Map(), seed};
    w.spawnWave(spawn);
    for (int i = 0; i < ticks; ++i) w.tick(1.0f / 60.0f);
    return w.stateHash();
}
}  // namespace

TEST_CASE("the same seed produces an identical world state") {
    CHECK(runAndHash(1234u, 200u, 600) == runAndHash(1234u, 200u, 600));
}

TEST_CASE("determinism holds over a long run") {
    CHECK(runAndHash(777u, 100u, 5000) == runAndHash(777u, 100u, 5000));
}

TEST_CASE("different seeds produce different states") {
    CHECK(runAndHash(1u, 200u, 600) != runAndHash(2u, 200u, 600));
}

TEST_CASE("spawn placement is reproducible from the seed") {
    World a{ls::makeM1Map(), 42u};
    World b{ls::makeM1Map(), 42u};
    a.spawnWave(500u);
    b.spawnWave(500u);

    REQUIRE(a.enemies().count() == b.enemies().count());
    for (uint32_t i = 0; i < a.enemies().count(); ++i) {
        CHECK(a.enemies().position[i].x == doctest::Approx(b.enemies().position[i].x));
        CHECK(a.enemies().position[i].y == doctest::Approx(b.enemies().position[i].y));
    }
}

TEST_CASE("interleaving two worlds does not couple them") {
    // Catches hidden global/static state in any system.
    World a{ls::makeM1Map(), 5u};
    World b{ls::makeM1Map(), 5u};
    a.spawnWave(100u);
    b.spawnWave(100u);

    for (int i = 0; i < 600; ++i) {
        a.tick(1.0f / 60.0f);
        b.tick(1.0f / 60.0f);
    }
    CHECK(a.stateHash() == b.stateHash());
}
```

Add `tests/test_determinism.cpp` to `laststand_tests`.

- [ ] **Step 2: Run test to verify it compiles and passes**

Run:
```bash
cd /Users/mariolandaburu/Desktop/laststand && cmake --build build -j && ctest --test-dir build --output-on-failure
```
Expected: PASS. A failure here means hidden global state, uninitialised memory, or wall-clock time inside the sim — treat it as a serious bug and fix the cause, never the test.

- [ ] **Step 3: Write the layering check**

Create `tools/check_layering.sh`:

```bash
#!/usr/bin/env bash
# Enforces GDD 14.2: the simulation layer must not depend on rendering.
# This is what makes headless benchmarking and deterministic replay possible.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
violations=0

for dir in sim math core ai; do
    target="$root/src/$dir"
    [ -d "$target" ] || continue
    if grep -rn -E '#include[[:space:]]*[<"](raylib\.h|render/|ui/)' "$target"; then
        echo "LAYERING VIOLATION in src/$dir (see above)" >&2
        violations=1
    fi
done

if [ "$violations" -ne 0 ]; then
    echo "" >&2
    echo "src/{sim,math,core,ai} must never include raylib, render/ or ui/." >&2
    echo "See GDD 14.2. Move the rendering concern into src/render/." >&2
    exit 1
fi

echo "layering OK"
```

Make it executable and register it with CTest:

```bash
cd /Users/mariolandaburu/Desktop/laststand && chmod +x tools/check_layering.sh
```

Add to `CMakeLists.txt`, after the existing `add_test`:

```cmake
add_test(NAME layering
         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_layering.sh)
```

- [ ] **Step 4: Verify the check passes, then verify it actually catches a violation**

Run:
```bash
cd /Users/mariolandaburu/Desktop/laststand && ./tools/check_layering.sh
```
Expected: `layering OK`

Now prove the check works rather than assuming it:
```bash
cd /Users/mariolandaburu/Desktop/laststand && printf '#include <raylib.h>\n' >> src/sim/Grid.h && ./tools/check_layering.sh; echo "exit=$?"
```
Expected: prints the offending line and `LAYERING VIOLATION`, `exit=1`.

Revert the deliberate break:
```bash
cd /Users/mariolandaburu/Desktop/laststand && git checkout src/sim/Grid.h && ./tools/check_layering.sh
```
Expected: `layering OK`

- [ ] **Step 5: Run the full suite**

Run:
```bash
cd /Users/mariolandaburu/Desktop/laststand && ctest --test-dir build --output-on-failure
```
Expected: `100% tests passed, 0 tests failed out of 2` (`unit` and `layering`).

- [ ] **Step 6: Commit**

```bash
cd /Users/mariolandaburu/Desktop/laststand
git add tests/test_determinism.cpp tools/check_layering.sh CMakeLists.txt
git commit -m "test: determinism self-consistency and enforced layering

Same seed must produce an identical state hash, including when two worlds
are interleaved (which catches hidden global state). check_layering.sh
fails the build if sim/math/core/ai ever include raylib or render/ui —
that separation is what makes headless benchmarking possible, so it is
enforced rather than merely documented. Golden-hash regression: M5."
```

---

## Task 15: Record the Stage 0 baseline and close the milestone

**Files:**
- Create: `docs/bench/m1.csv`
- Create: `README.md`
- Modify: `docs/GDD.md` (tick the M1 checklist — no design changes)

**Interfaces:**
- Consumes: the `--bench` output of the `laststand` binary (Task 13)
- Produces: the committed Stage 0 measurement that GDD §15's optimisation curve is drawn from.

Note: `writeBenchCsv` (Task 13) writes a single-row file. The M1 baseline is a *curve* across five entity counts, so it is assembled by hand from five runs here. `writeBenchCsv` becomes the automated path from M5, once the benchmark sweeps counts itself.

**This task's output is the artifact.** The numbers recorded here are what Milestone 5 improves on, and the delta between the two is the single most valuable thing this project produces for a portfolio. Record them honestly, including the bad ones — a 5,000-entity run that takes 40 ms per tick is the *point*.

- [ ] **Step 1: Measure the Stage 0 curve**

Run each of these and note `tick_mean_ms` and `tick_p99_ms`:

```bash
cd /Users/mariolandaburu/Desktop/laststand
for n in 100 500 1000 2000 5000; do
  echo "=== spawn $n ==="
  ./build/laststand --bench --ticks 600 --spawn "$n" | grep -E 'peak_entities|tick_mean_ms|tick_p99_ms'
done
```

Expected shape: cost grows roughly **quadratically** with entity count, because separation is O(n²). If 5,000 entities is not dramatically worse than 1,000, the separation loop is not running — check that `separationRadius` is non-zero and that the enemies are actually within range of each other.

- [ ] **Step 2: Write the results into docs/bench/m1.csv**

Create the file with this header and one row per measurement above, substituting the real numbers:

```csv
stage,peak_entities,ticks,tick_mean_ms,tick_p99_ms,notes
stage0,100,600,<measured>,<measured>,naive O(n^2) separation
stage0,500,600,<measured>,<measured>,naive O(n^2) separation
stage0,1000,600,<measured>,<measured>,naive O(n^2) separation
stage0,2000,600,<measured>,<measured>,naive O(n^2) separation
stage0,5000,600,<measured>,<measured>,naive O(n^2) separation
```

Replace every `<measured>` with the actual figure. **Do not estimate or round to a convenient number** — this file is evidence, and its whole value is that it is real.

- [ ] **Step 3: Write README.md**

```markdown
# LAST STAND

An incremental tower-defense game in C++20, built to run tens of thousands of
agents at 120 fps on a single core before reaching for threads.

> *You don't need to survive forever. You just need to get stronger faster
> than they do.*

**Status:** Milestone 1 of 6 — simulation foundation.

## Build

Requires CMake 3.21+ and a C++20 compiler. All dependencies are fetched.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/laststand
```

## Controls

| Key | Action |
|---|---|
| `F` | toggle flow-field arrows |
| `G` | toggle grid |
| `SPACE` | spawn 100 more enemies |
| `R` | reset |

## Benchmark

The simulation runs headlessly, with no window and no renderer:

```bash
./build/laststand --bench --ticks 10000 --spawn 5000
```

Results are tracked in [`docs/bench/`](docs/bench/) across milestones.

## Tests

```bash
ctest --test-dir build --output-on-failure
```

## Architecture notes

- **Flow-field pathing.** One Dijkstra pass per map at load; per-agent
  movement is then an O(1) grid sample plus a separation force. Per-agent A*
  at these counts is not slow, it is impossible.
- **Typed SoA pools, not a generic ECS.** Five entity types, all known at
  compile time, none needing runtime composition. An archetype ECS here would
  be a month of work solving a problem this game does not have.
- **Virtual dispatch scales with the number of *kinds*, not the number of
  *things*.** Turret targeting strategies are virtual — a few dozen calls per
  second. Enemies have no virtual functions at all: at 50,000 entities × 60 Hz
  that would be three million dependent loads per second.
- **Deterministic fixed-timestep simulation.** The sim layer contains no
  rendering, no input and no wall-clock time, which buys replays, reproducible
  bug reports and headless benchmarking from one property.
- **Optimisation is staged and measured, not assumed.** Milestone 1 ships a
  deliberately naive O(n²) separation loop so that later milestones have a real
  baseline to improve on. See [`docs/bench/`](docs/bench/).

Design document: [`docs/GDD.md`](docs/GDD.md).
```

- [ ] **Step 4: Verify the full milestone acceptance criteria**

Every one of these must hold. Run them in order:

```bash
cd /Users/mariolandaburu/Desktop/laststand
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -3
cmake --build build -j 2>&1 | grep -iE 'warning|error' || echo "NO WARNINGS"
ctest --test-dir build --output-on-failure
./build/laststand --bench --ticks 10000 --spawn 100
```

Checklist, from GDD §17.3:

1. [ ] Clean-clone configure and build succeed with **zero warnings**
2. [ ] 100 Grunts spawn, flow along the map, funnel through the chokepoint, reach the base and reduce it to 0 HP
3. [ ] The overlay shows entity count, frame time and tick time live
4. [ ] `SPACE` raises the count to 5,000 and **it runs badly** — that is the Stage 0 baseline, not a bug
5. [ ] `--bench --ticks 10000 --no-render` prints timings, and results are committed to `docs/bench/m1.csv`
6. [ ] `ctest` passes both `unit` and `layering`
7. [ ] Committed with the M1 baseline numbers in the message

- [ ] **Step 5: Commit**

```bash
cd /Users/mariolandaburu/Desktop/laststand
git add README.md docs/bench/m1.csv docs/GDD.md
git commit -m "docs: M1 complete — Stage 0 performance baseline recorded

100 enemies flow through the chokepoint to the base; the base dies.
Deterministic fixed-timestep sim, headless benchmark, enforced layering.

Stage 0 baseline (naive O(n^2) separation, 600 ticks):
  see docs/bench/m1.csv

This is the number M5 improves on."
```

---

## Milestone 1 exit criteria

M1 is done when all seven items in Task 15 Step 4 are ticked and the tree is committed. At that point the project has:

- a deterministic, headless-capable simulation core
- flow-field pathing that routes a horde around obstacles
- fixed-capacity SoA storage with a no-allocation-per-tick invariant
- a working render loop with interpolation
- a benchmark harness and a committed baseline
- 60+ unit tests and an enforced architectural boundary

and **no combat whatsoever** — which is correct. Turrets are Milestone 2.

## What comes next

Each milestone gets its own plan, written when its predecessor lands. Writing them now would mean inventing signatures against code that does not exist; the design intent is already carried by the GDD.

| Milestone | Focus | Plan |
|---|---|---|
| **M2** | Turrets, spatial hash, projectiles, damage, Dear ImGui | write after M1 |
| **M3** | Levels, battle end, Scrap, 6-node tree, save/load, RETRY — **the milestone that proves the game** | write after M2 |
| **M4** | Cannon, Flamethrower, Runner, Tank, Levels 2–3, 24-node tree, abilities, time controls | write after M3 |
| **M5** | Performance stages 1–3, LOD, 5,000 entities at 120 fps, golden-hash regression | write after M4 |
| **M6** | Juice, audio, Failure Analysis, README with GIF and perf graph | write after M5 |
