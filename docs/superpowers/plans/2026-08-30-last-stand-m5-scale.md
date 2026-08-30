# LAST STAND — Milestone 5: Scale — Implementation Plan

**Goal:** Apply GDD §15's performance stages 1–3 at slice counts, each as its own
commit with before/after numbers; add the density-driven LOD system; hold 5,000
entities inside the 120 fps budget; and land the determinism regression test.
Done when the benchmark CSV shows the curve and the golden-hash test passes.

**Spec:** `docs/GDD.md` §14.5 (the systems that make 50,000 possible), §14.7
(performance targets and the frame budget), §15 (the staged optimisation
sequence), §12.2 (density-driven LOD), §14.9 (determinism regression test,
layering test).

---

## The measurement rule

Nothing in this milestone ships on an argument. Every stage is:

1. **Profile first.** Find where the time actually is, then optimise that.
2. **Prove behaviour is unchanged.** An optimisation that changes results is a
   redesign wearing a disguise, and it needs to be argued as one.
3. **Commit the numbers**, in the message and in `docs/bench/m5.csv`.
4. **Report the stages that did not pay**, in the same detail as the ones that
   did. A milestone that only records its wins is a sales pitch.

The Stage 0 loop is kept behind `--naive-separation`, not deleted, so the
baseline stays reproducible on any machine rather than being a number in an old
commit message.

---

## Task 0: the review pass (landed first, separately)

Six defects found reading M1–M4 before starting, each with a regression test:
DENSEST reading a clobbered candidate buffer; Levels 2–3 ignoring their authored
spawn curves; upgrades not reaching the turrets on RETRY; targeting choices lost
on retry; level seeds drifting with the clear count; Ignite flashing across the
whole crowd in one tick. See `fix: six review defects…`.

The first of those shaped the milestone: removing the shared query buffer left
`forEachInRadius` as the hash's only primitive, which is what Stage 1 needed.

---

## Task 1: measurement harness

**Files:** `src/app/Cli.*`, `src/app/Bench.*`, `src/sim/MovementSystem.*`,
`src/sim/World.*`.

- `--sweep FILE` runs the standard ladder (100/500/1,000/2,000/5,000 entities,
  1,200 ticks) and appends a row per rung, with `--stage` and `--notes` labels.
  The committed CSVs are exactly this output.
- `--naive-separation` selects the Stage 0 loop.
- The benchmark's base is indestructible. `World::tick` short-circuits once a
  battle is over, so a base falling mid-run turned the remaining samples into
  no-ops and quietly deflated the mean.

---

## Task 2: Stage 1 — spatial-hash separation

**Files:** `src/sim/MovementSystem.*`, `src/sim/SpatialHash.*`, `src/sim/World.*`,
`tests/test_movement.cpp`.

Separation walks only the enemies binned within the separation radius. The hash
is built twice per tick — before movement, over the positions separation is
about to read, and after it, over the positions combat needs.

The hash cell drops from 64 units to 12. 64 was tuned to turret range, which was
right while acquisition was the only client; separation runs n times per tick
rather than a few dozen.

**Tests:** the naive and hashed paths, run over 400 enemies for 10 ticks, must
agree on every position. Same visited set, different summation order, so the
comparison is to float tolerance rather than bit-for-bit.

**Result:** 14.717 ms → 1.245 ms at 5,000 entities (11.8×).

---

## Task 3: Stage 2 — data layout

**Files:** `src/sim/MovementSystem.*`, `src/sim/SpatialHash.h`, `src/sim/World.*`.

GDD §15 stage 2 is "AoS to SoA". The pool has been SoA since M1, so the honest
version of the stage is: instrument the tick, find what is left, go after that.
At 5,000 entities movement was 1.215 ms of a 1.246 ms tick — 96%.

