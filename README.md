# LAST STAND

An incremental tower-defense game in C++20, built to run tens of thousands of
agents at 120 fps on a single core before reaching for threads.

> *You don't need to survive forever. You just need to get stronger faster
> than they do.*

**Status:** the slice is complete, and the campaign behind it is now a
branching graph: **eighteen sectors across six difficulty tiers**, **seven
enemy kinds**, and an economy whose curve is measured rather than asserted.

5,000 entities still simulate in well under a fifth of the 3.0 ms budget, down
from 14.7 ms at the naive baseline — because none of the presentation layer is
allowed to touch `sim/`.

## Build

Requires CMake 3.21+ and a C++20 compiler. All dependencies are fetched.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/laststand
```

## The campaign

Eighteen sectors on six tiers, 1 / 4 / 4 / 4 / 3 / 2. Every sector past the
first names **two parents** and opens as soon as *either* has been held, so the
tiers fan out and converge and there are many routes to the end. Requiring both
would have been a corridor with extra steps.

```
   I        II          III           IV            V           VI

                     ┌── Split ──┬── Spiral ──┬─ Gauntlet ─┬─ Open Ground
          ┌ Refinery ┤           │            │            │
          │          ├── Foundry ┼─ Crossroads┼─Meatgrinder ┤
          ├ Narrows ─┤           │            │            │
Outskirts ┤          ├─ Aqueduct ┼─ Catacombs ┼─ Causeway ──┴─ The Breach
          ├ Culvert ─┤           │            │
          │          └── Hollow ─┴── The Pit ─┘
          └ Scrapyard
```

Level select draws that graph rather than a list, and it draws it *from* the
graph — add a sector to `gameplay/Level.cpp` and it appears on the map, wired
to its parents, laid out by tier.

## The bestiary

Seven kinds. A kind that only has more HP than the last one is not a new enemy,
it is a multiplier with a name, so each of the four new ones invalidates a
different lazy build:

| kind | mechanic | what it punishes |
|---|---|---|
| **Swarmer** | packs tight instead of queueing | single-target DPS |
| **Brute** | 7 points of armour off every hit | fast weak guns; damage-per-second over damage-per-shot |
| **Phantom** | 85% burn resist, and it weaves | a wall of flamethrowers |
| **Behemoth** | regenerates 55 hp/s | patience — chip at it and it heals the chip off |

Armour is subtracted **per hit**, not as a percentage, which is what makes it a
different question rather than a bigger one: the same total DPS delivered as
one Cannon shell barely notices it. It is also the only thing that makes
**Armor Piercing** worth buying — the node *divides the armour* rather than
multiplying damage, so it is worth exactly as much as the armour it meets and
nothing at all against a Grunt.

## Screens

```
TITLE → MENU → SECTOR MAP → PREPARE → BATTLE → REPORT → UPGRADE TREE
          ↓                              ↓         ↑         │
       OPTIONS                         PAUSE       └─────────┘
```

The menu offers CONTINUE and NEW GAME; erasing a save takes two presses
rather than a dialog box. Options covers volumes, window size, fullscreen,
interface scale (75–150%), screenshake and the performance overlay, and every
change saves immediately.

Every arrow is reversible except `BATTLE → REPORT`. The title screen's
background is a live battle, dimmed — the simulation is fast enough to be a
menu backdrop, so it is one.

## Controls

Menus take the arrow keys, the mouse, `ENTER` and `ESC`; hovering with the
mouse moves the keyboard focus, so the two never disagree about what is
selected.

| Key | Action |
|---|---|
| click / drag | place a turret anywhere walkable, or pick one up and move it |
| right-click | recall a turret into the arsenal |
| `1` `2` `3` | pick Machine Gun / Cannon / Flamethrower |
| `F` / `C` | deploy everything in reserve / recall it all |
| `M` | cycle targeting mode (First → Closest → Strongest → Densest) |
| `L` | sector map (from Prepare) |
| `SPACE` / `ENTER` | deploy — allowed with turrets still in reserve |
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

## Tuning the economy

The progression curve is measured, not guessed. `--balance` plays the whole
campaign headlessly with an auto-player that follows the game's own Failure
Analysis advice when it can afford it, buys the cheapest node when it cannot,
and takes whichever sector the game itself suggests after each clear:

```bash
./build/laststand --balance 20
```

```
run  sector  result   kills        time   payout  x     bought  scrap
  1       1  loss        27/100      45s       86  0.75      3      1
  9       2  CLEAR      250/250      57s     1250  1.00      7     48
 12       6  CLEAR      700/700     153s     3553  1.00      4     32
 17      10  CLEAR     1300/1300    170s     6749  1.00     13     69
 20      17  CLEAR     3800/3800    155s    17538  1.00      4    313
