---
id: 338
title: GS.Corruption.Debug - the live on-screen bar the plan specified and stage 1 never built
agent: claude-acf
status: done
claimed: 2026-08-27T23:51Z
build: required
waiting_on: 
evaluated: 2026-08-27T23:53:47Z
observed: 2026-08-28T23:10:19Z | Michael watched the live overlay in the PIE window and it counted 4 soldiers dead - the bar and its per-term lines update as things happen, without typing Dump. This is the on-screen readout the plan specified; it could not be captured from my side because GEngine AddOnScreenDebugMessage is invisible to both CaptureViewport and HighResShot, so a human had to look.
scenario: PIE on L_Tutorial_Island with GS.Corruption.Debug 1 enabled, goblins fighting guards, watched directly in the PIE viewport by Michael
files: 
  - Source/GoblinSiege/World/GSCorruptionDebugCommands.cpp
---

## Goal

GS.Corruption.Debug - the live on-screen bar the plan specified and stage 1 never built

## Generate

`GS.Corruption.Debug 0|1` - a live on-screen bar and per-term readout, refreshed every frame.

```
CORRUPTION  [##########..............] 0.42  ->0.68  Burning
  objectives 0.45 x 0.31 = 0.14   (71 carriers, 4 types)
    Mill     x2   w1.00  avg 0.62  -> 40% of the term
  kills      0.20 x 0.27 = 0.05   (2 soldiers + 1 civilians x2.50 = 4.5 weighted, knee 12)
```

Colour tracks the stage (green Quiet -> straw Scarred -> ember Burning -> blood Mordor). Stable
message keys so each line replaces itself rather than stacking. ASCII bar characters only.

**A console COMMAND registering an `FTSTicker`, not a cvar.** A cvar needs something already ticking
to read it, and a statically-registered ticker depends on static-init order in a game module.
Registering on demand has neither problem, and the disabled cost is exactly zero rather than a
per-frame branch.

**It reuses `DescribeState()`** rather than reformatting the terms, so there is one definition of
what a term is. A prettier second copy of the same maths is a second thing to keep true.

Also corrected two stale claims in the file: the header and the `Release` command both still told the
reader that stage 1 wires no drivers, which stopped being true at #327.

## Evaluate

**NOT COMPILED.** Gate closed by five tickets, four of them other agents'.

**Why this existed as a debt at all.** It was in the original plan, skipped in stage 1 as the least
essential piece, and that judgement was wrong. Every verification of this feature since has been
"type `Dump`, read a snapshot, type `Dump` again, compare by eye" - and the things that have actually
been wrong were all about MOVEMENT: a term that climbs when it should not, a follower that lags, a
value that does not budge. **A snapshot is the one thing that cannot show any of those.** Two round
trips with Michael went on questions this would have answered on sight.

**Adversarially:**

- **The line filter is fragile.** `Line.Contains("x ") || Line.Contains("of the term")` is string
  matching against my own output format, so a future edit to `DescribeState`'s wording silently
  changes what the overlay shows. The alternative - a structured accessor - is the right fix and
  wants `GSCorruptionSubsystem.h`, which #337 holds.
- **The ticker keeps running when there is no subsystem**, deliberately: enabling the overlay before
  pressing Play would otherwise silently unregister and look broken. The cost is a world lookup per
  frame while enabled and nothing when disabled.
- **`FindGameWorld` takes the FIRST game world.** Fine for PIE-with-one-client; with two PIE clients
  it will pick one arbitrarily and never say which.
- **Nothing has run.** No compile, no eyes on it.

**Owes `AGENT_STATE.md`:** a FAILED line - *"a snapshot cannot show movement; build the live readout
before spending a session comparing dumps"*.

## Refine

**Changed from my own review:** the off path originally called
`GEngine->ClearOnScreenDebugMessages()`, which wipes **every** system's on-screen output, not just
this feature's. Turning off one overlay has no business blanking somebody else's debug draw, and our
lines expire on their own once the ticker stops. Removed, with the reasoning written at the site so
nobody helpfully adds it back.

**Deliberately left undone:**

- **A structured accessor to replace the string filter** - blocked on #337 holding the subsystem.
- **Stage 5 proper** (the MPC bridge) - also blocked on #337; and the GDD/`features.json` pairing is
  blocked on #335 holding `docs/`. This ticket exists because it was the one useful corruption file
  nobody was holding.

> 2026-08-28T01:00Z Adopted by claude-acf (was claude-corruption). Michael reassigned: this session has the 40 ACF skill packs listed, the previous one did not.
