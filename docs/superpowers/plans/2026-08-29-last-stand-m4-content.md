# LAST STAND — Milestone 4: Content — Implementation Plan

> **For agentic workers:** use superpowers:subagent-driven-development or superpowers:executing-plans to implement task-by-task.

**Goal:** Ship the content that turns the loop into a game: two new turrets (Cannon, Flamethrower), two new enemies (Runner, Tank), Levels 2 & 3, `DENSEST` + per-turret targeting, the 24-node upgrade tree with the transformation lines, the two slice abilities (Airstrike, Overcharge), and 1×/2×/4× time controls. Done when two visibly different builds can clear Level 2.

**Spec:** `docs/GDD.md` §5 (turrets + transformation lines), §6 (enemy roster), §7 (24 nodes), §9 (Levels 2–3), §10 (abilities), §4.3 (time controls).

**Architecture (unchanged):** `sim/` stays deterministic, branch-on-type for enemies, virtual-dispatch only for targeting. One important, deliberate decision below: Cannon is **hitscan-AoE**, not a traveling projectile.

---

## Deviations from spec, recorded

1. **Cannon is hitscan-AoE, not a projectile.** GDD §14.5 calls for real projectile entities for Cannon shells once travel time matters. At the slice's turret counts and ranges, travel time is imperceptible, and a full `ProjectilePool` is a M5 concern (pooling + pooling handles are already on the roadmap). M4 Cannon resolves its AoE instantly at the target/tracer, deferring projectile entities to M5/V1. This is recorded so the change to real shells later is a swap, not a surprise.
2. **Abilities live in `Session`, not `sim/`.** Airstrike/Overcharge are player-input events whose effects mutate `World`; they carry no per-tick hot path and are not unit-tested here (the same reasoning as M3's `Session`). Cooldowns are plain accumulators; determinism is unaffected because a replay records inputs and re-applies the same mutations.
3. **`Lingering Flames` is modelled as a burn-duration multiplier, not independent ground-fire entities.** Ground-fire terrain is real content that deserves its own entity category; collapsing it into "burn lasts longer + stronger" delivers the *feeling* (area denial that outlasts the cone) at a fraction of the surface area, and the real thing can replace it in V1.
4. **Dear ImGui stays deferred** (fourth milestone running): rlImGui still has no tagged release. The raylib `DrawText` tree/report screens carry M4.
5. **Levels 2 & 3 reuse the Level 1 map geometry.** The GDD §9.4 calls for distinct flow problems (two lanes for L2, open field into a hard chokepoint for L3). Shipping an extra map geometry per level is a M5 task; M4's "two builds clear Level 2" is about turret builds, which the shared map still exercises. What *does* differ is the invasion: L2 mixes in Runners and L3 mixes in Tanks, so the density/pressure problems change even on the same terrain.

Everything else maps to Tasks 1–9.

---

## Global Constraints (carried forward)

- C++20, `-Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion` on first-party only.
- `sim/, math/, core/, ai/, gameplay/, persist/` never include raylib / `render/` / `ui/`.
- Fixed 60 Hz, deterministic (seeded `Pcg32`, no wall-clock in `sim/`/`gameplay/`).
- No heap allocation inside a tick; enemy pool capped at 100'000.
- Enemy behaviour is a `type` byte driving a branch — never virtual. Turret targeting is virtual (a few hundred calls/sec).

---

## File structure (additions / changes)

```
src/
├── sim/
│   ├── EnemyPool.h/.cpp        CHANGE  + speed/burn arrays, typed spawn
│   ├── EnemyType.h             NEW     type enum + per-type stats
│   ├── MovementSystem.h/.cpp   CHANGE  per-enemy speed
│   ├── Turret.h                CHANGE  kind + combat fields (splash, knockback, burn…)
│   ├── Targeting.h/.cpp        CHANGE  + DensestStrategy, splash parameter
│   ├── CombatSystem.h/.cpp     CHANGE  branch on turret kind (AoE / cone / burn)
│   └── World.h/.cpp            CHANGE  typed spawning, ability hooks
├── gameplay/
│   ├── Level.h/.cpp            CHANGE  typed spawn events; Levels 1–3
│   ├── SpawnDirector.h/.cpp    CHANGE  spawn with type
│   ├── UpgradeTree.h/.cpp      CHANGE  24 nodes, Effects struct, unlocks
│   └── Progression.*           (unchanged)
├── app/
│   ├── Session.h/.cpp          CHANGE  apply effects/targeting, abilities, time scale
│   └── Bench.cpp               CHANGE  level-based bench (unchanged behaviour)
├── render/Renderer.h/.cpp      CHANGE  draw kinds, burn tint, ability FX
└── main.cpp                    CHANGE  targeting/time/ability keys
tests/
├── test_enemytype.cpp          NEW
├── test_enemypool.cpp          CHANGE  typed spawn
├── test_movement.cpp           CHANGE  per-enemy speed
├── test_targeting.cpp          CHANGE  DENSEST
├── test_combat.cpp             CHANGE  AoE / burn / knockback
├── test_level.cpp              CHANGE  L2/L3 shape
├── test_upgradetree.cpp        CHANGE  24 nodes + effects
└── (others unchanged)
```

---

## Task 1: Enemy types — Grunt / Runner / Tank

**Files:** `src/sim/EnemyType.h`, `src/sim/EnemyPool.h/.cpp`, `src/sim/MovementSystem.h/.cpp`, `tests/test_enemypool.cpp`, `tests/test_movement.cpp`.

- `enum class EnemyType : uint8_t { Grunt, Runner, Tank };` with `struct EnemyStats { float hp; float speed; };` and `const EnemyStats& statsFor(EnemyType)`.

    | Type | HP | speed (wu/s) |
    |---|---|---|
    | Grunt | 100 | 40.0 |
    | Runner | 40 | 100.0 |
    | Tank | 2000 | 12.0 |

- `EnemyPool` gains `std::vector<float> speed`, `burnDps`, `burnTtl` (all sized `kCapacity`, zeroed), and `spawn` becomes `spawn(Vec2 pos, EnemyType type)` — HP/speed looked up from `statsFor`. The burner arrays are the burn model: `burnDps` is summed stacks, `burnTtl` is seconds remaining.
- `MovementSystem` uses `pool.speed[i]` instead of the global `MovementParams.speed` (the param is removed from the hot path; separation is unchanged).

**Tests:** each type spawns with its HP and speed; burn arrays initialise to 0; movement distance differs between a Grunt and a Runner over the same ticks.

---

## Task 2: Cannon — heavy AoE + knockback

**Files:** `src/sim/Turret.h`, `src/sim/CombatSystem.h/.cpp`, `tests/test_combat.cpp`.

- `Turret` gains `TurretKind kind`, `float splashRadius`, `float knockback`, `float projectileSpeed`(unused for now), and targeting default.
- `TurretKind { MachineGun, Cannon, Flamethrower }`. Machine Gun keeps its exact M3 behaviour.
- Cannon: damage 90, interval 2.0 s, range 260, splash 45, knockback 120. On fire it damages **all** enemies within `splashRadius` of the target with full damage **ramped by distance** (`falloff = 1 - dist/splash`), then knockback pushes them along the direction away from impact. Knockback is a direct, deterministic position impulse (not a velocity), applied before movement next tick.

**Tests:** Cannon damages several clustered enemies in one shot; damage falls off with distance; standalone enemies take full damage at the centre; knockback moves enemies away from the impact point; a far enemy outside splash is untouched.

---

## Task 3: Flamethrower — cone + Burn

**Files:** `src/sim/Turret.h`, `src/sim/CombatSystem.h/.cpp`, `tests/test_combat.cpp`.

- Flamethrower: cone of range 120 and half-angle 35°, fire interval 0.4 s; on fire it applies Burn to every enemy whose position is within range **and** within the cone angle of the turret's facing (facing = flow direction at the turret cell, or last-target direction).
- Burn: each application adds `burnDps += 6` and sets `burnTtl = 3.0`. In `World::tick`, a `applyBurn(EnemyPool&, float dt)` pass damages each burning enemy by `burnDps * dt` and decrements `burnTtl`, clearing `burnDps` on expiry. Burn damage is not attributed to a shot's `kills` (it is, but `totalKills` still comes from `cullDead`, so attribution is fine).

**Tests:** a single flamethrower application leaves the enemy's health dropping over subsequent ticks without further fire; burn expires exactly after its TTL; overlapping applications extend and stack the dps; an enemy outside the cone is not burned.

---

## Task 4: `DENSEST` targeting and per-turret assignment

**Files:** `src/sim/Targeting.h/.cpp`, `tests/test_targeting.cpp`.

- `select(...)` gains a `float splashRadius` parameter.
- `DensestStrategy`: among in-range enemies, pick the one whose position maximizes the count of *other* enemies within `splashRadius` (ties → lowest index for determinism). It needs a second hash query per candidate, which is fine at turret counts.
- Add `TargetingMode::Densest` to the enum and the factory. This is the default for Cannon and Flamethrower.

**Tests:** DENSEST picks the enemy at the densest cluster, not the nearest/strongest; with no splash (radius 0) it degrades to first-in-range; dead enemies are skipped.

---

## Task 5: Levels 2 & 3 — typed spawn curves

**Files:** `src/gameplay/Level.h/.cpp`, `src/gameplay/SpawnDirector.h/.cpp`, `tests/test_level.cpp`, `tests/test_spawndirector.cpp`.

- `SpawnEvent` gains `EnemyType type`. `makeLevel1()` marks all events `Grunt`.
- `makeLevel2()` "Refinery Gate": 250 enemies, Grunt + Runner mix, power 25, two-lane map (a second chokepoint/split — reuse the M1 map geometry with an extra wall block to create a split). `makeLevel3()` "The Narrows": 600 enemies, Tanks mixed in, power 60, open field into a hard chokepoint.
- `SpawnDirector` calls `world.spawnWay(count, type)` (renamed `spawnWave`); `World::spawnWave` becomes `spawnWave(uint32_t, EnemyType)`.

**Tests:** level totals; L2 contains Runner events and L3 contains Tank events; spawning typed events yields enemies with matching `type`/speed/HP.

---

## Task 6: The 24-node tree and transformation lines

**Files:** `src/gameplay/UpgradeTree.h/.cpp`, `tests/test_upgradetree.cpp`.

Replace the 6-node tree with 24 nodes. Repeatable stat nodes keep the `cost(n)=base·1.35ⁿ` curve; one-shot unlocks and transformations cost once (`maxLevel 1`, same curve at n=0).

| Node | Branch | Repeats | Effect |
|---|---|---|---|
| Damage | FIREPOWER | yes | ×1.20 dmg |
| FireRate | FIREPOWER | yes | ×1.15 rate |
| Range | COVERAGE | yes | ×1.12 range |
| BaseHp | DEFENSE | yes | +300 HP |
| BaseRegen | DEFENSE | yes | +2 HP/s |
| Economy | ECONOMY | yes | ×1.20 kill scrap |
| Splash | FIREPOWER | yes | ×1.15 AoE radius |
| Burn | FIREPOWER | yes | ×1.20 burn dps |
| UnlockCannon | TURRETS | no | allows Cannon placement |
| UnlockFlamethrower | TURRETS | no | allows Flamethrower placement |
| ExtraHardpoint | COVERAGE | no | +1 hardpoint slot |
| TargetingDensest | COVERAGE | no | unlocks DENSEST |
| MGOverclock | TURRETS | no | MG ×2 rate, ×0.7 dmg |
| MGRicochet | TURRETS | no | MG shots bounce (+1 @ 50%) |
| MGBulletStorm | TURRETS | no | 20th shot → 5-bullet spread |
| CannonExplosive | TURRETS | no | Cannon splash ×1.5 |
| CannonKnockback | TURRETS | no | Cannon knockback ×2.5 |
| CannonCluster | TURRETS | no | Cannon +3 sub-blasts @ 25% |
| FlameIgnite | TURRETS | no | burning enemies spread Burn |
| FlameLingering | TURRETS | no | Burn ttl ×2 |
| FlameFirestorm | TURRETS | no | Burn dps ×2 |
| AbilityAirstrike | ABILITIES | no | unlocks Airstrike |
| AbilityOvercharge | ABILITIES | no | unlocks Overcharge |
| ArmorPiercing | FIREPOWER | no | ×1.5 dmg vs Tank |

`UpgradeTree` exposes `Effects bonuses() const` returning the folded `Effects` (scalars + unlock booleans), and `bool has(NodeId)` for one-shot nodes. `cost`/`purchase`/`respec`/`loadLevels` unchanged in shape. `kNodeCount` becomes 24.

This is the milestone where the GDD §7.3 rule ("one node in three must change behaviour") becomes testable: ~13 of 24 nodes are behaviour/unlock nodes.

**Tests:** costs follow the curve for both repeatable and one-shot nodes; a one-shot node can be purchased exactly once (second purchase fails); `Effects` composes levels; `has()` reflects purchases; unlock nodes do not stack.

---

## Task 7: Time controls — 1× / 2× / 4×

**Files:** `src/app/Session.h/.cpp`, `src/main.cpp`.

- A `float timeScale()` on `Session` (or main-owned), cycled 1→2→4→1 on one key (GDD §4.3 — "bound to a single key").
- Implemented as: multi-tick per frame (`advance(frameSeconds * scale)`), keeping `dt` fixed and fully deterministic. 4× is the skip-the-trivial-opening tool.

**Tests:** none (app-level; documented).

---

## Task 8: Abilities — Airstrike and Overcharge

**Files:** `src/app/Session.h/.cpp`, `src/main.cpp`, `src/render/Renderer.*`.

- **Overcharge** (15 s CD, unlocked node): double the target turret's fire rate for 4 s, then overheat it for 8 s (no firing). Implemented as turret fields `overchargeTtl`/`overheatTtl` consumed in `updateCombat`; target = turret under the cursor. Overcharge remains a real, persisted combat modifier so it still functions correctly during deterministic replay.
- **Airstrike** (25 s CD, unlocked node): heavy damage along a line across the field (key → strike at the densest row, or click-targeted). Applies `kAirstrikeDamage` to every enemy whose cell is on the line, plus a loud tracer. Unlocked-by-node, so the milestone's "two builds" test can gate it.

The cooldown-only logic (`canUse`/`elapsed`) lives as a small, testable helper; the world mutation stays in `Session`.

---

## Task 9: Renderer, wiring, docs, close

- `Renderer`: distinct visuals per turret kind (barrel/burner shapes), burn-tinted enemies (orange), splash indicators for AoE, airstrike strip.
- `main.cpp`: targeting key cycles FIRST→CLOSEST→STRONGEST→DENSEST for the selected turret; time-scale key; ability keys.
- `Session` applies `Effects` to turrets/base at battle start and on placement (kind-scoped transforms: Overclock/Explosive/Knockback/etc. fold into the placed turret's fields).
- Bench unchanged (Level 1). New `docs/bench/m4.csv`.
- README status → M4; controls table updated.
- Changelog + push.

---

## Milestone 4 exit criteria

1. Clean build, zero first-party warnings.
2. Cannon and Flamethrower visibly kill differently (AoE shockwaves vs cone burn), and Runners/Tanks read as different threats.
3. Levels 2 and 3 are playable; two distinguishable builds (MG-focused vs AoE/burn-focused) feel different on Level 2.
4. `DENSEST` and per-turret targeting modes work.
5. The tree has 24 nodes, ~13 behaviour/unlock; costs curve correctly; respec is free and exact.
6. Time controls and the two abilities function; Airstrike/Overcharge are gated by their nodes.
7. `ctest` (unit + layering) green; `docs/bench/m4.csv` committed; determinism holds.

**Explicitly out:** Tesla/Missile, Swarm/Shield/Elite, Levels 4–8, real Cannon projectiles, ground-fire entities, endless mode, Dear ImGui.
