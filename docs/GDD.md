# LAST STAND — Game Design Document

**Version:** 0.1 (design)
**Date:** 2026-08-29
**Author:** ElMariones
**Status:** Design approved pending review → implementation plan

> *You don't need to survive forever. You just need to get stronger faster than they do.*

---

## Document conventions

Every system in this document is tagged with the scope it belongs to:

| Tag | Meaning |
|---|---|
| **[SLICE]** | In the 4–6 week vertical slice. Build this. |
| **[V1]** | In the 1.0 north star. Design for it, don't build it yet. |
| **[V2]** | Explicitly deferred. Do not design around it. |

The architecture must support **[V1]** without rewrites. The content must ship at **[SLICE]**.

**Working title note:** *LAST STAND* is descriptive but heavily used (several shipped games share it). It works as an internal codename. Before any public release, consider: **OVERRUN**, **HOLDFAST**, **ATTRITION**, **SECTOR ZERO**. Not a blocker; flagged so it isn't decided by default.

---

## 1. Game pitch

**LAST STAND** is an incremental tower-defense game about the gap between how weak you are and how strong you need to be — and about closing it one run at a time.

Each level is a fixed battlefield with a fixed, enormous invasion. You configure a defense, you watch it fight, and you almost certainly lose. Losing pays you. You spend the payout on a permanent upgrade tree, and you go back in slightly less doomed than before. Repeat until the screen is an unreadable wall of fire, lightning and shrapnel and you are killing four thousand enemies a minute.

The whole game is engineered around one sensation: **look at how much stronger I am than I was an hour ago.**

**Elevator pitch:** *A tower defense where you always lose the first time, and losing is the point. Start with one turret against twenty enemies. End with twelve turrets against fifty thousand.*

**Reference points:** *Sir, We Have an Orc Problem* (run→upgrade→retry loop, horde-as-spectacle), *Vampire Survivors* (escalating visual chaos as reward), *Nova Drift* (mechanical transformation over stat inflation), *Geometry Wars* (vector spectacle at density).

**Deliberate divergences from the reference:**
- **Setting:** sci-fi industrial apocalypse, not fantasy. Different silhouette in a crowded genre.
- **Buildcraft identity:** upgrades transform *behaviour*, not just numbers. The endgame goal is "I'm running a burn/conduct build," not "I have big numbers."
- **No in-battle economy.** Configure before, react during, upgrade after. This keeps the game's centre of gravity on meta-progression.

---

## 2. Core pillars

Every design decision is checked against these four. If a feature doesn't serve one, it gets cut.

### Pillar 1 — Start pathetic, end absurd
The power delta between the first run and the last must be legible and enormous. Not 3×. More like 10,000×. The player should be able to *see* their progress in the number of things dying on screen, without reading a stat sheet.

### Pillar 2 — Losing pays
A defeat is a transaction, never a punishment. Every run ends with a number going up and a clear reason to press RETRY. There is no failure state that costs the player anything but time, and the retry loop is measured in seconds.

### Pillar 3 — The horde is a fluid
Enemies are authored as density, not as a queue of individuals. Maps are flow problems: where does the horde compress, where does it thin, where does it leak. Every turret's value is a function of local density, so the tactical meta genuinely shifts as counts scale.

### Pillar 4 — Builds, not stat sheets
By mid-game the player's upgrades should have committed them to an identity. Two players at the same total Scrap spend should have visibly different battlefields.

---

## 3. Core gameplay loop

### The macro loop

```
   SELECT LEVEL  ──▶  CONFIGURE DEFENSE  ──▶  BATTLE  ──▶  BATTLE REPORT
        ▲                                                        │
        │                                                        ▼
        └──────────────  UPGRADE TREE  ◀─────────  SCRAP PAYOUT
```

Target cycle time: **3–6 minutes**, of which battle is 2–4 minutes and everything else is under 60 seconds.

**Hard requirement:** from the moment the base dies to the moment the next battle starts, if the player takes the fastest path (RETRY with no upgrade), the elapsed time must be **under 5 seconds**. Including a purchase, **under 15 seconds**. This is a genre requirement, not a polish target. If the retry loop is slow, the game does not work at all. It is tested every milestone.

### The micro loop (inside a battle)

```
  observe pressure  ─▶  spend ability  ─▶  watch payoff  ─▶  observe next pressure
```

### The psychological loop

```
   "847 kills? I can get 1,000."
              │
              ▼
   buy +15% fire rate
              │
              ▼
   1,062 kills — new best
              │
              ▼
   "...I can get 1,500."
```

The Battle Report exists to manufacture this thought. See §13.

---

## 4. Battle system

### 4.1 Structure of a battle

A battle is a **fixed, finite, deterministic invasion**. It is not endless and it is not procedurally generated. Level 4 always sends the same 1,500 enemies in the same order from the same seed. This matters for three reasons: the player can learn a level and beat it with knowledge as well as power; balancing is tractable; and bug reproduction is trivial (§14.6).

The battle ends when either:
- **Victory** — all spawned enemies are dead.
- **Defeat** — base HP reaches 0.

There is no timer. There is no wave-clear pause. Enemies flow continuously, with density varying over the level's spawn curve — troughs to breathe, peaks to survive.

### 4.2 The three phases

| Phase | Player does | Duration |
|---|---|---|
| **Prepare** | Place turrets into slots, assign targeting priority, review the level briefing | Untimed |
| **Battle** | Fire abilities, overcharge turrets, paint priority targets, control time | 2–4 min |
| **Report** | Read the diagnosis, take the payout, upgrade or retry | Untimed |

