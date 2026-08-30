# LAST STAND — Milestone 6: Juice, UI and Audio — Implementation Plan

**Goal:** Turn a correct, fast simulation into a game somebody wants to watch.
Hitstop, screenshake, particles, damage numbers, corpses and Scrap arcs;
procedural audio with aggregation and voice limiting; the full screen set from
title to options; the Failure Analysis panel; and an aesthetic pass that makes
the battlefield readable at 5,000 entities.

**Done when:** a stranger watching the Level 3 endgame says "whoa."

**Spec:** `docs/GDD.md` §12.1 (art direction), §12.3 (juice), §12.4 (audio),
§13.1–13.4 (UI principles, the Battle Report, screens), §10 (abilities are the
emotional peak and should be generous with screenshake).

---

## Constraints carried forward

- `sim/`, `math/`, `core/`, `ai/`, `gameplay/`, `persist/` and the new `fx/`
  never include raylib, `render/`, `ui/` or `audio/`. The layering test gains
  `fx`.
- **The simulation does not change.** Juice reads sim state and never writes
  it. The three golden hashes in `tests/test_golden.cpp` must still pass at the
  end of the milestone, untouched. That is the check that M6 stayed cosmetic.
- Zero heap allocation during a battle still holds, and now covers particles,
  corpses and damage numbers — all fixed-capacity pools, asserted by
  `tests/test_noalloc.cpp`.

---

## Deviations from spec, recorded

1. **No binary assets. Audio is synthesised.** The GDD assumes sound files. This
   milestone generates every sound procedurally at startup — noise bursts
   through an envelope for gunfire, a pitched thump for impacts, filtered noise
   for fire — from a pure, unit-tested synthesiser. It keeps "one command from a
   clean clone" true, keeps a portfolio repo free of committed binaries, and
   makes the mix tunable by constant rather than by re-export. Real recorded
   audio is a drop-in replacement later.
2. **Dear ImGui stays deferred**, sixth milestone running: rlImGui still has no
   tagged release. The debug overlay the GDD §13.4 assigns to ImGui is drawn
   with the same immediate-mode widgets as the player-facing UI.
3. **"Accuracy 63%" is cut from the Battle Report.** Turrets only fire when
   target acquisition returns a target, so accuracy is 100% by construction and
   the line would be a lie. It is replaced with peak kills-per-second, which is
   a real number and a better brag.
4. **Failure Analysis lives in `gameplay/`, not `sim/`.** Breach lane, breach
   time, peak density and the DPS estimate are sampled from the world by a
   telemetry object the Session owns. Putting the counters in `World` would have
   meant re-blessing the golden hashes for a cosmetic feature — the exact thing
   the golden hashes exist to make you think twice about.
5. **Damage attribution is derived, not tracked.** Per-turret kills already
   exist; burn kills are `totalKills - Σ turret.kills`, because arrivals despawn
   without counting as kills. No new simulation state.

---

## File structure (additions)

```
src/
├── fx/                        NEW  pure, no raylib, unit-tested
│   ├── Juice.h/.cpp                hitstop + screenshake state
│   ├── Particles.h/.cpp            fixed-capacity pool, one update loop
│   ├── Corpses.h/.cpp              ring buffer of the dead
│   └── DamageNumbers.h/.cpp        aggregated, never one popup per hit
├── audio/
│   ├── Synth.h/.cpp           NEW  pure PCM synthesis (testable, no raylib)
│   └── AudioEngine.h/.cpp     NEW  raylib playback, voice limiting, beds
├── ui/
│   ├── Widgets.h/.cpp         NEW  immediate-mode button/slider/bar/panel
│   └── Screens.h/.cpp         NEW  title, menu, options, level select,
│                                   prepare, pause, report, tree
├── render/
│   ├── Theme.h                NEW  palette, type scale, spacing
│   └── Renderer.h/.cpp        CHANGE  camera shake, fx layers, aesthetic pass
├── gameplay/
│   └── Telemetry.h/.cpp       NEW  battle telemetry + Failure Analysis
├── app/
│   ├── Settings.h/.cpp        NEW  options + defaults
│   ├── Session.h/.cpp         CHANGE  screens, telemetry, fx feed, pause
│   └── ...
└── persist/SaveGame.h/.cpp    CHANGE  v2: settings block, v1 migration
```

---

## Task 1: Settings and save v2

`Settings` holds master/sfx/music volume, screenshake scale, hitstop on/off,
damage numbers on/off, LOD on/off, default time scale, and the debug overlay
toggle. `SaveData` gains a settings block and moves to version 2; a v1 file
still loads, with settings defaulted, because a save is the player's whole
progress and refusing to read one is the worst possible bug (GDD 14.8).

**Tests:** v2 round-trip; a v1 payload loads with defaults; a truncated or
corrupt file is refused without touching the caller's data.