```

Twenty-seven kills to three thousand eight hundred across twenty runs, with
retries on most sectors. Note the `scrap` column: the player ends nearly every
run **broke**. That is the fix for "it is too easy to get rich" — kill value
used to be a flat 4.0, which makes income *linear* in enemy count while the
tree makes damage *exponential* in Scrap. Those curves cross, and after they
cross the game funds itself. Kill value now falls from 4.00 in the first sector
to 0.85 in the last, so the final tier fields forty times the first tier's
invasion and pays about eight times as much for it.

### The difficulty matrix

A playthrough only ever tests one budget per sector — whatever the auto-player
happened to arrive with. That cannot answer *"is the last sector a challenge"*,
because the answer depends entirely on what you bring. So `--matrix` drops a
**fresh** player onto every sector at a spread of fixed budgets:

```bash
./build/laststand --matrix
```

```
sector                     tier      400     1500     5000    15000    40000    80000
 1 The Outskirts              1    CLEAR    CLEAR    CLEAR    CLEAR    CLEAR    CLEAR
 5 Scrapyard                  2      64%    CLEAR    CLEAR    CLEAR    CLEAR    CLEAR
 9 The Hollow                 3       5%      12%      80%    CLEAR    CLEAR    CLEAR
13 The Pit                    4       1%       3%      18%    CLEAR    CLEAR    CLEAR
16 Causeway                   5       0%       1%      12%    CLEAR    CLEAR    CLEAR
18 The Breach                 6       0%       1%       3%      42%      97%    CLEAR
```

Read down a column for the difficulty curve and across a row for what a sector
costs. The first sector falls to 400 Scrap; the last needs 80,000 — and at
40,000 it gets to 97%, which is the number the finale should print.

`tests/test_balance.cpp` turns both instruments into assertions: the first loss
must buy something, no more than two identical runs may occur back to back, a
run that spent Scrap may never come back smaller, the player must stay broke
while sectors remain, every sector must be clearable at *some* budget, and no
sector may be clearable on pocket change in the last two tiers.

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
cp build/laststand /tmp/measure && /tmp/measure --sweep docs/bench/m5.csv --stage stage3
```

**Copy the binary before you measure it.** That is not a superstition. On macOS
the executable the linker just wrote gets scanned on every exec, and a
byte-identical copy of the same file does not — same directory, same ad-hoc
signature, same md5, and `build/laststand` measured 0.93 ms against
`build/laststand-copy` at 0.66 ms on the same workload. Measuring the freshly
linked file inflates the result by up to 40% and the inflation varies run to
run, which is worse than a constant offset because it looks like a signal. This
cost an afternoon of chasing a 35% "regression" in code that turned out to be
identical: the way it was finally caught was A/B-ing two binaries with the same
md5, which is the check to reach for whenever a number moves and the disassembly
should not have.

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

Those rows were recorded on an idle machine over a single session; absolute
numbers drift with thermal state, so a claim about a *change* is only ever made
by re-measuring both sides back to back. The four new enemy kinds — armour,
regeneration, weaving and crowding — were checked that way against the previous
commit and cost nothing measurable: 0.652 ms against 0.655 ms at 5,000
entities, interleaved three times ([`docs/bench/m7.csv`](docs/bench/m7.csv)).

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

Screens can be captured reproducibly, which is how the UI gets reviewed
without pressing keys fast enough:

```bash
./build/laststand --shot 12 --shot-screen tree --shot-out tree.png
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
