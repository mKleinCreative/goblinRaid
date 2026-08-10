---
id: 089
title: Summoned goblins never engage: the proximity threat scan registers nothing
agent: claude-hordefix
status: done
claimed: 2026-08-09T00:51Z
build: required
waiting_on:
evaluated: 2026-08-09T01:05:50Z
files: 
  - Source/GoblinSiege/Horde/GSHordeSubsystem.cpp
  - Source/GoblinSiege/Horde/GSHordeSubsystem.h
---

## Goal

Summoned goblins never engage: the proximity threat scan registers nothing

## Generate

**The title of this ticket was wrong, and that is the most useful thing in it.** The threat scan was
never broken. The bug was a facing deadlock I introduced in #083, and it stopped ANY AI attacking
from a stand-off slot - the horde was simply where it showed up first.

**The fix - `AI/Tasks/BTTask_MeleeAttack.cpp`.** `MaxFacingSnapDegrees` (#083) made a swing *refuse*
when the target was more than 120 degrees off, so that flanking would work and an attack could be
made to whiff. That created a state nothing could leave:

1. Agent arrives at its stand-off slot. `MoveTo` completes; velocity drops to zero.
2. `BTTask_MeleeAttack` fails the facing test and returns Failed.
3. The Selector falls to the chase branch. `MoveTo` reports success *instantly* - it is already
   there - so the tree restarts.
4. Nothing has rotated the pawn. `bOrientRotationToMovement` needs velocity, and there is none.
5. Back to 2, forever.

Observed as a goblin standing 130uu from a knight, velocity 0, never swinging.

Now the correction is **clamped rather than refused**: turn up to `MaxFacingSnapDegrees` toward the
target, then fail this tick if still not facing. Everything the cap was for survives - flanking still
costs the attacker real time, a swing still cannot be whipped round 180 degrees - but a
badly-facing agent provably comes round within a tick or two.

**Diagnostics kept** (`Horde/GSHordeSubsystem.cpp`): the threat scan now reports
`N anchor(s), N candidate(s) -> N registered, N wrong race, N out of range` under
`GS.Combat.LogAI`, and `OnWorldBeginPlay` says whether the scan armed at all. "The allies do not
attack" had four indistinguishable causes from outside; this separates them in one line. Also a
`turning to face X (N deg to go)` line on the new path.

## Evaluate

**FIXED AND PIE-VERIFIED.** `[GS.Damage] BP_HordeGoblin_C_2 -> BP_KnightDPelegrini_C_1 ... = 19.0`,
and `BP_HordeGoblin_C_7` likewise - summoned allies attacking, in a fight the player was also in.
`turning to face` fired 4 times, so the new recovery path is exercised rather than merely present.

**Everything I suspected first was wrong, and the diagnostics are what settled it.** Recorded because
the wrong hypotheses were all plausible and each cost a build:
- *"The proximity scan never fires."* It fires: `5 anchor(s), 10 candidate(s) -> 1 registered,
  9 wrong race, 0 out of range`.
- *"There are two subsystem instances."* There are not - logging `this` and the world pointer in
  both the scan and the query printed identical values.
- *"`PruneStaleThreats` eats scan-registered threats because I pass a null Provoker."* It does not
  look at Provoker at all: `1 threat(s) before prune, 1 after`.
- *"The service does not tick in `BT_HordeGoblin`."* It ticks and it sees the target:
  `service tick: select=0 key='TargetActor' target=BP_KnightDPelegrini_C_0`.

The decisive measurement was writing `TargetActor` onto a goblin's blackboard by hand and watching
the controller overwrite it back to None - that proved `RefreshStimulus` was alive and the fault lay
past it, which is what finally pointed at the tree rather than the stimulus.

**Scope of the regression is wider than the horde.** Human defenders had the same deadlock; they hid
it because they are usually still moving when they swing, so `bOrientRotationToMovement` kept
re-facing them. Anything that stopped on its slot could stick. This was live for the whole session
and is in every build from #083 onward.

**Not verified:** whether the clamp changes how defenders fight the PLAYER when flanked - it should
only make them recover rather than freeze, but no human has tested it. And `BP_GS_TargetDummy` still
has no `RaceData`, so it is correctly invisible to the horde's threat scan: **allies will never
attack the practice dummies.** Left alone rather than changed unasked - it is a one-property fix if
the dummies should be sparring partners.

**Owed to `AGENT_STATE.md`:** a FAILED line. An AI that can refuse to act on a condition it cannot
itself change will deadlock; the refusal has to come with the means of recovery.

## Refine

**Removed both temporary diagnostics** once they had done their job - the per-tick
`service tick:` line in `BTService_AcquireTarget` and the instance/world pointers in the horde
subsystem. They were for one bug and would have been noise forever. The permanently useful ones
stayed: the scan tally and the arm/disable line, both of which answer questions that will recur.

**Kept the fix as a clamp rather than a turn-rate interpolation.** Turning at
`GetTurnRateRadPerSec` like `UBTTask_Block` does would be more physically consistent, but
`ExecuteTask` has no DeltaTime and threading one in for this would be more machinery than the
problem deserves. The clamp costs a tick or two of turning, which is the same felt result.

**Deliberately left undone:** the target-dummy race question above, and any re-tuning of
`MaxFacingSnapDegrees` (120) - it now behaves as intended and should be judged by watching, not by
me picking a second number.