## Task 2: Juice — hitstop and screenshake

Pure state machines in `fx/Juice.h`. Hitstop scales the tick budget for a few
frames; screenshake is a decaying amplitude driven by kills-per-second, ability
detonations and base hits, hard-clamped so the late game does not induce
nausea (GDD 12.3). Both are render/pacing concerns: the fixed timestep still
advances in whole ticks, hitstop simply withholds them.

**Tests:** shake decays to zero; amplitude clamps; kills-per-second drives it
monotonically; hitstop consumes exactly its budget then releases.

## Task 3: Particles, corpses, damage numbers

Three fixed-capacity pools, swap-removed like `EnemyPool`, all sized at
construction. Emitters: muzzle flash, impact spark, death burst, burn ember,
Scrap arc (the single most important reward animation in the game), airstrike
detonation. Corpses fade over 2 s from a ring buffer, so the battlefield
visibly fills with the dead and the memory does not grow. Damage numbers
aggregate per screen region above a threshold rate — never ten thousand
popups.

**Tests:** pools never exceed capacity; expiry frees slots; Scrap arcs reach
their target; aggregation collapses a burst into one number; zero allocation
during a battle.

## Task 4: Audio

`Synth` builds PCM into a caller-owned buffer: noise with an ADSR envelope,
decaying sine thump, filtered crackle, and a simple detuned pad for the bed.
`AudioEngine` uploads them once, then plays with a hard cap of 32 voices,
priority-based stealing, and per-sound cooldowns.

Aggregation is the real problem (GDD 12.4): at 4,000 kills a minute, one death
sound per enemy is unlistenable. Above a threshold, gunfire and deaths collapse
into a continuous bed whose intensity tracks the rate, with discrete one-shots
reserved for meaningful events — ability detonation, base hit, battle end. The
audio itself then communicates progression: early game is individual pops, late
game is a roar.

**Tests (on `Synth` and the mixer policy, not on playback):** envelopes start
and end at zero; output stays in range; voice allocation never exceeds the cap;
stealing prefers the lowest priority and oldest voice; the aggregation
threshold switches modes at the documented rate.

## Task 5: Theme and the aesthetic pass

`render/Theme.h` centralises the GDD 12.1 palette and a type scale, so the game
stops being a collection of ad-hoc `Color{}` literals. The battlefield gets a
vignette, warm rimlight on wall edges, a base that reads its own health, muzzle
flashes, thicker tracers with a bright core, and the camera shake applied as a
render-space offset. Enemy tiers keep the M5 LOD.

## Task 6: UI

`ui/Widgets` is a small immediate-mode layer — panel, label, button, slider,
toggle, bar, and a focus/keyboard model — because the tree is rects, lines,
text and hit-testing and does not need a framework.

`ui/Screens` draws: **Title** (over a live battle running dimmed behind it —
the simulation is fast enough to be the menu background, so it is),
**Main menu**, **Options**, **Level select**, **Prepare**, **Pause**,
**Report**, **Tree**.

Held to GDD 13.1: RETRY is always in the same position and always one key; no
modal dialogs; affordable tree nodes glow and unaffordable ones stay visible;
every animation completes instantly on a keypress.

## Task 7: Telemetry and Failure Analysis

`gameplay/Telemetry` samples the battle: per-lane density over time, when and
where the base first took damage, peak kills/sec, damage attribution by source.
`analyse()` turns that into the panel — breach lane and time, peak density
there, estimated DPS against estimated requirement, and the two tree nodes that
close the gap.

**Tests:** breach detection picks the first damaging lane; the suggestion
engine picks affordable, relevant, unowned nodes; a victory produces a
victory-shaped analysis rather than an empty failure one.

## Task 8: Wiring, transitions, skippability

Screen state machine in `Session`; fades between screens; the report's numbers
count up and any key completes them instantly. Pause on ESC during battle.

## Task 9: Docs and close

`docs/bench/m6.csv` to show the simulation is unchanged and the fx pools cost
what they claim. README gets the screen set and the audio note. GDD open
question 2 (the loss payout) gets a recorded answer.

---

## Milestone 6 exit criteria

1. The three golden hashes still pass, untouched — M6 changed nothing in `sim/`.
2. Zero allocations during a battle, now including every fx pool.
3. Title → menu → options → level select → prepare → battle → pause → report →
   tree, all reachable and all reversible except battle → report.
4. Settings persist; a v1 save still loads.
5. Audio: 32-voice cap, aggregation above the threshold, no clipping.
6. Failure Analysis names a lane, a time, a gap and two nodes.
7. `ctest` green, zero first-party warnings, layering clean including `fx`.

**Explicitly out:** Tesla/Missile/Swarm/Shield/Elite, Levels 4–8, synergy lines
in the tree (the V1 tree is where they earn their place), endless mode,
multithreading, controller support, and recorded audio assets.
