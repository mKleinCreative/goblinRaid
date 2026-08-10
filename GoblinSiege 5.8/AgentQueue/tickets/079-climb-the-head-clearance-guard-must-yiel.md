---
id: 079
title: "Climb: the top-out was aiming at a 79-degree fascia, and the 0.15 deck gate waved it through"
agent: claude-climbrebuild
status: done
claimed: 2026-08-08T02:40Z
build: none
waiting_on:
evaluated: 2026-08-08T20:18:52Z
files: 
  - Content/Blueprints/BP_GSPlayerCharacter.uasset
---

## Goal

Michael, after the previous test: *"it almost worked, sometime's it'd launch me up in the air. there
was also foot sliding"* - then, after the launch and slide were fixed, `read it`. The log showed one
10.3s climb that reached Zabs 1870, hung there, drained to 3 stamina and fell off.

## Generate

Three changes, all in the top-out accept path of `ClimbStaminaExits`.

| node | was | now | why |
|---|---|---|---|
| `578D7271` vector*float pin B | **-80** | **-120** | probe inset; -80 lands on the fascia on 8 of 8 sampled heights |
| `55D17294` float>=float pin B | **0.15** | **0.4695** | deck gate; 0.4695 is the character's own `WalkableFloorZ` |
| **5 new nodes** | - | sky check | rejects a "ledge" that has the building's roof above it |
| `ABBE87E0` float>float pin B | **0.35** | **0.10** | top-out stall bypass - see "the counter race" below |
| `7599E879` float>float pin B | **0.12** | **0.60** | lean-out trigger, demoted to a fallback |

**The sky check** (`88CDAEB5` LineTrace, spliced between the ledge probe `5E1A0842` and the head
trace `A7B5471D`): trace from `deckImpactPoint + (0,0,1500)` down to `deckImpactPoint + (0,0,15)`.
A hit means the deck is *under* something, so it is an interior floor, not a roof. `NOT`-ed and
`AND`-ed into the accept condition ahead of `FCABE512.A`.

New GUIDs, for rollback: `F5DD5E82` addLo, `E854959B` addHi, `88CDAEB5` skyTrace, `470F0FBC` notSky,
`82F641B4` andSky.

## Evaluate

**Measured on `SM_MERGED_House_Medium_11`, against the real (complex-as-simple) collision, using the
Blueprint's own probe geometry** - sphere r25 sweeping `actorZ+460` down to `actorZ+30`:

| inset | walkable deck found | what it actually hits |
|---|---|---|
| **80 (shipped until now)** | **0 of 8** | fascia, `nz 0.18` |
| **120 (now)** | **8 of 8** | roof deck, `nz 0.78` |
| 160 | 0 of 8 | rib, `nz 0.18` |
| 200 | 8 of 8 | roof deck |

The roof is ribbed at ~40uu, which is why this is not monotonic and why the old value was not
"nearly right" - it was wrong on every sample.

**The old 0.15 gate was the thing that made this a silent failure.** A fascia at `nz 0.18` passes
`>= 0.15`. So the top-out *did* fire, committed `ClimbLedgeLocation` to a 79-degree slope, and dropped
him - which is exactly Michael's earlier report: *"I climbed up the building, it almost got me over,
bumped me off."* I read that as a collision-normal problem at the time. It was this.

**The sky check separates interior floors from roofs perfectly: 22 correct, 0 wrong** across
Zrel 300-1900. It matters because the probe now reaches 120uu inboard and, with
`UseComplexAsSimple`, there is no longer any wall to stop it hitting an interior floor -
15 of the 22 sampled heights find one, at `nz +1.00`, which passes any gate we could set. Those are
currently masked by #074's head-clearance guard, but the `ClimbBlockedSeconds > 0.35` bypass
sitting next to it in the same `OR` would have teleported him inside the house. **That bug is
already in Michael's report list** (*"I tried to climb again and it pushed me inside the building"*)
and this is the first explanation of it I have that is backed by measurement.

