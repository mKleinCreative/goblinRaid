---
id: 247
title: Archer stutter: hold a range band instead of chasing a sliding point, and restore combat focus the shot task clears
agent: claude-acf
status: done
claimed: 2026-08-21T22:33Z
build: done
waiting_on: Built, never watched. Needs Michael in a fight: does Erika hold still between shots, and does she face who she is shooting? Then GS.AI.LogLocomotion 8 to compare against the 61%/2.0 flick baseline.
evaluated: 2026-08-21T22:40:52Z
observed: 2026-08-22T03:55:52Z | Erika holds position between shots instead of sprint-stop-sprint: flick/s 2.0 to 0.2, rest 61 to 67 percent, verdict SQUARE to smooth
scenario: 8s GS.AI.LogLocomotion capture in a live fight, archer engaged
files: 
  - Source/GoblinSiege/AI/GSAISteeringComponent.cpp
---

## Goal

Stop the archer stutter that Michael has been watching as "Erika only takes a step forward with one
foot". Act on what `GS.AI.LogLocomotion` (#246) actually measured, not on the diagnosis that
preceded it.

## Generate

**The measurement first, because it changed the fix.** An 8s capture during a real fight
(`MyProject.log`, 22:18:27), archers only:

| pawn | %rest | maxspd | flick/s | prop/s | turn deg/s |
|---|---|---|---|---|---|
| BP_ErikaArcher_C_1 | 61% | **1023** | 2.0 | **2.0** | **22** |
| BP_ErikaArcher_C_2 | 24% | **1023** | 1.4 | **1.4** | 124 |

1023.48 is BP_ErikaArcher's literal `MaxWalkSpeed`. A turn rate of 22 deg/s rules out the
heading-jitter hypothesis for C_1 outright. The trace shows ~0.4s bursts to full sprint about once a
second, then a 4.6s standstill.

**Two changes, both C++:**

`AI/Tasks/BTService_AcquireTarget.h/.cpp` (claimed under #240, which is mine) - new `HoldBandInner`
(400) / `HoldBandOuter` (1000). A ranged agent whose range is inside the band now publishes **its own
location** into `TargetLocation`, so the MoveTo executes, reports `AlreadyAtGoal` and generates no
velocity. Outside the band it goes to `StandoffRadiusOverride` (700) - the MIDDLE of the band, which
is what creates the deadband: 300 units of drift must accumulate before anything moves again.

`AI/GSAISteeringComponent.cpp:189` - the focus guard now asks
`GetFocusActorForPriority(EAIFocusPriority::Gameplay)` instead of trusting its own `FocusedTarget`
cache. `UBTTask_RangedAttack::OnTaskFinished` clears that focus on every exit (correctly, and
silently), so the cache said "already focused" and the focus was never re-applied while the target
stayed the same. After her first shot the archer's yaw fell back to path following's move focus.

`Combat/GSAnimDebugCommands.cpp` - **fixed a defect in my own instrument.** Its summary declared
"THE BEHAVIOUR-TREE DIAGNOSIS IS WRONG" on a majority vote that let 7 horde goblins - different
skeleton, different AnimBP - outvote the 3 humans that were the subject. It now reports both counts
side by side and refutes only on a capture with ZERO square-wave pawns.

## Evaluate

**Verified: compiles.** Full rebuild, 05:43, Succeeded. **That is all that is verified.** No capture
has been taken since these changes and nobody has watched an archer move. Per this project's own
rule, a green build proves nothing about behaviour - `SetDefaultSubobjectClass` compiled, linked and
was silently refused at runtime here.

**Three things I got wrong before the measurement, recorded so they are not repeated:**

1. I predicted 80uu dashes at ~0.15s "never approaching walk speed". Reality: ~0.4s bursts to
   **1023 uu/s**, five times larger. My number came from the MoveTo's `AcceptableRadius`; the actual
   drift of a moving target is far bigger than the arrival tolerance.
2. I proposed AnimBP thresholds of 60/25 and asked Michael to author them. The simulation column
   shows `flick/s == prop/s` for **both** archers - the change would have done **nothing**, because a
   signal swinging 0..1023 clears any threshold in that range. Dropped on his ruling.
3. My instrument's own verdict line was wrong, as above.

**Left unresolved, and it is a real hole:** the run clip `A_HU_Std_RunF` is authored at 406.9 uu/s
but `Walk -> Run` is entered at 500, so anything in the Run state slides >=1.23x by construction, and
Erika at 1023 slides 2.51x. Michael ruled "drop her to ~400" - but at a 400 cap she could never enter
Run at all and would slide 2.4x on the *walk* clip (167.5 uu/s) instead, which is worse. That
contradiction is his to rule on and is deliberately NOT actioned here.

## Refine

Changed in response to my own evaluation: the instrument's aggregation, above - shipping a fix while
leaving the tool that misreported it in place would have poisoned the next capture too.

Deliberately undone: the speed and the `Walk -> Run` threshold, pending Michael's ruling on the
contradiction above; and any AnimBP edit at all, since the one I proposed is measured worthless.

Owes AGENT_STATE.md a DECISION line: *ranged agents hold a range BAND, not a point - the MoveTo they
feed snapshots its goal and ignores later writes, so the only way to stop it moving is to hand it a
goal it is already standing on.* And a FAILED line: *predicted the stutter's magnitude from an
arrival tolerance and was out by 5x; the AnimBP fix proposed off that prediction would have changed
nothing.*
