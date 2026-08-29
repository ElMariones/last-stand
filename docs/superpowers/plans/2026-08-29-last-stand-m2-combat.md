# LAST STAND — Milestone 2: Combat — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the full spawn → kill combat loop. Enemies flow along the flow field (M1), turrets acquire targets through a uniform-grid spatial hash, resolve hitscan damage, and corpses get culled. Machine Gun only. Done when one turret visibly defends against a wave of Grunts and every shot-to-kill step is legible in the debug overlay.

**Architecture (unchanged from M1):** the simulation stays in `sim/`, deterministic, fixed-timestep, no allocation inside a tick. Turrets use virtual dispatch for targeting (GDD §14.4 — virtual dispatch scales with the number of *kinds*); enemies stay branch-on-type. The spatial hash is introduced now for turret target acquisition; it is the same class M5 will reuse to replace the O(n²) separation.

**Spec:** `docs/GDD.md` §5 (Turret system), §14.3–14.5 (spatial hash, polymorphism split, hitscan-with-visual).

**Tech Stack:** unchanged (C++20 · raylib 5.5 · doctest 2.4.11 · CMake).

---

## Deviations from spec, recorded

1. **`Rect` is deferred again.** The M1 plan postponed it "to M2 with turret range and hardpoint hit-testing". But M2 does no mouse hit-testing: turrets are placed programmatically on hardpoints, and range is a circle. A `Rect` would be speculative. It arrives in M3 with mouse-driven placement on the Prepare screen.
2. **`DENSEST` targeting is deferred to M4.** GDD §5.4 lists it for the slice, but `DENSEST` means "the position maximising enemies within a splash radius" — meaningless for a single-target hitscan Machine Gun with no splash. M2 ships `FIRST`, `CLOSEST`, `STRONGEST`; `DENSEST` lands with the AoE turrets (Cannon/Flamethrower) that actually have a splash radius.
3. **Dear ImGui is deferred past M2.** The M1 plan scheduled rlImGui for M2, citing that it "can fail without blocking anything". M2's combat is the load-bearing work; rlImGui has no tagged release and would put unpinnable build plumbing on the critical path again. The raylib `DrawText` overlay is extended to show kills/shots/turrets, which is all the M2 acceptance needs. ImGui remains its own task in M3.
4. **Turrets are placed explicitly, not auto-placed.** `World` starts with an empty turret list and gains `placeTurret`. This preserves every M1 test (which constructs a `World` with no turrets and expects the base to fall) and makes the M3 Prepare phase a natural extension. `main.cpp` places one Machine Gun per hardpoint at startup.

Everything else maps directly to Tasks 1–9 below.

---

## Global Constraints (carried from M1, unchanged)

- C++20, no extensions, `-Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion` on first-party targets only.
- `sim/`, `math/`, `core/`, `ai/` never include raylib / `render/` / `ui/` (enforced by `tools/check_layering.sh`).
- Fixed 60 Hz tick, `kTickSeconds = 1/60`; determinism via the explicitly seeded `Pcg32`; no wall-clock anywhere in `sim/`.
- No heap allocation inside a tick: the spatial hash and its query buffer are sized at construction, never resized.
- Enemy pool capacity 100'000.

---

## File structure (additions / changes)

```
laststand/
├── src/
│   ├── sim/
│   │   ├── SpatialHash.h/.cpp      NEW  uniform grid, counting-sort rebuild, radius query
│   │   ├── Targeting.h/.cpp        NEW  TargetingMode + virtual strategies (First/Closest/Strongest)
│   │   ├── Turret.h                NEW  Turret struct + placement
│   │   ├── CombatSystem.h/.cpp     NEW  updateCombat, cullDead, Tracer ring helper
│   │   ├── LevelMap.h/.cpp         CHANGE  add hardpoints; author 4 on the M1 map
│   │   └── World.h/.cpp            CHANGE  place turret, combat integration, tracers, stats accessors
│   ├── render/Renderer.h/.cpp      CHANGE  draw hardpoints, turrets, tracers, range rings, kills overlay
│   └── main.cpp                    CHANGE  place Machine Guns on hardpoints
├── tests/
│   ├── test_spatialhash.cpp        NEW
│   ├── test_targeting.cpp          NEW
│   ├── test_combat.cpp             NEW
│   ├── test_levelmap.cpp           CHANGE  hardpoint assertions
│   └── test_determinism.cpp        CHANGE  turret determinism case
└── CMakeLists.txt                  CHANGE  new sources + tests
```

---

## Task 1: SpatialHash — uniform grid, counting-sort rebuild

**Files:** `src/sim/SpatialHash.h`, `src/sim/SpatialHash.cpp`, `tests/test_spatialhash.cpp`, `CMakeLists.txt`.