I first tried the obvious obstruction test - line trace from head to deck - and **it failed, 6 of 22
wrong**: the eave is between head and deck by construction, so it rejects the real roof. Recorded
because it is the intuitive implementation and it is wrong.

**NOT PLAYED.** Trace simulation and graph readback only. Compiles `UP_TO_DATE` at 767 nodes; every
wire and pin re-read after the fact rather than trusted from the `True` return.

### The counter race (second play test, 2026-08-08)

Michael: *"the climbing looks normal but still that lip at the end."* Correct on both counts - the
sustained climb now runs clean, and he stops **dead at Z 1899.9 on every attempt**, which is the
eave underside.

**The lean-out and the top-out are both driven by `ClimbBlockedSeconds`, and the lean-out was
winning.** Straight from the log, frame by frame:

```
303   blocked 0.055
304   blocked 0.106
305   blocked 0.157   offset 70.0 -> 94.1     <- lean-out fires (its threshold was 0.12)
306   blocked 0.210   offset      -> 112.1
307   dZ +30.6        blocked -> 0.000        <- unjammed, counter wiped
```

The lean-out nudged him free just enough to reset the very counter the top-out needed, so `blocked`
peaked at **0.210 across 1008 ticks and never reached 0.35**. Then he jumped to Z 2001, fell to
1420, re-climbed and jammed at 1899.9 again - that loop is in the log twice.

Thresholds swapped so the top-out gets first refusal (0.10) and the lean-out becomes a fallback
(0.60). **Verified against the real collision at all three heights he actually jammed at** (1809,
1899.9, 1938): probe finds `nz +0.78`, sky is open, top-out fires, target is the roof deck at
**Z 2184** - a rise of +246 to +375uu.

**`blocked` WAS observable and I said it was not.** It is in the `GSDBG|LIP` line, not `GSDBG|CLIMB`.
Had I grepped for the field instead of trusting my summary of an older format, the race was visible
in the previous log.

**The risk I would watch next:** that rise is 246-375uu, and `AM_GS_ClimbTopOut` is a 1.13s montage.
If the warp cannot cover it the top-out will read as a snap or a teleport rather than a mantle. That
is the next thing to measure, and #075 already flagged the warp-target as unproven.

### Third play test: the inset is unfixable as a constant, and my 120 was a regression

The threshold swap worked - `blocked` reached 0.305 and passed 0.10 on 6 ticks, and the lean-out
never fired (offset stayed 70 all session). The top-out was still refused.

**Cause: he climbs the SOUTH face of Medium_11 (Y about 49433). Every measurement in this ticket was
taken on the NORTH face (Y about 50629), 1210uu away.** At his real position inset 120 returns
`nz 0.26`, which fails the 0.4695 gate. The cascade there needs 160+.

Measured across **56 roof lips on 14 houses**:

| inset | lips resolved |
|---|---|
| 80 (the original value) | **49 / 56 = 88%** |
| 120 (this ticket shipped) | **46 / 56 = 82%** |
| any single value | 88% is the ceiling |
| **search 80..360** | **54 / 56 = 96%** |

**My change from -80 to -120 was a net regression** - it helped one face of one house, the only one I
measured. Reverted to **-80** so a known-worse value is not left in place; that is NOT the fix and is
not worth a play test on its own, because neither 80 nor 120 resolves the wall he actually climbs.

**The real fix is to stop guessing the recession depth and search for it** - Bungie's step 5, which
I implemented as a constant. Implementation route found: `BlueprintTools.write_graph_dsl` supports
`(fn ...)` with `for`/`if`/`break`, so the cascade belongs in one function graph rather than 20 more
nodes in a 767-node EventGraph. Not built yet.

3 of 56 lips have no walkable deck at any inset - steep roofs, which is the separate roof-continuation
feature, not this bug.

### The fix: `FindClimbLedge`, a search instead of a constant

New Blueprint function `FindClimbLedge (WallNormal, FromLocation) -> (Found, Deck)`, 47 nodes,
authored with `BlueprintTools.write_graph_dsl`:

```
for i in 0..13:
    inset = 80 + i*20                       # 80..340
    deck  = SphereTrace(r25) down from FromLocation.Z+460 to +30, at inset inboard
    if deck.hit and deck.ImpactNormal.Z >= 0.4695        # walkable
       and not LineTrace(deck+1500 -> deck+15):          # open sky, not an interior floor
        return true, deck.ImpactPoint
return false, (0,0,0)
```

Wired into `ClimbStaminaExits`: exec is now head-trace -> `FindClimbLedge` -> accept branch;
`Found` drives `FCABE512.A`, `Deck` sets `ClimbLedgeLocation`. The old single-inset probe, the
0.4695 gate and the sky check I added earlier are disconnected from the condition (verified by
readback) but still execute - two wasted traces per tick, to be removed once this is proven.

EventGraph 768 nodes, compiles `UP_TO_DATE`, saved.

**Three tooling traps hit while building this, all worth recording:**

1. **A multi-output `bind` with too few names fails SILENTLY.** 8 names against 18 output pins
   produced a graph that compiled clean and was wrong - it hoisted a stray trace outside the loop
   and read `.z` of the wrong pin. With 19 names it errored properly. Only the exact arity works.
2. **`write_graph_dsl` APPENDS, it does not replace.** The second write left two complete function
   bodies, 93 nodes, one orphaned. Fix: `remove_function_graph` + `compile` (removal is deferred
   until compile) then re-add, or you get `FindClimbLedge_0` alongside the original.
3. **`read_graph_dsl` is lossy and cannot render a multi-output bind** - it printed the same
   misleading text for both the broken and the correct graph. I nearly rewrote a working function
   because of it. Node/connection readback is authoritative; the DSL round-trip is not.

Also: `create_node_by_key` with `FUNC Self::<fn>` returns an empty string and every subsequent
`connect_nodes` returns False. The working key is `FUNC BP_GSPlayerCharacter_C::<fn>`.

### Two claims I made earlier in the session that this ticket corrects

- **"The capsule only achieves 11 uu/s against 420 commanded."** Wrong. The debug print runs at
  **19 Hz, not 60** - I divided by the wrong dt. Mid-climb the log shows 21uu per sample = **410 uu/s**.
  The sustained climb is healthy and `ClimbSpeed 430` is being delivered.
- **"`ClimbBlockedSeconds` never exceeds 0.14."** Not observable - there is **no `blocked` field in
  the current log format** at all. That number came from an older format. The accumulator is wired
  correctly (`Abs(ActorZ - ClimbLastZ) < 2.0` AND input held, `ClimbLastZ` set immediately after),
  and the jam lasted 1.6s against a 0.35s threshold, so it should fire. Unverified either way.

## Refine

- **Measured the probe against the Blueprint's own geometry, not an idealised one.** The earlier
  sweeps in this session used my own start/end and got answers that did not describe the shipped
  code. Reading `5E1A0842`'s actual Start/End tree is what turned "the eave is a blocker" into
  "the probe is aiming 40uu short of the deck".
- **Read the sign convention instead of assuming it.** `-80` on a `vector * float` fed by
  `ClimbWallNormal` is 80uu *inboard*; guessing the other way would have moved the probe outward
  into fresh air and produced a confident, useless fix.
- **Set the gate to a number the character already owns.** 0.4695 is `WalkableFloorZ` derived from
  its own `WalkableFloorAngle 62`, not a tuned constant. A hand-picked 0.5 would drift the first
  time that angle changes.
- **Kept the change to the accept path.** The head-clearance guard, the stall bypass, the lean-out
  and the collision-ignore are all untouched, so if this fails the diff to reason about is three
  numbers and one trace.

**Deliberately left undone:** adding `bl=` (ClimbBlockedSeconds) and per-sample dZ to the `GSDBG|CLIMB`
print, which is the instrumentation that would have answered the second corrected claim above without
a play test - it is the first thing to do if this one fails; the plan's Stage C guard removal; rolling
`UseComplexAsSimple` and this probe tuning out to the other 42 houses, which will need the corpus
re-run with `bTraceComplex=False`.
