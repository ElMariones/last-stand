# LAST STAND

An incremental tower-defense game in C++20, built to run tens of thousands of
agents at 120 fps on a single core before reaching for threads.

> *You don't need to survive forever. You just need to get stronger faster
> than they do.*

**Status:** Milestone 6 of 6 — the slice is complete. Title screen, menus and
options; procedural audio that aggregates into a roar; hitstop, screenshake,
particles, corpses and Scrap arcs; and a Battle Report that tells you where you
were breached, by how much you missed, and which two nodes fix it.

5,000 entities still simulate in **0.53 ms** per tick — 18% of the 3.0 ms
budget, down from 14.7 ms at the naive baseline — because none of the above is
allowed to touch `sim/`.

## Build

Requires CMake 3.21+ and a C++20 compiler. All dependencies are fetched.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/laststand
```

## Screens

```
TITLE → MENU → LEVEL SELECT → PREPARE → BATTLE → REPORT → UPGRADE TREE
          ↓                                 ↓         ↑         │
       OPTIONS                            PAUSE       └─────────┘
```

Every arrow is reversible except `BATTLE → REPORT`. The title screen's
background is a live battle, dimmed — the simulation is fast enough to be a
menu backdrop, so it is one.

## Controls

Menus take the arrow keys, the mouse, `ENTER` and `ESC`; hovering with the
mouse moves the keyboard focus, so the two never disagree about what is
selected.

| Key | Action |
|---|---|
| click | place the selected turret kind on a hardpoint (Prepare) |
| `TAB` | cycle turret kind: Machine Gun → Cannon → Flamethrower |
| `M` | cycle targeting mode (First → Closest → Strongest → Densest) |
| `L` | level select (from Prepare) |
| `SPACE` / `ENTER` | start the battle |
| `S` | cycle time scale 1× → 2× → 4× |
| `A` | airstrike the densest lane (if unlocked) |
| `O` | overcharge the turret under the cursor (if unlocked) |
| `ESC` | pause (battle) · back (menus) |
| `R` | retry — restart the level with the same loadout |
| `U` | open the upgrade tree (from the report) |
| `X` | respec all (tree) |
| `F` / `G` / `T` | debug: flow-field / grid / range rings |

Scrap, the 24-node tree, per-level bests and your options persist to
`laststand.save`. The core loop is battle → report → upgrade → retry, and the
retry path is one keypress from either the report or the tree — it is the most
optimised path in the game, and it is measured every milestone.

## Sound

There are no audio files in this repository. All fourteen voices are
synthesised at launch from a spec — an oscillator glide, a noise mix, a
one-pole lowpass, an envelope and a seed — which keeps the one-command build
honest and the mix tunable by constant rather than by re-export.

The hard problem at four thousand kills a minute is that one death sound per
enemy is unlistenable. Above a smoothed rate threshold, deaths and gunfire stop
playing individually and hand over to two continuous beds whose gain tracks the
rate, with the 32-voice cap spent on what you must not miss: the base being
hit, an ability landing, the UI answering you. The audio ends up reporting
progression on its own — the early game is individual pops, the late game is a
roar.

## Benchmark

The simulation runs headlessly, with no window and no renderer:

```bash
./build/laststand --bench --ticks 10000 --spawn 5000
```

One command writes the whole entity-count curve to a CSV:

```bash
./build/laststand --sweep docs/bench/m5.csv --stage stage3
```

### The optimisation curve

Mean tick time, MacBook Pro M5, Release, 1,200 ticks per rung. Every row is a
committed `--sweep` run in [`docs/bench/`](docs/bench/), not a recollection.

| entities | stage 0 — naive | stage 1 — spatial hash | stage 2 — cell pairs | total |
|---:|---:|---:|---:|---:|
| 100 | 0.020 ms | 0.013 ms | 0.010 ms | 2.0× |
| 500 | 0.165 ms | 0.076 ms | 0.033 ms | 5.0× |
| 1,000 | 0.597 ms | 0.183 ms | 0.075 ms | 7.9× |
| 2,000 | 2.378 ms | 0.420 ms | 0.178 ms | 13.3× |
| 5,000 | **14.717 ms** | 1.245 ms | **0.568 ms** | **25.9×** |

**Stage 1** replaced the `O(n²)` pairwise separation loop with a spatial-hash
neighbourhood query. **Stage 2** stopped computing every interaction twice:
separation force is antisymmetric, so the pass walks *cell pairs* rather than
entities and applies each interaction to both ends.

The naive loop is still there, behind `--naive-separation`, so the baseline is
reproducible rather than a claim in an old commit message. And one stage did not
pay: packing positions into the hash's cell order changed nothing measurable at
these counts (1.2321 → 1.2280 ms), because a 40 KB position array already lives
in L2. It is documented in `docs/superpowers/plans/` alongside the ones that
worked.

### Level of detail

Detail degrades by **local density**, not global count (GDD 12.2), so a lone
scout keeps its outline, head, weapon arm and walk cycle while the chokepoint
collapses to one shape per enemy. `--bench` reports the submission cost without
needing a window:

```
lod_tiers      full 131  silhouette 1303  shape 3222
lod_triangles  8048   (all-full 32592, all-shape 4656)
```

Wall-clock render timing needs a desktop session:

```bash
./build/laststand --render-bench 5000 --no-lod --no-batch   # the M4 path
./build/laststand --render-bench 5000                       # LOD + batching
```

## Tests

```bash
ctest --test-dir build --output-on-failure
```

Two of them are worth pointing at:

- **The golden-hash determinism test.** Three fixed scenarios run a fixed number
  of ticks from a fixed seed and are checked against hashes committed in the
  test. Unlike comparing the simulation against itself, this also catches a
  change that is deterministic but *wrong*. It passes identically in Debug and
  Release, which is why the build sets `-ffp-contract=off`: left on, the
  compiler fuses a multiply-add at `-O2` and not at `-O0`, and the same source
  produces different floats.
- **The zero-allocation assertion.** A replaced global `operator new` counts
  allocations while armed; three battles — plain, burning-and-exploding, and
  Session-driven with abilities firing — allocate exactly zero times across
  thousands of ticks.

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
- **Optimisation is staged and measured, not assumed.** Milestone 1 shipped a
  deliberately naive O(n²) separation loop so that Milestone 5 had a real
  baseline to improve on, and the 25.9× above is the measured delta between
  them. The sequence is the point: a project that was fast from the first commit
  demonstrates nothing.
- **Draw calls are a function of tier count, not entity count.** Enemies are
  bucketed by LOD tier and each bucket is submitted in one batch.
- **Juice cannot reach the simulation.** Hitstop withholds whole ticks from the
  fixed timestep rather than scaling `dt`; screenshake is a `Camera2D` offset;
  particles, corpses and damage numbers live in `fx/`, which the layering test
  forbids from including raylib. The evidence that Milestone 6 stayed cosmetic
  is that the three golden hashes from Milestone 5 still pass untouched.

Design document: [`docs/GDD.md`](docs/GDD.md).