**Interfaces:**
- Consumes `ls::Vec2`.
- Produces `namespace ls { class SpatialHash }` with:
  - `SpatialHash(float worldWidth, float worldHeight, float cellSize, uint32_t maxEntities)`
  - `void build(const std::vector<Vec2>& positions, uint32_t count)`
  - `SpatialQuery query(const std::vector<Vec2>& positions, Vec2 center, float radius) const` where `SpatialQuery { const uint32_t* indices; uint32_t count; }`
  - `int cols() const`, `int rows() const`, `float cellSize() const`

**Behaviour contract:**
- `build` runs a counting sort (zero counts → tally → prefix-sum → scatter). Two passes over the entities, no allocation after construction.
- The internal `sorted_` array is sized `maxEntities`; the query buffer is a second internal array also `maxEntities`.
- `query` returns every entity index whose containing cell overlaps the query rectangle **and** whose distance² to `center` is `< radius²` — i.e. the result is exact, not a superset. The returned pointer is valid until the next `query` call (single-threaded, per-turret sequential use).
- Positions outside the world are clamped into the edge cell rather than dropped (they still exist as entities and must still be found).

**Tests:** dimensions round-trip; an entity at the origin is found; an entity beyond `radius` is not found; results respect the cell boundary (an entity just inside vs just outside a cell edge); a query far outside the world returns empty; clamping keeps off-world entities findable.

---

## Task 2: Targeting — virtual dispatch for target selection

**Files:** `src/sim/Targeting.h`, `src/sim/Targeting.cpp`, `tests/test_targeting.cpp`, `CMakeLists.txt`.

**Interfaces:**
- `enum class TargetingMode : uint8_t { First, Closest, Strongest };`
- `class TargetingStrategy` with `virtual uint32_t select(const EnemyPool&, const SpatialHash&, Vec2 origin, float range, Vec2 basePos) const = 0;` returning `EnemyPool::kInvalid` when nothing is in range (or nothing is alive).
- Concrete strategies `FirstStrategy`, `ClosestStrategy`, `StrongestStrategy`; factory `const TargetingStrategy& strategyFor(TargetingMode)`.

**Semantics (all skip enemies with `health <= 0`):**
- `FIRST` — the living in-range enemy closest to the base (min `distanceSq(enemy, basePos)`); "will arrive first".
- `CLOSEST` — min `distanceSq(enemy, origin)`.
- `STRONGEST` — max remaining `health`.

This is the one place in the codebase that uses virtual dispatch on purpose, per GDD §14.4: a handful of strategies called a few hundred times per second, worth the extensibility, and never in a per-enemy hot loop.

**Tests (each against a hand-built pool + spatial hash, with distinct health/positions so ties never occur):** each mode picks the correct index; every mode returns `kInvalid` on an empty hash; dead enemies are skipped; `strategyFor` returns distinct instances per mode.

---

## Task 3: Turret — the turret value type

**Files:** `src/sim/Turret.h`.

```cpp
struct Turret {
    Vec2  position{0,0};
    float range = 160.0f;
    float damage = 5.0f;
    float fireInterval = 0.125f;   // 8 shots/sec (Machine Gun baseline, GDD 5.2)
    float cooldown = 0.0f;
    TargetingMode mode = TargetingMode::First;
    uint32_t shotsFired = 0u;
    uint32_t kills = 0u;
};
```

No test file — the struct is exercised through Task 4 and Task 6. A dedicated `TurretPool` with `{index, generation}` handles arrives in M3 with placement/removal; M2 turrets live in a plain `std::vector<Turret>` owned by `World`, populated once before the battle.

---

## Task 4: CombatSystem — fire, damage, cull

**Files:** `src/sim/CombatSystem.h`, `src/sim/CombatSystem.cpp`, `tests/test_combat.cpp`, `CMakeLists.txt`.

**Interfaces:**
- `constexpr uint32_t kMaxTracers = 256;`
- `struct Tracer { Vec2 from; Vec2 to; float ttl; };` (ttl in seconds)
- `void updateCombat(std::vector<Turret>& turrets, EnemyPool& enemies, const SpatialHash& hash, Vec2 basePos, float dt, std::array<Tracer, kMaxTracers>& tracers, uint32_t& tracerCount);`
- `uint32_t cullDead(EnemyPool& enemies);`
- `inline void appendTracer(std::array<Tracer, kMaxTracers>&, uint32_t&, Vec2 from, Vec2 to, float ttl);`
- `float applyDamage(EnemyPool& enemies, uint32_t i, float damage);` (returns `true` if this shot killed it; clamps health at 0)

**Critical design point — kills are deferred.** `updateCombat` applies damage and leaves a killed enemy at `health == 0` in place. `cullDead` swap-removes all of them once, after every turret has fired. This keeps the spatial hash coherent for the entire turret loop: `kill()` swap-removes and would invalidate every subsequent query's indices mid-tick. Targeting already skips `health <= 0`, so a corpse is never re-selected.

`updateCombat` advances each turret's cooldown by `dt`; when it crosses zero it acquires a target (`strategyFor(mode).select(...)`), applies damage, appends a tracer (muzzle → target, `ttl = 0.08f`), increments `shotsFired`/`kills`, and adds `fireInterval` back to the cooldown.

