# LAST STAND — Milestone 3: The Loop — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Close the run → upgrade → retry cycle. A level spawns its invasion over time (a spawn curve, not a one-shot wave), the battle ends in a victory or defeat, a Battle Report pays Scrap (defeat pays 75%), that Scrap buys nodes on a persistent upgrade tree, the save writes atomically, and RETRY restarts the same level in under 5 seconds. This is the milestone that proves the game: if the loop isn't fun, the design changes here, not the content.

**Spec:** `docs/GDD.md` §4.1 (deterministic invasion), §7 (upgrade tree), §8 (economy + "losing pays"), §9.1 (level table), §13.2 (Battle Report), §14.2 (module layout), §14.8 (persistence).

**Architecture (unchanged):** `sim/` stays deterministic, fixed-timestep, headless-capable, no allocation in-tick. Two new first-party layers arrive now, matching GDD §14.2:
- `gameplay/` — `Level`, `SpawnDirector`, `UpgradeTree`, `Progression`. Depends on `sim/`; must not depend on `render/`/`ui/`/raylib (so payouts, costs and spawn curves stay unit-testable and headless).
- `persist/` — `SaveGame`, versioned binary, atomic write. Depends on nothing but the standard library.

---

## Deviations from spec, recorded

1. **Dear ImGui is deferred to M4, permanently justified now.** rlImGui has no tagged release, so it cannot be version-pinned via FetchContent; both M1 and M2 plans pushed it for this reason. M3's value is the loop and the numbers that drive it — all of which are *more* testable with a plain raylib `DrawText` overlay plus pure C++ in `gameplay/`/`persist/`. ImGui lands as its own isolated task in M4 with live tuning sliders for the tree and payout, where it can't block the loop.
2. **Prepare-phase turret placement is minimal.** GDD §4.2 has a full Prepare screen (place turrets, assign targeting). M3 ships only *click-a-hardpoint-to-place-a-Machine-Gun* via the now-needed `Rect` hit-test; targeting-mode assignment and turret selection are M4. Four hardpoints are pre-placed by default so a battle is always one RETRY away.
3. **Diminishing replays (§8.4) are implemented but not yet surfaced in UI.** The payout math — including the per-level clear-count table — is fully implemented and tested in `Progression`, since it is pure and core to "the optimal strategy must never be replaying Level 1". It is wired into the Battle Report from day one.
4. **One level (Level 1 "The Outskirts") ships this milestone.** Levels 2–3 with Runner/Tank are M4 content. The `Level` structure and spawn director are built to take a per-level authoring table so adding levels is data, not code.

Everything else maps to Tasks 1–9 below.

---

## Global Constraints (carried forward)

- C++20, no extensions, `-Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion` on first-party targets only.
- `sim/`, `math/`, `core/`, `ai/` **and now `gameplay/`, `persist/`** must never include raylib / `render/` / `ui/` — extended in `tools/check_layering.sh`.
- Fixed 60 Hz tick; determinism via the seeded `Pcg32`; no wall-clock in `sim/` or `gameplay/`.
- No heap allocation inside a tick (the spawn director and level schedule are precomputed at load).
- Enemy pool capacity 100'000. Tick rate 60 Hz.

---

## File structure (additions / changes)

```
laststand/
├── src/
│   ├── math/
│   │   └── Rect.h                       NEW  AABB + contains (deferred from M1)
│   ├── gameplay/
│   │   ├── Level.h/.cpp                 NEW  Level struct + Level 1 authoring + spawn schedule
│   │   ├── SpawnDirector.h/.cpp         NEW  spawns enemies per the schedule over time
│   │   ├── UpgradeTree.h/.cpp           NEW  6 nodes, cost curve, purchase/refund, bonuses
│   │   └── Progression.h/.cpp           NEW  payout formula, personal bests, diminishing replays
│   ├── persist/
│   │   └── SaveGame.h/.cpp              NEW  versioned binary save/load, atomic write
│   ├── sim/
│   │   └── World.h/.cpp                 CHANGE  victory condition, battle-result extraction, base regen
│   ├── app/
│   │   ├── Session.h/.cpp               NEW  phase state machine: battle/report/tree/retry
│   │   └── Bench.cpp                    CHANGE  measure the full Loop (spawn director, not one-shot wave)
│   ├── render/Renderer.h/.cpp           CHANGE  draw hardpoint hit-test highlight, report/tree text
│   └── main.cpp                         CHANGE  drive the Session, RETRY/upgrade keys
├── tests/
│   ├── test_rect.cpp                    NEW
│   ├── test_level.cpp                   NEW
│   ├── test_spawndirector.cpp           NEW
│   ├── test_upgradetree.cpp             NEW
│   ├── test_progression.cpp             NEW
│   ├── test_savegame.cpp                NEW
│   └── test_determinism.cpp             CHANGE  determinism with the spawn director
├── tools/check_layering.sh              CHANGE  protect gameplay/ and persist/ too
└── CMakeLists.txt                       CHANGE  new sources + tests
```