### 4.3 Design risk: agency during battle

**The risk.** Removing in-battle construction is correct for the incremental focus, but it strips out the thing that normally occupies the player's hands. A player watching a 3-minute battle with nothing to do will put the game down, and no amount of spectacle fixes boredom in minute two.

**The mitigations**, in priority order. The slice ships the first three; if playtesting shows the battle still feels passive, that is a signal to add more, not to ignore.

1. **[SLICE] Time controls — 1× / 2× / 4×.** The single highest-value feature here. It respects the player's time, it lets them skip the trivial opening, and it makes the chaotic peak feel *chosen*. An incremental game without a speed control is broken. Bound to a single key.
2. **[SLICE] Active abilities on cooldown.** Airstrike to start. These are the real moment-to-moment decisions: hold the cooldown for the peak, or spend it now to stop a leak. One ability at slice, up to four by V1.
3. **[SLICE] Overcharge.** Click a turret to double its fire rate for 4 seconds, followed by an 8-second overheat where it doesn't fire at all. A genuine risk/reward decision, once per turret, constantly. Cheap to implement, enormous agency value.
4. **[V1] Target painting.** Drag a priority zone; turrets in range weight it heavily. Lets the player answer "the shields are getting through on the left."
5. **[V1] Emergency abilities gated on base damage.** A one-shot panic button that only charges when you're losing, so comebacks feel earned.

**Verification:** at the end of Milestone 3, play ten consecutive runs. If the hands are idle for more than ~15 seconds at a stretch, escalate to mitigation 4.

### 4.4 Turret slots

The battlefield has a fixed number of **hardpoints** — marked positions where turrets can be placed. Hardpoints are authored per level, not a free-placement grid.

This is deliberate. Free placement means the optimal solution is a maze and the player's job becomes route-drawing, which is a different game and a much larger balancing problem. Fixed hardpoints keep the decision as *"which turret goes where, given what this level's flow looks like"* — a clean, readable choice that scales.

- Slice: 4 hardpoints, 2 unlocked at start.
- V1: 6–12 hardpoints per level, unlocked via the Coverage branch.
- Hardpoints have **positional character**: a forward hardpoint sees enemies early at low density; a rear hardpoint sees them late at high density. This is what makes Flamethrower-vs-Machine-Gun a real placement decision rather than a formality.

### 4.5 Base and defeat

The base has HP. Every enemy that reaches it deals damage equal to its remaining HP contribution (a Tank that survives hurts far more than a Grunt), then despawns. There is no "leak counter" abstraction — the damage is the enemy.

This means the failure mode is legible: *you didn't kill the tanks*, or *a thousand grunts got through at once*. The Battle Report reads that directly off the sim.

---

## 5. Turret system

### 5.1 Design principle

Every turret's damage output is a **function of local enemy density**. This is Pillar 3 made mechanical, and it's what makes the turret meta shift as levels scale rather than converging on one best turret.

```
 DPS
  │                                    ╱ Tesla
  │                              ╱────
  │                        ╱────      ╱ Flamethrower
  │                  ╱────      ╱────
  │            ╱────      ╱────
  │  ────────────────────────────────── Machine Gun
  │ ╱
  └──────────────────────────────────────▶ local density
```

Machine Gun is flat — reliable everywhere, best when enemies are sparse. Tesla is nearly worthless against three enemies and monstrous against three hundred. That curve *is* the progression: the turret that carried you through Level 1 is dead weight by Level 6, so the player is pushed to re-evaluate rather than stack.

### 5.2 The five turrets

| Turret | Role | Density curve | Slice |
|---|---|---|---|
| **Machine Gun** | Reliable single-target DPS | Flat | ✅ |
| **Cannon** | Slow, heavy, knockback | Slight positive | ✅ |
| **Flamethrower** | Cone area-denial, DoT | Strong positive | ✅ |
| **Tesla Coil** | Chain lightning | Extreme positive | [V1] |
| **Missile Launcher** | Long-range burst AoE | Strong positive, range-gated | [V1] |

**Machine Gun** — 5 dmg, 8/sec, medium range. The baseline everything is balanced against. Never bad, never exciting.

**Cannon** — 90 dmg, 0.5/sec, long range, small explosion, knocks targets back. The knockback is the interesting part: it compresses the horde, which feeds every AoE turret behind it. See §11.

**Flamethrower** — 12 dmg/sec in a short cone, applies **Burn** (a stacking DoT). Terrible against a lone Tank, devastating against a compressed stream. Burn is also the setup half of the game's best synergy.

**Tesla Coil** — 40 dmg to a target, chains to 3 nearby enemies at 60% falloff. At high density the chain finds targets forever; at low density it's a bad Machine Gun.

**Missile Launcher** — 200 dmg in a large radius, 0.25/sec, minimum range. Fires at the densest cluster in range rather than the nearest enemy, which is a small but very satisfying piece of targeting logic.

### 5.3 Mechanical transformations, not stat inflation

This is the buildcraft pillar. Each turret has an upgrade line where later nodes **change how it behaves**, and some are deliberately double-edged so they're a choice rather than a purchase.

**Machine Gun line**
```
  Base ─▶ Overclock ─▶ Bullet Storm ─▶ Ricochet
```
- **Overclock** — fire rate ×2, damage ×0.7. *Net DPS up 40%, but worse against armour.* A real tradeoff.
- **Bullet Storm** — every 20th shot fires 5 bullets in a spread.
- **Ricochet** — bullets bounce to one additional enemy at 50% damage. Scales with density, quietly converting the flat turret into a soft-positive one.