**Tests:** a single-shot turret kills an enemy in range; damage is applied without overshooting below 0; `cullDead` removes only the dead and returns the right count; cooldown prevents a second shot until `fireInterval` elapses; a turret with no enemies in range never fires; `applyDamage` reports the kill transition correctly.

---

## Task 5: Hardpoints on the level map

**Files:** `src/sim/LevelMap.h`, `src/sim/LevelMap.cpp`, `tests/test_levelmap.cpp`.

- Add `std::vector<Vec2> hardpoints;` to `LevelMap`.
- `makeM1Map()` authors 4 hardpoints in the open field behind the chokepoint, forming a kill-box over the funnel toward the base at cell (58,18):

    | cell | world |
    |---|---|
    | (34, 12) | (690, 250) |
    | (34, 22) | (690, 450) |
    | (46, 10) | (930, 210) |
    | (46, 26) | (930, 530) |

- (The "2 unlocked at start" rule from GDD §4.4 is a Prepare/M3 concern; M2 places on all four.)

**Tests:** M1 map has exactly 4 hardpoints; each hardpoint rests on a walkable cell; the hardpoints lie on the base side of the chokepoint (x > 28).

---

## Task 6: World — combat integration and stats

**Files:** `src/sim/World.h`, `src/sim/World.cpp`, `tests/test_combat.cpp` (or `test_world.cpp`).

- Add members: `std::vector<Turret> turrets_`, `SpatialHash hash_`, `std::array<Tracer,kMaxTracers> tracers_`, `uint32_t tracerCount_`, counters `totalShots_`, `totalKills_`.
- `void placeTurret(Vec2 position)` appends a default Machine Gun at `position`.
- `tick()` order becomes:
  1. `updateMovement(...)`
  2. `hash_.build(enemies_.position, enemies_.count())`
  3. age tracers (decrement `ttl`, swap-remove expired)
  4. `updateCombat(...)` (accumulate `totalShots_`/`totalKills_` from the turrets' deltas)
  5. `cullDead(...)`
  6. `applyArrivals(...)`
- Accessors: `const std::vector<Turret>& turrets() const`, `const std::array<Tracer,kMaxTracers>& tracers() const`, `uint32_t tracerCount() const`, `uint64_t totalShots() const`, `uint32_t totalKills() const`.
- `stateHash()` additionally folds in each turret's `cooldown` so combat state is represented.

The spatial hash is constructed in the `World` constructor from the map's world dimensions, a cell size of `64.0f` (tuned to the ~160-unit turret range), and the enemy pool capacity.

**Tests:** an M1 `World` with no turrets behaves exactly as before (regression); placing a turret in the chokepoint funnel and spawning Grunts results in enemies dying (`totalKills() > 0`) and fewer arrivals than the no-turret baseline; `stateHash` is still self-consistent.

---

## Task 7: Renderer and wiring

**Files:** `src/render/Renderer.h`, `src/render/Renderer.cpp`, `src/main.cpp`.

- `DebugFlags` gains `bool showTurretRange = false;`.
- Draw hardpoints as dim gray rings; turrets as cyan circles with a barrel pointing at the flow direction (or their last target); tracers as fading line segments (`alpha = ttl / 0.08f`); range rings when toggled.
- Overlay adds `kills`, `shots`, and `turrets` counts; adds `[T]` range-rings to the help line.
- `main.cpp`: `makeWorld` places one Machine Gun on each hardpoint after construction; the loop handles `KEY_T`.

No test file — rendering is verified visually, matching the M1 decision.

---

## Task 8: Determinism with turrets

**Files:** `tests/test_determinism.cpp`.

Add a case: two `World`s with the same seed, a turret placed at the same hardpoint, same wave — run N ticks, `stateHash` identical. This catches any wall-clock or global state that a turret path could introduce.

---

## Task 9: Close the milestone

- `README.md` gains a Combat line under Status and a turret note in Controls.
- Record a fresh `docs/bench/m2.csv` (same five spawn counts) so the combat cost of the spatial hash + targeting is visible against the M1 baseline.
- Tick the M2 checklist in `docs/GDD.md` (`§16`, no design change).

---

## Milestone 2 exit criteria

1. Clean-clone configure + build succeed with zero first-party warnings.
2. Turrets visibly fire, enemies visibly die, and the base survives longer than the no-turret baseline.
3. The overlay shows kills / shots / turrets live.
4. `ctest` passes (`unit` + `layering`), with the spatial hash, targeting, and combat tests green.
5. `--bench` still runs headlessly and `docs/bench/m2.csv` is committed.
6. Determinism holds with turrets placed.

**Explicitly out:** Cannon/Flamethrower, Runner/Tank, targeting modes beyond the three above, `DENSEST`, `Rect`, ImGui, Prepare-phase placement, save/load. Those are M3–M4.