---

## Task 1: `Rect` — the deferred AABB

**Files:** `src/math/Rect.h`, `tests/test_rect.cpp`, `CMakeLists.txt`.

Header-only, `namespace ls { struct Rect { Vec2 min; Vec2 max; }; }` with `bool contains(Rect, Vec2)`, `float width(Rect)`, `float height(Rect)`, and `Rect fromCenter(Vec2, float halfW, float halfH)`. This finally justifies itself: hardpoint hit-testing (Task 6) and, later, turret range AABBs and ImGui widgets.

**Tests:** `contains` on the boundary and at the corners; `fromCenter` produces the correct extents; width/height are correct for degenerate and inverted rects.

---

## Task 2: `Level` — deterministic invasion authoring

**Files:** `src/gameplay/Level.h`, `src/gameplay/Level.cpp`, `tests/test_level.cpp`, `CMakeLists.txt`.

```cpp
struct SpawnEvent { float timeSeconds; uint32_t count; };   // a burst at time t

struct Level {
    std::string name;
    uint32_t recommendedPower;
    uint32_t totalEnemies;              // sum of the schedule
    std::vector<SpawnEvent> schedule;   // sorted by time, precomputed
    float killValue;                    // Scrap per kill (economy baseline)
    float depthBonusWeight;             // scales the progress^2 term
    LevelMap map;                       // terrain + hardpoints
};
```

`makeLevel1()` returns "The Outskirts": 100 Grunts, `recommendedPower = 10`, a schedule that opens sparse, peaks mid-battle, and thins out (troughs to breathe — GDD §4.1), `killValue = 3.0f`, `depthBonusWeight = 100.0f`. The schedule is **deterministic** (no RNG in authoring); per-enemy RNG (spawn cell, jitter) still flows from the seeded `Pcg32` in `World`.

**Tests:** `totalEnemies` equals the schedule sum; the schedule is non-empty, sorted, and starts at time 0; `makeLevel1` totals 100; the map has hardpoints.

---

## Task 3: `SpawnDirector` — continuous spawning

**Files:** `src/gameplay/SpawnDirector.h`, `src/gameplay/SpawnDirector.cpp`, `tests/test_spawndirector.cpp`, `CMakeLists.txt`.