**Cannon line**
```
  Base ─▶ Explosive Shells ─▶ Knockback ─▶ Cluster Shot
```
- **Cluster Shot** — the shell splits into 4 smaller explosions on impact.

**Flamethrower line**
```
  Base ─▶ Lingering Flames ─▶ Ignite ─▶ Firestorm
```
- **Lingering Flames** — ground fire persists 3s where the cone was. Converts a turret into terrain.
- **Ignite** — burning enemies spread Burn to adjacent enemies. *This is exponential at density* and is the moment a player realises the game has a build layer.

**Tesla line** [V1]
```
  Base ─▶ Extra Chains ─▶ Conductive ─▶ Overload
```
- **Conductive** — chains prefer burning enemies and deal +100% to them. The payoff half of the signature synergy.
- **Overload** — chain damage no longer falls off with distance.

**Missile line** [V1]
```
  Base ─▶ Cluster Warhead ─▶ Homing ─▶ Carpet Bombing
```

### 5.4 Targeting

Each turret has a player-assignable targeting mode, set during Prepare:

`FIRST` · `LAST` · `CLOSEST` · `STRONGEST` · `WEAKEST` · `DENSEST`

`DENSEST` (pick the position maximising enemies within splash radius) is the interesting one and is the default for AoE turrets. It's a genuinely useful bit of spatial reasoning and it makes the Missile Launcher feel genuinely smart rather than merely loud.

Slice ships `FIRST`, `CLOSEST`, `STRONGEST`, `DENSEST`.

---

## 6. Enemy system

### 6.1 Roster

Six types. Each exists to punish a specific gap in the player's build — that's the entire design brief for an enemy.

| Enemy | HP | Speed | Punishes | Slice |
|---|---|---|---|---|
| **Grunt** | 100 | 1.0 | nothing — baseline mass | ✅ |
| **Runner** | 40 | 2.5 | low fire rate, rear-only coverage | ✅ |
| **Tank** | 2,000 | 0.3 | pure-AoE builds with no single-target DPS | ✅ |
| **Swarm** | 15 | 1.4 | single-target builds with no AoE | [V1] |
| **Shield** | 300 | 0.8 | naive targeting; grants nearby allies 50% DR | [V1] |
| **Elite** | 8,000 | 0.6 | everything; drops 20× Scrap | [V1] |

**Grunt** is the fluid. It exists in the thousands and is the substance the spectacle is made of.

**Runner** arrives before your defense has settled and forces the player to care about coverage depth rather than just total DPS.

**Tank** is the check on the AoE spiral. Without it, every player converges on Flamethrower + Tesla and the build layer collapses.

**Swarm** spawns in packs of 20 from a single spawn event. Cheap HP, huge counts — this is the entity-count pressure valve and the primary tool for making Level 6+ look insane.

**Shield** makes targeting mode a real decision. A `CLOSEST` turret will happily shoot the shielded wall forever; `STRONGEST` cuts the head off.

**Elite** is the risk/reward beat: killing one funds a whole upgrade, ignoring one usually ends the run.

### 6.2 Movement — the horde as fluid

Enemies do **not** path individually. Each map has a precomputed **flow field**: a grid where every cell stores a direction vector pointing down the cheapest route to the base, generated once at level load by a BFS/Dijkstra pass from the base outward.

Per enemy, per tick, movement is:

```
  desired = flowfield.sample(position)          // where the map says to go
          + separation(neighbours) * w_sep      // don't overlap my neighbours
          + avoidance(obstacles)  * w_avoid     // don't walk into walls
```

This is the single decision that makes 50,000 enemies possible. Per-enemy A* at that count is not a matter of optimisation, it is impossible. The flow field also produces exactly the emergent behaviour the design wants for free: enemies compress at chokepoints, stream along walls, and pile up behind obstacles, all without any authored crowd logic.

Separation is resolved against the spatial hash (§14.3), sampling a capped number of neighbours per enemy — the crowd reads as a fluid without paying an O(n²) bill.

### 6.3 Scaling

Enemy *count* is the primary difficulty knob. HP and speed multipliers are secondary and are used sparingly — inflating HP makes the game feel slower, whereas inflating count makes it feel bigger. Count is always the better lever for this game's fantasy.

---

## 7. Upgrade and meta-progression

### 7.1 Structure

A persistent, branching tree, purchased with **Scrap**, never reset (until [V2] prestige). Four branches from a common root:

```
                              ROOT
                                │
        ┌───────────┬───────────┼───────────┬───────────┐
        │           │           │           │           │
    FIREPOWER   COVERAGE    TURRETS     DEFENSE     ECONOMY
```

**FIREPOWER** — damage, fire rate, crit, armour penetration. Converges on **Overdrive**.
**COVERAGE** — range, projectile speed, extra hardpoints, targeting modes.
**TURRETS** — unlocks and the per-turret transformation lines from §5.3. The buildcraft branch.
**DEFENSE** — base HP, armour, regeneration, Emergency Shield, Last Stand.
**ECONOMY** — Scrap per kill, wave bonus, death bonus, elite bonus.

**ABILITIES** becomes a sixth branch at [V1] (Airstrike → Cluster Bomb → Napalm → Carpet Bombing).

### 7.2 Node costs

Within a branch, cost scales as:

```
  cost(n) = base * 1.35^n
```

Cross-branch nodes (the convergence points like Overdrive) cost a flat premium and require both parents. These are the build-defining purchases and should feel like events.

| Scope | Nodes |
|---|---|
| **[SLICE]** | 24 — enough to prove the loop and produce two distinguishable builds |
| **[V1]** | 80–100 |

### 7.3 The anti-inflation rule

