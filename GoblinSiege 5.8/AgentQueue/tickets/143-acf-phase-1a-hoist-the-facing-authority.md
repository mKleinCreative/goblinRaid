---
id: 143
title: ACF Phase 1a: hoist the facing authority and separation steer off AGSAIControllerBase into UGSAISteeringComponent
agent: claude-acf
status: done
claimed: 2026-08-13T04:25Z
build: required
waiting_on:
evaluated: 2026-08-13T04:47:56Z
observed: 2026-08-13T05:43:22Z | Michael watched a 10-goblin scrum on the arena garrison and reported the agents face what they are fighting perfectly, and that bodies hold apart to a respectable degree - so both the facing authority and the separation steer survived the move onto UGSAISteeringComponent
scenario: PIE in L_CombatArena, 10 horn-summoned goblins ringed on a thinned 3-man garrison with separation, personal space and facing all ON, watched live
files: 
  - Source/GoblinSiege/AI/GSAISteeringComponent.h
  - Source/GoblinSiege/AI/GSAISteeringComponent.cpp
  - Source/GoblinSiege/AI/GSAIControllerBase.h
  - Source/GoblinSiege/AI/GSAIControllerBase.cpp
---

## Goal

ACF Phase 1a: hoist the facing authority and separation steer off AGSAIControllerBase into UGSAISteeringComponent

## Generate

**New: `Source/GoblinSiege/AI/GSAISteeringComponent.h/.cpp`** - `UGSAISteeringComponent`, holding the
facing authority (#133) and the separation steer (#132) moved wholesale off `AGSAIControllerBase`.
Behaviour, tuning values, cvars and every comment explaining why each number is what it is came
across unchanged. `GS.Combat.FaceTarget` and `GS.Combat.Separation` are registered here now; they are
still found by name, so `GSDebugCommands.cpp`'s `GSSeparationIsOn()` (which reads them through
`IConsoleManager` rather than by symbol) needed no edit.

**Modified: `GSAIControllerBase.h/.cpp`** - creates the component unconditionally for every AI
controller; `IsFacingAuthorityEnabled()` kept as a forwarder so the three BT call sites
(`BTTask_MeleeAttack:124`, `BTTask_MenaceOrbit:174`, `BTTask_Block:212`) did not have to change;
`Tick` override deleted; `OnUnPossess` now calls `SteeringComponent->HandleUnPossess()` before Super,
while `GetPawn()` still answers.

**Two things deliberately designed in, not incidental:**

1. **The component ticks itself.** `PrimaryComponentTick`, not the owning actor's tick. This closes
   #135 permanently: both behaviours used to ride `AGSAIControllerBase::Tick`, and
   `AGSHordeAIController`'s constructor set `bCanEverTick = false`, so neither ever executed on a
   summoned goblin for the entire life of #132 and #133 - two tickets that both closed claiming the
   horde was covered. A `UActorComponent` registers its own tick function and does not consult the
   owner's flag, so no subclass constructor can do that again.
2. **The blackboard is resolved defensively** - `GetBlackboardComponent()` first, then
   `FindComponentByClass<UBlackboardComponent>()`. ACF groundwork: `AACFAIController` creates its own
   `UBlackboardComponent` named `"BlackBoardComp"` that `AAIController` knows nothing about, so after
   Phase 1 there may be two, and a facing authority reading the empty one would look exactly like the
   bug this component exists to fix.

Also recorded in `GSAIControllerBase.h` while in there: the `DoNotCreateDefaultSubobject` opt-out for
perception **does not work in 5.8**. The engine logs `Ignored DoNotCreateDefaultSubobject for
AIPerceptionComponent as it's marked as required` eleven times in one PIE session, so the horde is
carrying the sight sense GDD §3.4 exists to refuse. Intent left in place and documented rather than
silently removed.

## Evaluate

**Verified:** it builds. Editor-closed build 2026-08-12, `Result: Succeeded` in 05:05, zero errors,
only the two pre-existing `AbilityTags` C4996 warnings. `UGSAISteeringComponent` registers in the
editor. No code outside the moved files referenced any of the moved members - grepped for all twelve
symbol names before building; the only hits were the three BT call sites on the forwarder and a
handful of comments.

