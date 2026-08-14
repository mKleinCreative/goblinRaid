---
id: 135
title: Summoned goblins never face or separate: AGSHordeAIController disables the tick both #132 and #133 rely on
agent: claude-hordetick
status: done
claimed: 2026-08-12T02:39Z
build: required
waiting_on:
evaluated: 2026-08-12T02:44:50Z
files: 
  - GoblinSiege 5.8/Source/GoblinSiege/Horde/GSHordeAIController.cpp
  - GoblinSiege 5.8/Source/GoblinSiege/Horde/GSHordeAIController.h
observed: 2026-08-12T04:55:22Z | every horn-summoned goblin reported rotation mode DesiredRot and a live blackboard target; that flag is set only by AGSAIControllerBase::TickFacing, which cannot run unless the controller ticks
scenario: PIE on L_CombatArena, 8 goblins via GS.Horde.SpawnTest against a militia patrol, read out with GS.Anim.Snapshot 0
---

## Goal

Summoned goblins never face or separate: AGSHordeAIController disables the tick both #132 and #133 rely on

Michael, 2026-08-11, after #133 shipped and the goblin locomotion was signed off: *"the goblins still
walk sideways towards an enemy instead of facing the proper direction."*

## Generate

### One line, two shipped features, never executed

`AGSHordeAIController`'s constructor contained `PrimaryActorTick.bCanEverTick = false;`.
`AGSAIControllerBase`'s constructor sets it **true**. The subclass runs second, so every summoned
goblin has been silently opted out of everything the base class does per frame:

| ticket | feature | believed | actual on a horde goblin |
|---|---|---|---|
| #132 | separation steer (`TickSeparation`) | *"it covers the horde too (AGSHordeAIController derives from this)"* - its Evaluate | **never ran** |
| #133 | facing authority (`TickFacing`) | applies to every AI combatant | **never ran** |

Changed to `true`, with the reasoning written into both the `.cpp` and the `.h` so the next agent
does not undo it.

### Why nothing caught this

Both features work correctly on **defenders**, which possess `AGSAIControllerBase` directly through
`AGSEnemyCharacter`. `GS.Combat.Duel` - the command every crowd ticket has used to test - re-badges
human defender BPs onto `DA_Race_Goblin`, so the "goblins" in a duel are defenders wearing a goblin
race tag and they tick normally. **The only agents affected are ones summoned by the horn**, and the
failure is a silent absence rather than an error: no log, no warning, no compile complaint, just a
feature that is not there.

That is also why #132 could measure a real improvement and still be half wrong.

### The design objection, honoured rather than overruled

`AGSHordeAIController.h` says: *"The blackboard refresh is a timer, not Tick. Ten goblins ticking to
ask the same subsystem the same question is the cost this design exists to avoid"*, and GDD §3.4
requires a perception-less horde. #132 quoted that objection and wrote `TickSeparation` specifically
to clear it - broadphase overlap at 4Hz, per-frame work a loop over at most four cached pointers
behind three early-outs. `TickFacing` is cheaper still: a blackboard read and two bools.

The rule that actually matters is **no per-agent SEARCH on the frame**, not "no tick". The header
comment has been rewritten to say that, because read literally it caused this bug: it reads as a
blanket ban, and the constructor implemented it as one.

The perception component is still declined - that line is untouched and is what §3.4 is really about.

## Evaluate

**BUILD SUCCEEDED in 00:39**, editor closed, `-IgnoreQueue` on Michael's explicit instruction (both
open tickets were mine and no other agent was working). No new warnings - the two C4996
`AbilityTags` deprecations in `GSGA_Block.cpp` and `GSGA_Interact.cpp` are pre-existing.

**NOT WATCHED.** The change is one boolean and two comment blocks; the risk is not in its correctness
but in its consequences, which are exactly what the original comment warned about.

**How to check it, in one PIE session on `L_CombatArena`:**

```
GS.Horde.SpawnTest 6        # or blow the horn (MMB) next to a patrol
GS.Combat.LogAI 1
GS.Combat.FaceTarget 0      # then 1 - summoned goblins should now respond to this at all
GS.Combat.CrowdWatch 20 4   # the 'cross' bucket is #132's separation, now live on the horde
```

The tell for THIS ticket specifically: before it, those two cvars had **no observable effect
whatsoever** on a horn-summoned goblin. If toggling them changes nothing on the horde, the tick is
still not reaching them and the fix is wrong.

**Verified:** the constructor line existed and was `false` (read from source, and it is the only
`bCanEverTick` in `Horde/`); `BB_HordeGoblin` does carry a `TargetActor` key, so `TickFacing` will
find a target once it runs - checked first, because a missing key would have been a second bug
hiding behind this one and fixing only the tick would have looked like a failed fix.

**Unverified and worth stating plainly:** that this is the *whole* reason the goblins walk sideways.
It is a proven prerequisite - the code could not possibly have run - but whether facing now reads
correctly on a summoned warband is a question for a PIE session, and #133's facing authority has
still never been watched on ANY pawn.

**The performance risk is now real rather than theoretical.** Ten summoned goblins now tick where
none did. The mitigations are designed in and independently switchable (`GS.Combat.Separation 0`,
`GS.Combat.FaceTarget 0`), and the week-3 exit test is *ten goblins at frame rate*, so this is the
change most likely to show up there.

**Owed to `AGENT_STATE.md`:** a FAILED entry. Not for the boolean, but for the pattern - *a subclass
constructor can silently disable a base-class feature, and two tickets shipped believing they covered
the horde when neither had ever executed on one. Anything added to `AGSAIControllerBase::Tick` must
be tested on a HORN-SUMMONED goblin, because `GS.Combat.Duel` spawns defenders and cannot see this
class of bug.*

## Refine

**Changed in response to my own review:** the header comment, which I had not intended to touch.
Fixing the constructor while leaving the paragraph that justified it would have left the next agent a
documented argument for reverting the fix - and #132 had already cited that paragraph as a design
constraint. The comment now distinguishes the real rule (no per-frame search) from the incidental one
(no tick).

**Deliberately left undone:** the tick is enabled wholesale rather than gated to "only while this
goblin has a target". That would be cheaper still, but it would make the two cvars stop being a clean
A/B, and there is no measurement yet showing the cost is worth optimising. Measure ten goblins first.