**At least one node in three must change behaviour, not a number.** A tree of 100 percentage increments is a spreadsheet. This rule is what keeps Pillar 4 alive and it is checked whenever nodes are added.

### 7.4 Respec

Free, unlimited, from the tree screen. Refunds 100% of Scrap.

Charging for respec punishes exactly the experimentation the build pillar is trying to encourage, and there is no competitive integrity to protect in a single-player game. Making respec free costs nothing and removes the fear of a wrong purchase — which means players will actually try the weird build.

---

## 8. Economy

### 8.1 Currency

**Scrap** — one currency, permanent, earned from battles, spent on the tree. There is no in-battle currency and no second resource. One currency keeps every decision comparable, which is what makes the "should I buy this or save?" tension work.

### 8.2 Payout formula

```
  scrap = (kills × kill_value × economy_mult)
        + (depth_bonus × progress²)
        + (elite_kills × elite_value)
        + personal_best_bonus
```

Where `progress` is the fraction of the invasion destroyed. The **squared** term is doing important work: it makes getting 90% of the way through a hard level worth substantially more than clearing an easy one, which points the player forward instead of at the farm.

### 8.3 Losing pays

**A defeat pays 75% of what the same progress would have paid on a victory.**

This number is the beating heart of Pillar 2 and it is deliberately high. The 25% gap is enough to make victory feel like the goal; it is nowhere near enough to make defeat feel wasted. If playtesting shows players dreading a loss, this number goes up, not down.

### 8.4 Anti-grind: diminishing replays

Replaying a cleared level pays progressively less:

| Attempt | Payout |
|---|---|
| First clear | 100% |
| 2nd | 70% |
| 3rd | 50% |
| 4th | 35% |
| 5th+ | 25% (floor) |

The floor is never zero — a player who wants to grind is allowed to, just inefficiently. But the optimal strategy must never be "replay Level 1 for three hours," because that is both boring and a design failure.

### 8.5 The personal-best bonus

Beating your previous best kill count on any level grants a flat Scrap bonus, **including on a loss.** This is the mechanical reward for the exact psychological loop in §3, and it means a failed run that got further than the last one is unambiguously progress.

---

## 9. Level progression

### 9.1 Level table

| Lvl | Name | Enemies | New | Power | Slice |
|---|---|---|---|---|---|
| 1 | The Outskirts | 100 | Grunt | 10 | ✅ |
| 2 | Refinery Gate | 250 | Runner | 25 | ✅ |
| 3 | The Narrows | 600 | Tank | 60 | ✅ |
| 4 | Collapsed Span | 1,500 | Swarm | 140 | |
| 5 | Ore Fields | 4,000 | Shield | 320 | |
| 6 | The Basin | 10,000 | Elite | 700 | |
| 7 | Reactor Yard | 25,000 | — | 1,500 | |
| 8 | Last Stand | 50,000 | — | 3,200 | |

### 9.2 Power rating

Each level displays a **Recommended Power** against the player's current Power (a single scalar derived from total Scrap invested). This is a suggestion, never a gate.

```
  ┌─────────────────────────────────┐
  │  LEVEL 03                       │
  │  THE NARROWS                    │
  │                                 │
  │  Enemies          600           │
  │  Recommended      60            │
  │  Your power       47            │
  │                                 │
  │  BEST: 412 / 600  (69%)         │
  │                    [ DEPLOY ]   │
  └─────────────────────────────────┘
  ```

Showing an under-powered player exactly how short they are, and then letting them go anyway, is the invitation. "Fuck it, let's try" is a designed moment.

### 9.3 Level graph

Levels branch rather than forming a single chain, so the player always has two things to bang their head against and can route around a wall.

```
        L1
         │
    ┌────┴────┐
    L2        L3
    └────┬────┘
        L4
    ┌────┴────┐
    L5        L6
    └────┬────┘
        L7 ─▶ L8
```

### 9.4 Map design

Each level is a distinct **flow problem**, not a reskin:

- **L1 — single lane.** Teaches turret + target + base.
- **L2 — two lanes converging.** Teaches coverage; punishes stacking everything in one spot.
- **L3 — open field into a hard chokepoint.** The first level where AoE positioning beats raw DPS.
- **L4+ — multiple entrances, obstacles, split paths.** Compression and leak management.

### 9.5 Endless mode [V1]

One map, infinite escalating spawns, runs until the framerate breaks. Doubles as the project's public benchmark and produces the portfolio screenshot.

---

## 10. Active abilities

Cooldown-based, player-triggered, aimed with the cursor. These carry the battle-phase agency (§4.3).

| Ability | Effect | CD | Scope |
|---|---|---|---|
| **Airstrike** | Heavy damage in a line across the field | 25s | ✅ |
| **Overcharge** | Target turret: ×2 fire rate 4s, then 8s overheat | 15s | ✅ |
| **EMP** | Stuns all enemies in a large radius for 3s | 40s | [V1] |
| **Napalm** | Ground fire for 8s; applies Burn (synergy setup) | 35s | [V1] |
| **Nuke** | Clears the screen. Once per battle. | — | [V1] |

Abilities are the emotional peak of a battle — the moment where a losing run gets clawed back. They should be loud, slow enough to watch, and generous with screenshake.

---

## 11. Build variety and synergies

The game's identity lives here. Named, discoverable combinations that are strictly more than the sum of their parts.

### 11.1 Named synergies

**CONDUCTIVE BURN** — *Flamethrower (Ignite) + Tesla (Conductive)*
Burning enemies conduct. Tesla chains prefer them and deal +100%. At high density, Ignite spreads Burn faster than enemies can advance, and Tesla converts the entire burning mass into a single cascading chain. The signature build.