**NOT VERIFIED, and this is the honest headline: the moved behaviours have never been observed
running.** The whole risk in a move like this is that the code stops executing while still
compiling - exactly what #135 was - and I did not manage to prove it does.

The attempt, and why it failed, in order:

1. Spawned eight goblins and a militia patrol in `L_CombatArena`, ran `GS.Combat.CrowdStats` with
   `GS.Combat.Separation 1`: *53 pairs under 400uu, 0 interpenetrating, worst clearance 25.4uu*
   against a `SeparationMargin` of 25.0.
2. Set `GS.Combat.Separation 0`, waited, sampled again: **identical numbers, same worst pair.**
3. That should have been impossible if the steer were doing anything, so I measured positions rather
   than believing the sample. **Nearest goblin-to-human distance was 1802uu against an
   `AutoThreatRadius` of 1200** - the two groups never came within threat range, nobody engaged,
   nothing crowded, and both samples were of a static spawn grid. The A/B was inconclusive, not
   passing. Reporting sample A alone would have been a fabricated verification.
4. Teleported the player into the patrol to force contact. **The goblins did not move - at all.**
   Identical coordinates after the player relocated 2300uu.

**Which uncovered a regression I caused in #141-era work**, not in this ticket's C++:

```
LogBehaviorTree: Warning: BT_HordeGoblin has missing decorator node! (branch 0, 1, 2, 3, 4)
```

Repointing `BT_HordeGoblin`'s blackboard and rewriting its `FBlackboardKeySelector` fields from
Python invalidated every decorator on the root Selector. The tree could not run, so the horde stood
still. `BT_HordeGoblin.uasset` and `BB_HordeGoblin.uasset` restored from git; the broken pair kept at
`<scratchpad>/broken-bt-backup/`. #141's record has been corrected - it previously claimed that work
"should not be reverted".

**The lesson, for AGENT_STATE:** a behaviour tree's decorators are not safely editable by setting
`selected_key_name` through reflection and saving. Nothing errored, nothing returned false, and the
asset read back as if it had worked - the failure only appeared as a runtime `LogBehaviorTree`
warning and a crowd that would not move. BT/BB structure is editor work.

**Also unresolved, and now genuinely open rather than assumed:** whether the horde follows its
summoner at all. Michael reported that it does. With the committed assets restored, "Follow the
summoner" is a `MoveTo` whose blackboard key is `SelfActor` - a move to the agent's own position -
and `FollowTarget` is unresolved against `ACFAIBB`. Those two facts and his observation cannot all be
true, and I have not reconciled them. It needs one watched PIE run, walking away from the warband.

**Owed to AGENT_STATE:** the self-ticking-component pattern as the standing answer to #135; the BT
reflection-editing hazard above; the 5.8 `DoNotCreateDefaultSubobject` finding; and the open follow
question.

## Refine

**Changed on self-review:** the header edit was attempted as a surgical replace, failed to match on
whitespace, and was rewritten wholesale instead - so the file was re-read and re-verified rather than
patched blind. The moved-out section is replaced by a pointer to the component's header rather than a
summary of it, because a summary is how two descriptions of one behaviour drift apart.

**The verification was abandoned three times rather than reported optimistically**, and that is the
main thing this ticket did right: sample A alone looked like a pass, the A/B looked like a pass until
the numbers came back identical, and the identical numbers looked like a component fault until the
positions showed the two groups 1802uu apart. Each step would have produced a confident, wrong claim.

**Deliberately left undone:**

- **The behavioural verification.** It needs a real fight - two groups inside 1200uu of each other -
  and it needs the behaviour tree working, which it now is again only because the assets were
  reverted. This is the first thing to do next session, and it should be watched, not logged.
- **The BB/BT reparent** has to be redone properly in the editor. The reasoning is intact in #141;
  only the method was wrong.
- **The stale comment at `GSDebugCommands.cpp:627`** still says the separation cvar is a file-static
  in `AI/GSAIControllerBase.cpp`. It is in `AI/GSAISteeringComponent.cpp` now. One line, in a file
  this ticket did not claim, so it was not touched - noted here rather than edited around.
- **Phase 1 proper** (`AGSAIControllerBase : AACFAIController`) is untouched. This ticket only cleared
  the way for it.
