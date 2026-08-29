# LAST STAND

An incremental tower-defense game in C++20, built to run tens of thousands of
agents at 120 fps on a single core before reaching for threads.

> *You don't need to survive forever. You just need to get stronger faster
> than they do.*

**Status:** Milestone 3 of 6 — the loop (spawn curves, Scrap economy, upgrade tree, save, RETRY).

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
| `T` | toggle turret range rings |
| `SPACE` / `ENTER` | start the battle (from Prepare) |
| `R` | retry — restart the level immediately |
| `U` | open the upgrade tree (from the report) |
| `1`–`6` | buy a node (tree) |
| `X` | respec all (tree) |

The game state (Scrap, tree, level bests) persists to `laststand.save`. The
core loop is: battle → report → upgrade → retry, with the retry path a single
keypress from either the report or the tree.

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