- `void update(World&, const Level&, float dt)` advances an internal clock and spawns every `SpawnEvent` whose time has been reached (spawning `count` enemies into the world's spawn cells).
- `bool exhausted(const Level&) const` — true when the schedule is fully emitted.
- Stateless per-battle aside from the clock; constructed fresh each battle.

**Tests:** emitting a schedule over time produces exactly `totalEnemies`; no enemies are spawned before their event time; `exhausted` flips only after the last event; an empty schedule is immediately exhausted and spawns nothing.

---

## Task 4: `World` — victory condition, base regen, result extraction

**Files:** `src/sim/World.h`, `src/sim/World.cpp`, `tests/test_world.cpp`.

- `World` gains an `enemiesSpawned_` counter (incremented by `SpawnDirector` through a new `void registerSpawned(uint32_t)`), and a `bool victory() const` — `!isOver() && enemiesSpawned_ == levelTotal() && enemies().count() == 0`. `World` stores `levelTotal_` so `victory()` is self-contained.
- `isOver()` becomes "battle finished" (defeat **or** victory); `isDefeat()` = base destroyed; `isVictory()` = the above. Existing `isOver()` callers that mean "base died" switch to `isDefeat()`. This is the one place M3 must touch existing M1/M2 semantics, and it is deliberate: a battle now has three states (running, won, lost).
- Optional base regen: `Base::regenPerSecond` (default 0, so M1/M2 tests are unchanged), applied in `tick()` capped at `maxHealth`.

**Tests:** a world whose last enemy is killed and whose spawn total is reached reports `victory()`; a base that hits 0 reports `isDefeat()` and never `victory()`; regen heals toward `maxHealth` without overshoot and never revives a destroyed base; M1/M2 behaviour (no turret, base falls) still passes.

---

## Task 5: `UpgradeTree` — six nodes, cost curve, bonuses

**Files:** `src/gameplay/UpgradeTree.h`, `src/gameplay/UpgradeTree.cpp`, `tests/test_upgradetree.cpp`, `CMakeLists.txt`.

```cpp
enum class NodeId : uint8_t { Damage, FireRate, Range, BaseHp, BaseRegen, Economy };

struct Bonuses {
    float damageMult  = 1.0f;   // multiplicative
    float fireRateMult = 1.0f;
    float rangeMult   = 1.0f;
    float baseBonusHp = 0.0f;   // additive
    float baseRegen   = 0.0f;   // additive hp/sec
    float scrapMult   = 1.0f;
};

class UpgradeTree {
public:
    uint32_t cost(NodeId) const;              // base * 1.35^n for n owned levels
    bool     canAfford(NodeId, uint32_t scrap) const;
    bool     purchase(NodeId, uint32_t& scrap);   // deducts scrap, increments level
    void     respecAll(uint32_t& scrap);          // refund 100% (GDD 7.4)
    uint32_t level(NodeId) const;
    Bonuses  bonuses() const;                     // folds levels into effects
    uint32_t totalSpent() const;
};
```

Six nodes, each a repeatable "stat" node (levels stack, cost curves per GDD §7.2 `cost = base * 1.35^n`). Bases:

| Node | Branch | Base cost | Effect per level |
|---|---|---|---|
| Damage | FIREPOWER | 40 | ×1.20 |
| FireRate | FIREPOWER | 40 | ×1.15 |
| Range | COVERAGE | 35 | ×1.12 |
| BaseHp | DEFENSE | 50 | +300 |
| BaseRegen | DEFENSE | 60 | +2 HP/s |
| Economy | ECONOMY | 45 | ×1.20 |

Anti-inflation note (GDD §7.3) applies from the start: the six nodes here are stat nodes because M3 is proving the *loop*; behaviour-changing nodes (the transformation lines) are M4 content and the "1-in-3 changes behaviour" rule is checked again when they land.

**Tests:** cost matches `base * 1.35^n` exactly; a purchase deducts the right scrap and increments the level; a purchase fails without touching scrap when unaffordable; `respecAll` returns 100% and zeroes the tree; `bonuses()` composes levels correctly at 0, 1 and 3 purchases.

---

## Task 6: `Progression` — payout, bests, diminishing replays

**Files:** `src/gameplay/Progression.h`, `src/gameplay/Progression.cpp`, `tests/test_progression.cpp`, `CMakeLists.txt`.

```cpp
struct BattleResult {
    bool     victory;
    uint32_t kills;
    uint32_t totalEnemies;      // spawned during the battle
    uint32_t previousBest;      // this level's best before the battle
    uint32_t clearCount;        // prior successful clears of this level
};

struct Payout {
    uint32_t scrap;
    uint32_t killScrap;         // breakdown for the report
    uint32_t depthScrap;
    uint32_t bestBonus;
    bool     newBest;
    float    multiplier;        // defeat 0.75 and/or diminishing replay
};

Payout computePayout(const BattleResult&, float killValue,
                     float depthWeight, float scrapMult);
```

Formula (§8.2, §8.3, §8.5, §8.4), in fixed order:
1. `progress = totalEnemies ? kills / totalEnemies : 0`.
2. `killScrap = kills * killValue`.
3. `depthScrap = depthWeight * progress²`.
4. `bestBonus = (kills > previousBest && previousBest > 0) ? 50 : 0`; `newBest` also true when `previousBest == 0` (first run), but no bonus on a first clear.
5. `victory` → replay factor from the clear-count table `{1.0, 0.7, 0.5, 0.35, 0.25}` (floor 0.25 after the 5th); defeat → `0.75` flat.
6. `scrap = round((killScrap + depthScrap + bestBonus) * scrapMult * factor)`.

**Tests:** a 0-kill defeat pays the depth floor correctly; 100% progress beats 90% (the squared term); defeat pays exactly 75% of the identical victory (pre-replay); the replay table floors at 0.25; `scrapMult` scales linearly; personal-best bonus fires on a new best (including a *loss* that beats the prior best) and not otherwise.

---

## Task 7: `SaveGame` — versioned, atomic persistence

**Files:** `src/persist/SaveGame.h`, `src/persist/SaveGame.cpp`, `tests/test_savegame.cpp`, `CMakeLists.txt`.

- `struct SaveData { uint32_t version; uint32_t scrap; std::array<uint32_t, 6> nodeLevels; std::array<uint32_t, 8> bestKills; std::array<uint32_t, 8> clearCounts; };`
- `bool save(const SaveData&, const char* path)` — magic `'LSTD'`, version `1`, little-endian fixed-width fields, written to `path + ".tmp"`, `fsync`, then `rename` over the target (GDD §14.8).
- `bool load(SaveData&, const char* path)` — validates magic + version, returns `false` (and leaves a default `SaveData`) on truncation/corruption/absent file.
- `std::vector<uint8_t> serialize(const SaveData&)` / `bool deserialize(const uint8_t*, size_t, SaveData&)` for pure round-trip testing without touching the filesystem.

**Tests:** serialize→deserialize round-trips every field; a corrupted magic rejects; a truncated buffer rejects; `save` then `load` returns an identical `SaveData`; version mismatch rejects.

---

## Task 8: `Session` — the phase state machine and the RETRY loop

**Files:** `src/app/Session.h`, `src/app/Session.cpp`, `src/main.cpp`, `src/render/Renderer.*`.

`Session` is the app-level orchestrator (may touch everything, not unit-tested — the pure parts already are). It owns `SaveData`, an `UpgradeTree`, the current `Level`, a `World`, a `SpawnDirector`, and a `Phase` enum:

```
Prepare → Battle → Report → (Upgrade tree) → back to Prepare (RETRY)
```

Key responsibilities and the milestone's acceptance:
- **Prepare** (pre-RETRY): click a hardpoint (via `Rect::contains` on the mouse in world space) to place a turret; four default turrets are pre-placed. `ENTER`/`SPACE` starts the battle.
- **Battle:** run the fixed-timestep `World`, drive `SpawnDirector`, end on `isOver()` with a `BattleResult`.
- **Report:** `computePayout`, show kill/depth/best breakdown as `DrawText`.
- **Tree:** keys `1..6` buy a node; `X` respecs all. Scrap, costs, and affordable-vs-dim are shown.
- **RETRY (`R`) and window/skip discipline (GDD §13.1):** RETRY is one keypress from Report or Tree, always the same key, no confirmation dialog, and it rebuilds a fresh `World` from the same `Level` + current bonuses. The measured path (report → `R` → battle running) must be **sub-5-second**; it is asserted in `main` by timing the reset and printed to the overlay.

The renderer draws the report panel and the tree as `DrawText`; `Session` exposes the small `struct` the renderer needs (phase, scrap, kills, best, payout, node costs/levels). No living-player UI framework — see deviation 1.

---

## Task 9: Close the milestone

- Extend `tools/check_layering.sh` to protect `gameplay` and `persist` (Task 1 covers this change).
- `Bench.cpp` drives the *Loop*, not a one-shot wave: build the Level, spawn via `SpawnDirector`, and tick until the battle ends (capped), measuring per-tick cost including spawn + combat + victory check.
- Commit `docs/bench/m3.csv` (same five spawn counts, but as level schedules).
- Update `README.md` (status → M3; controls for report/tree/retry).
- Changelog + push, per the project's commit-per-milestone discipline.

---

## Milestone 3 exit criteria

1. Clean-clone configure + build with zero first-party warnings.
2. A Level 1 battle runs to completion (win or lose), pays Scrap, updates bests, writes the save atomically.
3. The upgrade tree buys/refunds nodes with the correct cost curve and changes the next battle's outcome measurably.
4. RETRY restarts in under 5 seconds (timed and printed).
5. `ctest` passes `unit` + `layering`; payout/cost/save round-trip are all green.
6. `--bench` runs the full Loop headlessly; `docs/bench/m3.csv` committed.
7. Determinism holds with the spawn director.

**Explicitly out:** Cannon/Flamethrower, Runner/Tank, Levels 2–3, behaviour-transforming tree nodes, targeting-mode assignment UI, `DENSEST`, Dear ImGui. Those are M4.