- **Cell-pair walk.** Separation force is antisymmetric, so a per-entity query
  computes every interaction twice. Walk cell pairs instead — each cell against
  itself, then against four of its eight neighbours — and apply each interaction
  to both ends. Valid only while the radius fits in a cell; wider radii fall back
  to the per-entity query, and a test covers that boundary.
- **Cell-packed positions.** The build packs positions in cell order beside the
  indices. No measurable change at slice counts (1.2321 → 1.2280 ms): the
  position array is 40 KB and already in L2. Kept on correctness grounds — a
  query can no longer be handed a different array than the hash was built from —
  and recorded as a stage that did not pay.

Separation also became order-independent, reading neighbours from the
start-of-tick snapshot in `prevPosition`.

**Result:** 1.245 ms → 0.568 ms at 5,000 entities (2.2×; 26× on the baseline).

---

## Task 4: Stage 3 — LOD and batched rendering

**Files:** `src/render/Lod.h` (new), `src/render/Renderer.*`, `src/app/Bench.*`,
`src/main.cpp`, `tests/test_lod.cpp` (new).

- Three tiers per GDD §12.2, chosen by the occupancy of the enemy's own
  spatial-hash cell — local density, not global count.
- One `rlBegin(RL_TRIANGLES)` span per tier. raylib's `DrawTriangle` brackets
  every call with `rlSetTexture`, so the horde paid submission overhead
  proportional to entity count.
- View culling before bucketing.
- `lodCensus()` is a pure function of the pool and the hash, so `--bench`
  reports submission cost headlessly and reproducibly.
- `--render-bench N [--no-lod] [--no-batch]` times `renderer.draw()` with vsync
  off, each technique switchable.

**Deviation, recorded:** the wall-clock render numbers were not measured. The
shell this milestone was built in has no display, and GLFW crashes inside
`InitWindow` rather than returning, so `--render-bench` is shipped untimed. The
headless census is the committed artifact; the timing is one command away on a
desktop session.

**Result (headless, 5,000 entities, Level 1):** 8,048 triangles submitted,
against 32,592 to articulate everyone (4.05×) and 4,656 for the flat M4 path.
LOD does not make the M4 frame cheaper — M4 already drew the cheapest possible
thing. It makes the articulation affordable.

---

## Task 5: determinism regression and the zero-alloc invariant

**Files:** `src/sim/World.cpp`, `CMakeLists.txt`, `tests/test_golden.cpp` (new),
`tests/test_noalloc.cpp` (new).

- Three golden scenarios with hashes committed in the test: horde-only, the full
  Level 1 battle, and Level 3 against a build that exercises every damage path.
- `World::stateHash` widened to cover live count, tick count, the running
  tallies, velocity, burn ttl, enemy type and per-turret ability state.
- `-ffp-contract=off` on first-party targets. Verified, not assumed: with
  contraction on, Release and Debug disagree on all three hashes; with it off
  they agree exactly.
- A replaced global `operator new` counts allocations while armed. Three battles
  — plain, burning-and-exploding, and Session-driven with abilities firing —
  allocate zero times across thousands of ticks. The first test in the file
  allocates deliberately and requires the counter to notice.

---

## Milestone 5 exit criteria

1. `docs/bench/m5.csv` shows the stage 0 → 3 curve across the entity ladder. ✅
2. 5,000 entities simulate inside the 3.0 ms budget — 0.557 ms, 19% of it. ✅
3. The golden-hash test passes, and passes identically in Debug and Release. ✅
4. Zero allocations during a battle, asserted. ✅
5. LOD degrades by local density; draw calls are a function of tiers. ✅
6. `ctest` (unit + layering) green; zero first-party warnings. ✅
7. Wall-clock render timings. ❌ — no display in the build environment; the
   harness ships, the numbers do not.

**Explicitly out:** multithreading (GDD §15 stage 5, deliberately last and a V1
item), projectile pooling with generation handles, the 100,000-entity headless
run, Tracy integration, GitHub Actions.
