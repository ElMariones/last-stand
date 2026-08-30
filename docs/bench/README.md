# Benchmarks

One CSV per milestone, each row a rung on the same entity ladder so the files
plot against each other. Every row is machine output from

```bash
./build/laststand --sweep docs/bench/mN.csv --stage <label> --notes "<what changed>"
```

## Method

- **Machine:** MacBook Pro M5, 24 GB. Release build, `-O3`, single core.
- **Ladder:** 100 / 500 / 1,000 / 2,000 / 5,000 entities.
- **Duration:** 1,200 ticks per rung (20 seconds of simulated time).
- **Scenario:** the Level 1 map, all four hardpoints manned, the whole
  population released at t=0. The benchmark measures steady-state per-tick cost
  at a given density, not the shape of an authored spawn curve.
- **The base is indestructible.** `World::tick` short-circuits once a battle is
  over, so a base falling mid-run turns the remaining samples into no-ops and
  deflates the mean. This was fixed in M5; rows from earlier milestones may
  include a truncated tail at 5,000.
- **Rendering is not involved.** No window is opened. `--bench` additionally
  reports the LOD census — how many enemies land in each detail tier and how
  many triangles that submits — which is a pure function of the pool and the
  spatial hash, so it needs no GPU.

## Reading m5.csv

`stage0` is the naive `O(n²)` separation loop that M1 shipped deliberately, and
it is still reachable at any time with `--naive-separation`. `stage1` is the
spatial-hash neighbourhood query; `stage2` the cell-pair walk that computes each
interaction once. `stage3` is unchanged simulation — LOD and batching are
render-side — and is recorded to say so.

The 5,000-entity rung is the one the slice targets: 14.717 ms → 0.568 ms, inside
a 3.0 ms budget with room for the V1 counts to eat.