**COMPRESSION** — *Cannon (Knockback) + Missile (Cluster)*
Knockback shoves the front rank into the rank behind it, spiking local density. Missiles target the densest cluster. The cannon manufactures the target the missiles want.

**SHATTER** — *EMP / slow + Cannon* [V1]
Stunned enemies take +50% impact damage. Turns a defensive cooldown into a damage multiplier.

**OVERKILL CASCADE** — *Machine Gun (Ricochet) + any density*
Ricochet bounces scale with the number of nearby enemies. The flat turret becomes a density turret. Rewards a player who read §5.1 correctly.

### 11.2 Discovery

Synergies are **not** hidden. When both halves are owned, the tree draws a connecting line between the nodes and names the combination. Hiding them just means most players never find them, and a build system nobody notices is a build system that doesn't exist.

---

## 12. Visual and audio direction

### 12.1 Art direction: detailed vector, industrial sci-fi

Not sprites, not primitives. **Constructed vector shapes** — each entity is a small assembly of polygons with thick outlines, a limited palette, and secondary motion.

An enemy at full detail is built from ~15 primitives: torso, head, weapon arm, two legs on a sine-driven walk cycle, an outline pass, a drop shadow, and a damage-flash overlay. Each instance carries a random phase offset so the crowd shimmers organically instead of marching in lockstep. Detail comes from **palette, outline weight and motion**, not polygon count.

**Palette**
- Background: near-black, deep desaturated rust
- Terrain: dark warm grey, orange rimlight
- Enemies: sickly green-white, black outline
- Player/turrets: cold cyan-white
- Fire: white core → yellow → deep orange
- Electricity: white core → cyan → violet

Cold player, warm world, sickly enemies. The player can always read the battlefield by hue alone, which is what keeps 10,000 entities from becoming mud.

### 12.2 Density-driven LOD

Ten thousand articulated creatures cannot be drawn. Detail therefore degrades by **local screen-space density**, not by global count:

| Tier | Condition | Draw |
|---|---|---|
| **0** | sparse neighbourhood | Full articulation, outline, shadow, walk cycle (~15 prims) |
| **1** | moderate | Torso + head + weapon silhouette, bob only (~4 prims) |
| **2** | packed | Single directional shape + motion trail (1 prim) |

Because the trigger is *local* density, a lone scout crossing an empty field stays fully detailed while the chokepoint reduces to a churning mass — which is exactly what you want, since nobody can see individual legs inside a crowd of four hundred anyway. The system is invisible when it works and is a strong technical talking point.

### 12.3 Juice

The difference between "a lot of enemies died" and "HOLY SHIT." Non-negotiable, budgeted as a full milestone (§16, M6):

- **Hitstop** — 40ms freeze on Elite kills and ability detonations
- **Screenshake** — amplitude scaled to kills-per-second, hard-clamped so the late game doesn't induce nausea
- **Damage numbers** — aggregated above a threshold; never 10,000 individual popups
- **Corpses** — fade over 2s, capped by a ring buffer; the battlefield should visibly fill with the dead
- **Scrap particles** — arc from kills to the counter. The single most important reward animation in the game.
- **Muzzle flash, shell casings, impact sparks, ground scorch**

### 12.4 Audio

The core problem at 4,000 kills/minute is **voice limiting**. Playing one death sound per enemy is unlistenable and will blow the mixer.

The solution is **aggregation**: sample the kill rate each frame and drive a continuous layered "crowd destruction" bed whose intensity tracks it, with discrete one-shots reserved for meaningful events (Elite death, ability detonation, base hit). Individual gunfire similarly collapses into a sustained loop above a threshold rate. Hard cap of ~32 concurrent voices with priority-based stealing.

Done right, the audio itself communicates progression: the early game is individual pops, the late game is a continuous roar.

---

## 13. UI / UX

### 13.1 Principles

1. **The retry path is the most optimised path in the game.** RETRY is always in the same screen position, always one keypress, never behind a confirmation.
2. **No modal dialogs.** Ever.
3. **The tree is readable at a glance.** Affordable nodes glow; unaffordable ones are dim but visible, because seeing what you can't afford yet is the motivation.
4. **Everything is skippable.** Every animation, transition and result screen accepts a keypress to complete instantly.

### 13.2 The Battle Report

The most important screen in the game. It has two jobs: pay the player, and manufacture the next run.

