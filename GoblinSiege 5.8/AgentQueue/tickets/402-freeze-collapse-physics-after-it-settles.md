---
id: 402
title: Freeze collapse physics after it settles; calm the collapse and the market
agent: claude-settle
status: review
claimed: 2026-09-09T16:13Z
build: none
waiting_on:
evaluated: 2026-09-09T16:28:44Z
observed:
scenario:
files: 
  - Source/GoblinSiege/Destruction/GSCrumbleComponent.h
  - Source/GoblinSiege/Destruction/GSCrumbleComponent.cpp
  - Source/GoblinSiege/Destruction/GSMarketObjective.h
  - Source/GoblinSiege/Destruction/GSMarketObjective.cpp
---

## Goal

Michael, 2026-09-09, watching a full-map collapse in PIE:

> "the trees disappeared again, for one, there was way too much physics being rendered when the
> building collapsed, things were shooting out fairly quickly. It's still spawning too many smoke
> instances. the market looks too intense. Is there a way for us to cancel out the physics
> simulation after maybe 5-6 seconds of the collapse? just so we can simulate it settling"

## Generate

**The settle, which was the actual question.** `UGSCrumbleComponent` now stops a wreck simulating
once it has come to rest, keeping it exactly where it landed.

- `FreezePhysicsAfterSeconds` (10.0) - when to first ask "is it still?". 0 disables.
- `bFreezeEvenIfStillMoving` (false) - the default WAITS for stillness rather than freezing on the
  clock, re-checking every `FreezeRecheckSeconds` (1.0) against `SettledSpeedThreshold` (15 uu/s).
  Freezing a collapse that is genuinely still falling would leave masonry hanging in the sky, which
  is the exact failure `SweepStragglers` exists to clean up after and which Michael has already
  caught once.
- `FreezeHardDeadlineSeconds` (30.0) - a wreck that jitters forever still gets frozen, with a
  `Warning` naming its speed so the number is visible rather than silently applied.
- On freeze, `SweepTimer` is cleared: straining pieces that no longer have a physics proxy is
  pointless.

**`Config/DefaultEngine.ini` gained `[SystemSettings] p.Chaos.GC.DestroyProxyOnSetSimulatePhysicsFalse=1`,
and this is load-bearing.** Not in the original claim - noted rather than hidden.

## Evaluate

**Prototyped in live PIE before a line of C++ was written**, which is the only reason this ticket
is not shipping a no-op. Sequence, all measured after `GS.Raid.CompleteAllObjectives`:

| step | simulating | game thread | fps |
|---|---|---|---|
| after collapse | 366 of 366 (**44,762 pieces**) | 1381 ms | 0.7 |
| `SetSimulatePhysics(false)` on all 366 | **366 of 366** | 1073 ms | 0.9 |
| same call, cvar set to 1 first | **0 of 366** | **84 ms** | 11.8 |

The middle row is the finding. `UGeometryCollectionComponent::SetSimulatePhysics(false)` only tears
down the physics proxy when `p.Chaos.GC.DestroyProxyOnSetSimulatePhysicsFalse` is 1; the engine
default is **0**, "preserves legacy behavior of keeping the proxy alive" (its own help text). The
call returns cleanly either way. Had this been written straight to C++ and shipped, it would have
logged "settled and froze" on every building and changed nothing measurable - a feature that lies.
`FreezeSettledPhysics` therefore reads the cvar and refuses with a `Warning` rather than pretending.

Piece poses were sampled across the freeze: 474 pieces, 0 moved more than 5uu, worst delta 0.00uu.
**Caveat on that number:** it was taken on wrecks that had already stopped moving, so it proves the
freeze does not SNAP anything back to rest - it does not prove a mid-fall freeze looks acceptable.
That is what `bFreezeEvenIfStillMoving=false` exists to avoid, and it is untested.

**Not compiled, not run.** Same gate as #400.

**Physics was the dominant cost but not the only one:** 84 ms of game thread remains after freezing
everything, still ~12 FPS. Something else in that frame is expensive and this ticket does not
address it.

**Deliberately NOT changed, with reasons:**

- **"things were shooting out fairly quickly"** - that is `CollapseShoveMagnitude` (5,000,000, seven
  shoves per building). The C++ default for `CollapseShoveCount` is **0**, so the ring is off by
  default and the placed buildings carry their own values; changing the C++ default would not reach
  the level. This is a per-instance level edit and a look number - Michael's eye, not my guess. The
  knob and its current values are reported rather than silently halved.
- **"the market looks too intense"** - `AGSMarketObjective` has no FX tuning of its own; its
  intensity is that it adopts a cluster of stalls within `AutoAdoptRadius` (3000uu) and each one
  spawns its own fire and smolder. **#400's `MinSmolderSpacing` (1500uu) should already collapse
  most of that**, since the whole market sits inside one spacing radius. Adding a second, market-
  specific fix on top of an uncompiled one would be piling guess on guess. Build #400 first, look,
  then decide.
- **"the trees disappeared again"** - see below. Not addressed here, and the settle probably does
  NOT fix it.

## Refine

**The trees, honestly.** The log carries `[VSM] Non-Nanite Marking Job Queue overflow ... occurs
when many non-nanite meshes cover a large area of the shadow map`, three times, each immediately
after a mass collapse and never otherwise. Tens of thousands of loose non-Nanite debris pieces
casting shadows is a very plausible cause.

**But freezing physics will not fix it.** A frozen piece still renders and still casts a shadow -
the freeze removes simulation cost, not mesh count. Anyone reading the perf win above and expecting
the trees back is going to be disappointed, so it is written down here rather than left as an
implication. The real levers are reducing the loose piece count, making debris Nanite, or raising
the VSM non-Nanite marking budget - none attempted, all untested.

Left undone: the build, and therefore every behavioural claim in this ticket.

---

## Follow-up, 2026-09-09, after Michael watched the prototype

> "we need to have them go for a little longer. a lot of the pieces were still in the air. What I
> meant by market was the Inn building. but it seems like there's a lot less smoke now."

**The mid-air pieces were the prototype, not this code.** What he watched was the Python experiment
that measured the 1381ms -> 84ms win: it froze all 366 collections at once, unconditionally, while
they were still falling. `bFreezeEvenIfStillMoving` defaults to false precisely so the shipped
version cannot do that. Raised the timings anyway, because the instruction stands on its own:
`FreezePhysicsAfterSeconds` 6 -> 10, `FreezeHardDeadlineSeconds` 20 -> 30 (the deadline is the only
remaining path that can freeze a piece mid-air, so it gets the most room).

**"Market" was the Inn.** That redirects the intensity complaint onto the 440-piece kitbashed
building that #395 and #400 are both already about - so #400's `MinSmolderSpacing` is aimed at
exactly the right target, and no market-specific work is needed. `AGSMarketObjective` was claimed on
this ticket and is untouched.

**"A lot less smoke now" is NOT this work, and must not be recorded as verification of it.** The
game DLL is dated 2026-08-31; no source change from today is compiled. The only thing that actually
changed in what he watched is #399's regenerated `GC_MERGED_House_Medium_07` - which previously flung
pieces up to 2.7 km, each a potential smolder source. That is a plausible cause and it belongs to
#399, not here. #400's smoke spacing remains entirely unobserved.
