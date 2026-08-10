---
id: 074
title: "Climb: never top out while there is still wall above your head"
agent: claude-climbrebuild
status: done
claimed: 2026-08-07T19:32Z
build: none
waiting_on:
evaluated: 2026-08-07T20:05:34Z
files: 
  - Content/Blueprints/BP_GSPlayerCharacter.uasset
---

## Goal

Stop the climb interrupting itself. Michael's rule, in his words: *"continue to move forward and
upwards if there's a small variance in the surface plane"* - and from the annotated screenshot
`climbinb lip problem.png`, where he circled the wall ABOVE a timber band and said the game
"views this as something to climb over, so it tries the climbing up onto a roof animation instead
of continuing the climb up the wall."

## Generate

**Eight nodes.** In `ClimbStaminaExits`, between the ledge probe (`5E1A0842`) and the accept branch
(`3D8418C0`): a `LineTraceSingle` from `ActorLocation + (0,0,140)` along `GetActorForwardVector() * 150`,
`NOT`-ed and `AND`-ed into the branch's existing condition.

If anything is in front of your head, the wall continues, and **the top-out is not considered at all**.

Uses `GetActorForwardVector()` rather than `ClimbWallNormal`: while climbing, `ClimbTick` rotates the
actor with `MakeRotFromX(newNormal * -1)`, so forward IS the inward wall normal already. Three fewer
nodes and one less thing to keep in sync.

## Evaluate

**Measured across the regression corpus.** The top-out now fires only where the wall actually ends:

| | before this ticket | after |
|---|---|---|
| top-out fires at | Zrel **325-375** (mid-wall) | Zrel **725-775** |
| wall actually ends at | - | Zrel **650-750** |

Seven of eleven points now commit within ~25-125uu of the real top of the wall, having previously
committed less than halfway up it. Blueprint compiles `UpToDate` at 606 nodes.

**This is the whole of Michael's complaint, and it is one condition.** The five-stage cascade in the
approved plan was built to decide *which* surfaces are worth mantling onto. The actual question was
almost never worth asking: if there is wall above your head the answer is always "keep climbing", and
the ledge probe should not run. I spent most of a day on the harder question.

**NOT PLAYED.** Trace simulation only. Whether it feels right - and in particular whether 140uu is
the correct head height and 150uu the correct reach - needs Michael on the wall.

**Three points still never top out** (`Medium_01`, `Small_6`, `TreeLog_1`) and that is NOT this
ticket failing. Their walls end at a surface too steep to stand on - `Medium_11`, the house Michael
actually named, ends in a **75-79 degree pitch** (`nz 0.18-0.26`). There is nowhere to top out TO.
Getting up those needs the climb to continue onto the roof *as a climbable surface*, which is the
next piece of work and the real remaining half of his rule.

**Sequencing risk I accepted:** this ticket's check runs BEFORE the capsule-fit and rise-clamp added
in #070, so it short-circuits them. That is intended - it is cheaper and it is the more fundamental
question - but it means #070's two gates now fire far less often and are no longer the primary
defence. They stay because they still catch the case where the wall ends at something unstandable.

**Touched outside the goal:** none. One file, claimed.

**Owed AGENT_STATE.md** - DECISION (2026-08-07, Michael): the climb never tops out while there is
climbable surface above head height. Top-out is what happens when the wall RUNS OUT, not a thing to
be tried whenever a ledge is in reach.

## Refine

- **Took the rule as stated instead of engineering around it.** Michael gave a one-line spec and asked
  what he could do to keep it less complicated. The honest answer was that the complexity was mine,
  and the fix was eight nodes.
- **Put the check BEFORE the ledge probe's accept test, not inside it.** Cheapest possible ordering:
  one line trace short-circuits an entire sphere trace, capsule sweep, break, and two branches on
  every tick where the wall continues - which is nearly every tick of a climb.
- **Used the forward vector.** The obvious implementation re-reads `ClimbWallNormal` and negates it.
  The actor is already rotated to face the wall, so that work is done.
- **Did not remove #070's capsule-fit or rise clamp**, even though this check largely supersedes them.
  They are the backstop for a wall that ends at something you cannot stand on, and that case is real -
  it is 3 of 11 corpus points.

**Deliberately left undone:** continuing the climb onto a steep roof as a climbable surface, which is
what `SM_MERGED_House_Medium_11` actually needs; `ClimbTopOut`'s fixed 1.05s `Delay`; the stamina
migration (blocked - see below).

**A blocker I reported that turned out not to exist - recorded because the diagnosis was wrong in an
instructive way.** I queried `unreal.GSStaminaComponent` from Python and found `set_regen_suppressed`
/ `is_regen_suppressed` / `get_drain_rate` missing while `try_consume` and `set_drain_rate` were
present, and concluded that the build had been Live Coding and had not re-run UHT.

The build was fine. **The running editor had loaded its DLL before the build and was answering from
stale in-memory reflection.** Checked on disk afterwards: `GSStaminaComponent.gen.cpp` regenerated
2026-08-07T13:08 with 17 occurrences of `SetRegenSuppressed`, DLL 13:09.

**Lesson: reflection queried from a live editor describes the DLL that editor STARTED with, not the
one on disk.** To ask whether a build actually registered something, grep the `.gen.cpp` - it is
authoritative, it needs no editor, and it cannot be stale in the way a running process can.