```
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

                  DEFEATED

            847 / 1,000  destroyed
                  03:42 survived

         ┌─────────────────────────┐
         │   + 1,247  SCRAP        │
         │     ↑ incl. +200 best   │
         └─────────────────────────┘

              NEW BEST  ·  was 612

  ─────────────────────────────────────────

   FAILURE ANALYSIS
   Breach at NORTH LANE, 03:12
   Peak density there      340 / tile
   Your DPS at that point  2,100
   Estimated requirement   3,400

   ▸ Suggested: FIREPOWER › Overclock
   ▸ Suggested: TURRETS  › Flamethrower

  ─────────────────────────────────────────

   Machine Gun  ████████████████░░░░  42%
   Flamethrower ████████████░░░░░░░░  31%
   Cannon       ███████░░░░░░░░░░░░░  18%
   Burn         ███░░░░░░░░░░░░░░░░░   9%

   Shots 18,291  ·  Accuracy 63%

  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

        [ UPGRADE ]        [ RETRY ]
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

**Failure Analysis is the standout feature.** Most games in this genre say "you lost." This one says *where, when, and by how much you missed* — and then points at the two nodes that fix it. Every number in that panel already exists in the simulation; surfacing it costs almost nothing and it is what converts a defeat into an immediate, specific, actionable "one more run."

The damage-attribution breakdown is a genuine balancing instrument as well as a toy. A turret sitting at 4% across ten runs is a turret that needs a buff, and the game will tell you so without a spreadsheet.

### 13.3 Screens

`MAIN MENU → LEVEL SELECT → PREPARE → BATTLE → REPORT → UPGRADE TREE`

Every arrow is reversible except `BATTLE → REPORT`.

### 13.4 UI implementation

- **Dear ImGui** for all developer tooling: entity inspectors, live tuning sliders, the profiler overlay, spawn controls. This is not a shortcut — in-house ImGui tools are standard practice at studios, and a project that ships with real debug tooling reads as professional rather than student.
- **Custom immediate-mode UI** for player-facing screens, drawn through raylib. The tree is rects, lines, text and hit-testing; it does not need a framework, and hand-rolling it keeps the player-facing layer free of programmer-art tells.

---

## 14. C++ technical architecture

> This section is the portfolio. The game is the excuse; the engineering is the artifact. Every decision below should be defensible in an interview, including the decisions *not* to do something.

### 14.1 Stack

| | |
|---|---|
| Language | C++20 |
| Platform library | raylib 5.x |
| Debug UI | Dear ImGui (+ rlImGui) |
| Build | CMake + FetchContent |
| Compiler | Apple Clang (M5, arm64) |
| Tests | doctest |
| Profiling | Tracy, plus Instruments for platform-level work |

**Why raylib over an engine.** The goal is to demonstrate C++ that the author chose to write. In Godot with GDExtension, a reviewer opening the repo sees `godot::Node2D`, `_bind_methods()` and `Variant` marshalling — real skill, but *engine integration* skill, and it buries the actual engineering. Worse, hitting the entity counts this design targets would mean bypassing Godot's node system for `RenderingServer` anyway, paying the engine's complexity without using the part that saves time. raylib supplies a window, an input poll, an audio device and a batched 2D renderer, and gets out of the way. Everything interesting is authored.

### 14.2 Module layout

```
  src/
  ├── core/          GameLoop · Time · Input · Events · Log · Assert
  ├── math/          Vec2 · Rect · RNG (PCG32) · easing
  ├── render/        Renderer · SpriteBatch · LODSystem · Particles · Camera
  ├── sim/           World · EnemyPool · TurretPool · ProjectilePool
  │                  MovementSystem · TargetingSystem · DamageSystem · SpatialHash
  ├── gameplay/      Level · SpawnDirector · UpgradeTree · Progression · Abilities
  ├── ai/            FlowField · TargetingStrategy · Behaviors
  ├── ui/            Screens · TreeView · BattleReport · DebugPanels
  └── persist/       SaveGame · Serialization · ReplayRecorder
```

`sim/` must not include anything from `render/` or `ui/`. This is enforced, not merely intended (§14.9) — it's what makes headless benchmarking and deterministic replay possible.

### 14.3 Data layout: typed SoA pools, and no ECS

Entities live in **fixed-capacity, struct-of-arrays pools**, one per type:

```cpp
struct EnemyPool {
    std::vector<Vec2>     position;    // hot: touched every tick
    std::vector<Vec2>     velocity;
    std::vector<float>    health;
    std::vector<uint8_t>  type;
    std::vector<uint8_t>  lodTier;
    std::vector<uint32_t> flags;       // burning, stunned, shielded
    std::vector<float>    animPhase;   // cold: render only
    uint32_t count = 0;
};
```

**The explicit decision not to build a generic ECS is itself part of the portfolio.** A hand-rolled archetype ECS is a month of work that this game does not need — there are five entity types, all known at compile time, and none of them need runtime composition. Typed pools give the same cache behaviour with a fraction of the complexity and no type erasure. Building an ECS here would demonstrate that the author knows the *word*; declining to build one demonstrates that the author knows the *tradeoff*. The README states this reasoning outright.

**Invariant: zero heap allocation during a battle.** All pools are sized at level load from the level's declared maxima. A debug allocator hook asserts on any allocation between battle start and battle end. This is the kind of constraint that is trivial to hold if designed in from hour one and impossible to retrofit at week five.

### 14.4 Polymorphism: where it belongs and where it doesn't

A deliberate, defensible split:

- **Turrets use virtual dispatch.** `TargetingStrategy` is an abstract base with `selectTarget()`. There are at most a few dozen turrets, each calling it once per fire interval — a few hundred virtual calls per second. The cost is unmeasurable and the extensibility is genuinely worth it.
- **Enemies use none.** No virtual functions, no inheritance, no per-entity indirection. Behaviour is a `type` byte driving a branch in a tight loop over contiguous arrays. At 50,000 entities × 60Hz that is 3,000,000 updates per second, and a vtable lookup per entity per tick means three million dependent loads and a thrashed instruction cache.

The general rule the codebase follows: **virtual dispatch scales with the number of *kinds*, not the number of *things*.** Applying it uniformly in either direction would be the error.

### 14.5 The systems that make 50,000 possible

**Flow-field pathing** — one Dijkstra pass per map at load, `O(cells)`. Per-enemy movement is a grid sample plus a separation force: `O(1)` per entity, no search, no per-agent state. This is *the* enabling decision; without it nothing else matters.

**Uniform-grid spatial hash** — cell size tuned to roughly the largest query radius. Serves both turret target acquisition and neighbour separation. Rebuilt each tick by counting sort (two passes, no allocation), which is faster than incremental maintenance at these counts and far simpler to reason about.

**Object pooling with free-list indices** — projectiles, particles and corpses are recycled, never allocated. Handles are `{index, generation}` pairs so a stale reference to a recycled slot is detectable rather than a silent use-after-free.

**Hitscan-with-visual** — most turrets resolve damage instantly and spawn a purely cosmetic tracer. Real projectile entities exist only where the gameplay needs travel time (Cannon shells, Missiles). This keeps the projectile pool in the hundreds rather than the tens of thousands, for no perceptible loss.

**Batched rendering by LOD tier** — entities are bucketed by tier, then each bucket is submitted as one batch. Draw call count is a function of tier count, not entity count.

**Fixed 60Hz timestep, decoupled render with interpolation** — the simulation advances in fixed integer steps; the renderer interpolates between the last two states. Frame rate and simulation rate are fully independent, which is what makes determinism (§14.6) achievable at all.

**[V1] Multithreading** — movement and separation are trivially parallel over enemy ranges, and are the obvious job-system candidate once single-threaded work is exhausted. Deliberately *last*: threading before profiling is how projects acquire heisenbugs instead of speed. The single-threaded version must be measured and optimised first, and the before/after is a far better story than "it was threaded from the start."

### 14.6 Determinism, replay and headless benchmarking

The simulation is deterministic given a seed. This requires: a fixed timestep, an explicit PCG32 RNG per level (never `rand()`, never anything global), no reliance on iteration order over unordered containers, and — critically — **no floating-point drift from render-side state leaking into sim**, which is guaranteed by the `sim/` isolation rule in §14.2.

Three things fall out of this for nearly free:

1. **Replays.** Record the seed plus the player's timestamped inputs; play it back exactly. The whole recording is a few hundred bytes.
2. **Reproducible bugs.** "It crashed at Level 6, seed 817293, tick 14,203" is a bug report you can act on.
3. **Headless benchmark mode.** `--bench --level 8 --ticks 20000 --no-render` runs the simulation with the renderer detached and reports tick timings. This makes performance work measurable rather than vibes-based, and it generates the graphs the README needs.

### 14.7 Performance targets

Machine: MacBook Pro M5, 24 GB.

| Milestone | Entities | Target |
|---|---|---|
| Slice | 5,000 | 120 fps sustained |
| Slice floor | 5,000 | never below 60 fps |
| V1 | 50,000 | 60 fps sustained |
| Benchmark | 100,000 | headless, measured, documented |

Frame budget at 120 fps is **8.33 ms**:

```
  simulation      3.0 ms   movement · separation · targeting · damage
  render          3.0 ms   LOD bucketing · batching · particles
  ui + audio      1.0 ms
  headroom        1.3 ms
