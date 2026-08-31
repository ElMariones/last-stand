# M7 — a branching campaign, four new enemies, and an economy that stops paying

## The complaints

1. Eight sectors in a straight line is a corridor, not a campaign. There is no
   choice in it and no reason to replay a tier.
2. Three enemy types means one answer: more damage. Nothing in the game asks a
   different question of a build.
3. It is easy to get rich, and the last sectors are not a challenge. Both are
   the same bug: payout is linear in enemy count and player DPS is
   super-linear in Scrap, so by sector 6 the curve has already crossed.

## The shape of the fix

### A DAG, not a list

18 sectors on six difficulty tiers: 1 / 4 / 4 / 4 / 3 / 2. Every sector past
the first tier lists two parents and opens when EITHER is cleared, so the tiers
converge and diverge and there are many routes through. `levelTier` and
`levelParent` are data; `Session::isLevelUnlocked` reads them; the sector map
lays itself out from them and draws the actual edges.

### Four enemies that each break a different lazy build

| kind | the lazy build it punishes |
|---|---|
| **Swarmer** | single-target DPS — 18 hp, 130 u/s, packs tight instead of spreading |
| **Brute** | flat damage per shot — 7 points of armour subtracted from every hit |
| **Phantom** | flamethrowers — 85% burn resist, and it weaves so aim slips off it |
| **Behemoth** | not enough DPS — regenerates 55 hp/s, so a slow grind never kills it |

Armour and burn resist mean the answer stops being "more of what you already
have". Weave and crowding are movement, so they also change what the horde
LOOKS like, which is the point of a new enemy at all.

### The economy

Three levers, all measured rather than guessed:

- **kill value decays with sector size.** Payout was linear in enemy count;
  now sector 1 pays 4.0 a kill and sector 18 pays 0.45, so an 80x bigger
  invasion pays about 9x more, not 80x.
- **steeper node costs, shallower node effects.** Growth 1.35 -> 1.42;
  Damage 1.20 -> 1.14 per level, Fire Rate 1.15 -> 1.11.
- **turrets get expensive fast.** Price growth 1.35 -> 1.55: a tenth gun
  should be a campaign decision, not an afternoon's Scrap.

## How it gets verified

`--balance N` plays the whole DAG headlessly, taking the first unlocked
uncleared sector after each clear. New: `--matrix` plays EVERY sector at four
fixed Scrap budgets and prints a clear/loss grid, which is the only honest way
to answer "is the last sector a challenge" — it says what you have to bring.

---

## What actually happened

### The bestiary and the graph landed as designed

18 sectors, 6 tiers, 10 new maps, 4 new enemy kinds. Every claim in the plan
above is now an assertion in `tests/test_level.cpp` or `tests/test_balance.cpp`
rather than a paragraph: the graph is acyclic and every sector is reachable,
each enemy kind debuts alone, kill value falls tier over tier while toughness
rises, every sector is clearable at some budget, and none of the last two
tiers' sectors is clearable on pocket change.

### The economy fix worked, and the evidence is the `scrap` column

The point was never the payout numbers, it was whether the player is ever
comfortable. Across a twenty-run campaign the auto-player ends runs holding
1, 29, 33, 23, 38, 27, 9, 3, 48, 44, 22, 32, 26, 3, 14, 59, 69, 197, 146, 313
Scrap. It is broke the entire way through, which is the condition under which
an upgrade is a decision rather than a formality.

### Three bugs the work exposed

**The matrix silently ignored its own budget.** Every column came back
identical. `Session::buy` requires `Phase::Tree`, and the harness was handing a
player Scrap while sitting in Prepare — so it measured six copies of the same
zero-investment run and printed them as a difficulty curve. A tool that reports
plausible numbers while measuring nothing is worse than one that crashes.

**The auto-player was capped at 64 purchases.** With that fixed, the top two
budgets stopped producing identical rows. The plateau at the bottom-right of
the grid had been the harness, not the game.

**Backface culling was eating the new art.** Half the detail on the four new
enemies was invisible because the point lists were wound the way they were
sketched. This is the second time it has cost an afternoon, so `emit()` now
normalises winding itself with one cross product per triangle — render-side, so
the simulation budget never sees it, and the class of bug is gone rather than
fixed.

### The measurement trap, which was the expensive lesson

A 35% "regression" appeared at 5,000 entities and did not reproduce anywhere.
It survived: reverting the movement changes, removing the extra RNG draw,
denormalising the stats lookup into per-entity arrays, a clean rebuild, and
grafting the entire M7 `src/` onto the previous commit's build directory —
where it ran at *base* speed.

The cause was not code. On macOS the binary the linker just wrote is scanned on
every exec; a byte-identical copy is not. `build/laststand` and
`build/laststand-copy` — same md5, same directory, same ad-hoc signature —
measured 0.93 ms and 0.66 ms.

Two things came out of it. The benchmark instructions now copy the binary
first, and the README says why. And one change was made on the strength of that
phantom number before it was understood — denormalising `crowding` and `weave`
into per-entity arrays — which was reverted once it measured as nothing: it
carried extra state and extra work in swap-remove to buy an improvement that
was never there.

The A/B against the previous commit, interleaved three times on copied
binaries: **0.652 ms base, 0.655 ms with four new enemy kinds.** Armour,
regeneration, weaving and crowding are free.