```

Memory: under 256 MB resident at 50,000 entities. Zero allocations during a battle (asserted).

**These numbers are tracked from Milestone 1, not measured at the end.** A `--bench` run is part of the definition of done for every milestone, and the results are committed as a CSV so the optimisation curve is a documented artifact rather than a claim.

### 14.8 Persistence

Save state is small: unlocked nodes, Scrap, per-level bests, settings. Versioned binary with a magic number and a schema version, plus a JSON debug export for eyeballing.

Written atomically — temp file, `fsync`, rename — because a corrupted save in a game whose entire value is accumulated progress is the worst possible bug.

### 14.9 Engineering practice

This is the half that separates a portfolio project from a hobby project.

- **CMake + FetchContent**, one-command build from a clean clone. If a reviewer can't build it in 60 seconds, they won't.
- **doctest** unit tests on the parts where correctness is non-obvious and testable: flow-field generation, spatial hash queries, upgrade cost curves, payout formulas, save round-trips. Not on rendering.
- **A determinism regression test** — run 10,000 ticks from a fixed seed, hash the world state, compare against a committed golden hash. This single test protects the feature that makes everything else in §14.6 work, and it is the test most worth pointing at in an interview.
- **A layering test** that greps `sim/` for `render/`, `ui/` and raylib includes and fails the build on a hit. Architecture that isn't enforced decays.
- **GitHub Actions** building macOS and Linux, running tests and the benchmark on every push.
- **Clear commit history.** The commits are part of the artifact — a reviewer reads them. No `wip` commits, no 4,000-line "added game" dumps.
- **A README that leads with a GIF**, then the perf graph, then the three or four architectural decisions worth defending. Most reviewers spend 90 seconds on a repo; the README is what they read.

---

## 15. Performance strategy

The optimisation work is staged deliberately, because **the sequence is the story.** A project that was fast from the first commit demonstrates nothing; a project with a measured curve from 1,000 to 100,000 demonstrates method.

| Stage | Entities | Work | Expected outcome |
|---|---|---|---|
| 0 | 1,000 | Naive AoS, per-entity update, `O(n²)` neighbour checks | Works fine. Ship it. |
| 1 | 5,000 | Profile. `O(n²)` dominates → spatial hash | ~10× on neighbour queries |
| 2 | 10,000 | Profile. Cache misses dominate → AoS to SoA | 2–3× on movement |
| 3 | 25,000 | Draw calls dominate → LOD + batching | Render cost decoupled from count |
| 4 | 50,000 | Allocation churn → pooling, zero-alloc invariant | Frame time variance collapses |
| 5 | 100,000 | Single thread saturated → job system over movement | Scales with cores |

**Stage 0 is written naively on purpose and is not skipped.** Committing the slow version first, profiling it, and committing the fix produces a git history that *demonstrates* the method — which is worth considerably more to a reviewer than arriving at the fast version by assertion. Each stage lands as its own commit with the before/after benchmark numbers in the message.

---

## 16. Roadmap

**Target: 4–6 weeks, evenings and weekends. The deliverable at week 6 is a vertical slice, not v1.0.**

Every milestone ends in a playable, committed, benchmarked state. There is never a week where the game doesn't run.

### M1 — Skeleton (week 1)
CMake + raylib + ImGui building clean. Window, fixed timestep with interpolation, camera. Grid and flow field generating and debug-drawn. 100 Grunts walking a lane into a base that has HP. No turrets, no combat.
**Done when:** enemies flow, the base dies, and a `--bench` run prints tick timings.

### M2 — Combat (week 2)
Turret pool, hardpoints, spatial hash, target acquisition, projectiles, damage, death. Machine Gun only. The full loop from spawn to kill.
**Done when:** one turret defends against 100 Grunts and it's legible what's happening.

### M3 — The loop (week 3)
Level definitions with spawn curves. Battle end states. Battle Report with Scrap payout. A 6-node upgrade tree. Save/load. RETRY.
**Done when:** the run → upgrade → retry cycle is closed and the sub-5-second retry is measured. **This is the milestone that proves the game.** If this isn't fun, stop and fix the design before adding content.

### M4 — Content (week 4)
Cannon and Flamethrower. Runner and Tank. Levels 2 and 3. Tree to 24 nodes with the transformation lines. Targeting modes. Airstrike and Overcharge. Time controls.
**Done when:** two visibly different builds can clear Level 2.

### M5 — Scale (week 5)
Performance stages 1–3 from §15 (the techniques, applied at slice counts), each its own commit with before/after numbers. LOD system. Push to 5,000 entities at 120 fps. Determinism regression test.
**Done when:** the benchmark CSV shows the curve and the golden-hash test passes.

### M6 — Juice (week 6)
Hitstop, screenshake, particles, damage numbers, corpses, Scrap arcs. Audio with aggregation and voice limiting. Failure Analysis panel. README with GIF and perf graph.
**Done when:** a stranger watching the Level 3 endgame says "whoa."

### Beyond the slice
**v0.5 [V1]** — Tesla, Missile, Swarm, Shield, Elite, Levels 4–6, tree to 80 nodes, synergies, endless mode, multithreading, 50,000 entities.
**v1.0 [V1]** — Levels 7–8, full ability set, controller support, settings, polish.
**v2 [V2]** — Overdrive prestige. Not designed further until v1.0 ships.

### Cut list, in order
If week 5 arrives and the schedule is short, these go, in this sequence: Level 3 → Airstrike → Flamethrower → the 24-node tree drops to 12. **M3 and M6 are never cut.** A rough game with a tight loop and good juice is a portfolio piece; a feature-complete game with neither is a tech demo.

---

## 17. First milestone — exact implementation spec

The concrete starting point. Nothing here is ambiguous; it is written to be executed.

### 17.1 Environment

```bash
brew install cmake tracy
```

Clang is already present via Xcode CLT. raylib, Dear ImGui and doctest arrive through `FetchContent` — nothing else is installed globally.

### 17.2 M1 scope

**In:**
- CMake project, C++20, `-Wall -Wextra -Werror`, arm64, Debug and Release
- raylib window 1280×720, resizable, vsync
- rlImGui with an FPS/frame-time overlay
- `GameLoop` — fixed 60Hz accumulator, render interpolation, clamped max frame time
- `Vec2`, `Rect`, PCG32 RNG
- `Grid` — 64×36 cells over the play area
- `FlowField` — Dijkstra from the base; debug arrow overlay toggled from ImGui
- `EnemyPool` — SoA, fixed capacity 100,000, `spawn()` / `kill()` / `update()`
- Movement: flow-field sample + naive `O(n²)` separation (**Stage 0 — intentionally slow, do not optimise**)
- One hand-authored lane map
- `Base` with HP; enemies on arrival deal damage and despawn
- Enemies drawn as LOD Tier 2 shapes only
- `--bench --ticks N --no-render` printing min/mean/p99 tick time
- doctest covering flow-field correctness and RNG determinism

**Explicitly out:** turrets, projectiles, combat, UI beyond the debug overlay, save/load, audio, art, LOD tiers 0–1, spatial hash.

### 17.3 Definition of done

1. `cmake -B build && cmake --build build` succeeds from a clean clone with zero warnings.
2. 100 Grunts spawn, flow along the lane, reach the base, and reduce it to 0 HP.
3. The ImGui overlay shows entity count, frame time and tick time live.
4. A slider raises the spawn count to 5,000; **it is expected to run badly.** That is the Stage 0 baseline.
5. `--bench --ticks 10000 --no-render` prints timings, and the result is committed to `docs/bench/m1.csv`.
6. `ctest` passes.
7. Committed with a message recording the M1 baseline numbers.

### 17.4 Why this order

M1 deliberately builds the two things that are impossible to retrofit — **the fixed-timestep deterministic core, and the SoA pool layout** — before anything fun exists. Both are cheap on day one and catastrophic in week five. The naive `O(n²)` separation is left in on purpose: it is the baseline that Milestone 5's optimisation work is measured against, and that measured delta is the single most valuable artifact this project produces.

---

## Open questions

1. **Is the passive battle actually fun?** The largest unknown. §4.3 is the mitigation plan; M3 is the decision point.
2. **Does the 75% loss payout feel generous or pointless?** Needs playtesting; expect to tune upward.
3. **Is `DENSEST` targeting readable to the player,** or does it look like the turret is misfiring? May need a visual tell.
4. **Working title.** See the note at the top.
