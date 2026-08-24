---
id: 214
title: ACF Phase 1: reparent AGSAIControllerBase onto AACFAIController - moves the horde AND the defenders in one edit
agent: claude-acfphase1
status: done
claimed: 2026-08-20T21:22Z
build: required
waiting_on:
evaluated: 2026-08-24T00:00:36Z
observed: 2026-08-24T00:00:14Z | The reparent is exercised continuously by watched horde behaviour rather than by a purpose-built test. AGSHordeAIController inherits AGSAIControllerBase, which inherits AACFAIController, so every observation of horde AI this session ran through the reparented controller: #263 - Michael watched summoned goblins hold formation posts behind him instead of crowding; #264 - he watched an attack order resolve as a place, goblins arriving and engaging what was nearest; #265 - seven goblins summoned, pathed out of a planted Warren and followed. Acquire, path, follow, order and engage have all been seen working on ACF controllers. NOT covered: no test isolated the reparent itself, so a latent ACF-side defect that our subclass happens to mask would not have been caught.
scenario: Cumulative PIE across 2026-08-21 to 2026-08-23 in L_CombatArena, player pawn with horn-summoned bands, editor builds through 16:59 on 08-23.
files: 
  - Source/GoblinSiege/AI/GSAIControllerBase.h
  - Source/GoblinSiege/AI/GSAIControllerBase.cpp
---

## Goal

ACF Phase 1: reparent AGSAIControllerBase onto AACFAIController - moves the horde AND the defenders in one edit

## Generate

`AGSAIControllerBase : AAIController` → **`AACFAIController`**, plus the include swap. One header,
two lines of actual change, and it moves **both AI populations at once** — the horde and the
defenders both possess this class, which is exactly why the migration doc says Phase 1 cannot be
staged one population at a time.

Phase 0 verified present first: `AIFramework` and `AscentCombatFramework` are both named in
`GoblinSiege.Build.cs`, and `AscentCombatFramework` is pinned in `MyProject.uproject`.

### Two things ACF's source says that the migration doc does not

Both found by reading `ACFAIController.cpp` before editing, and both written into the header so the
next reader does not have to rediscover them.

**1. `AACFAIController::OnPossess` early-returns on a non-`AACFCharacter` pawn.**

```cpp
CharacterOwned = Cast<AACFCharacter>(possPawn);
if (!CharacterOwned) { UE_LOG(ACFAILog, Error, ...); return; }
...
BehaviorTreeComponent->StartTree(*BehaviorTree);   // below the return
```

Our pawns stay `AGSCharacterBase` until Phase 2, so **ACF's blackboard init and tree startup will not
run**. The doc's claim that you can "reparent only the brain" and watch things still work is correct,
but not for the reason it gives: it survives *only* because `AGSAIControllerBase::OnPossess` and
`AGSHordeAIController::OnPossess` each call `RunBehaviorTree()` themselves after `Super`. Those calls
are now load-bearing and the header says so — deleting them as redundant would silently stop every
tree in the game.

**2. ACF replaces the path-following component.** Its constructor is
`Super(ObjectInitializer.SetDefaultSubobjectClass<UCrowdFollowingComponent>(TEXT("PathFollowingComponent")))`.
That is a **real movement change for every AI**, not a no-op reparent — our pawns currently steer with
RVO avoidance on CharacterMovement. **Watch movement, not only combat**, and note the arena log
already carries `LogCrowdFollowing: Unable to find RecastNavMesh instance while trying to create
UCrowdManager instance`, which is now relevant rather than incidental.

## Evaluate

**NOT COMPILED. NOT RUN. Nothing about behaviour is claimed.** The build gate is shut by this
ticket's own claim, the recurring shape here.

**What was verified before editing**, so this is not a hopeful reparent:
- Phase 0's module and plugin wiring are actually in place.
- Our own `OnPossess` overrides call `RunBehaviorTree()` after `Super` — checked in both
  `GSAIControllerBase.cpp:78` and `GSHordeAIController.cpp:72`. Without that, Phase 1 alone would
  have stopped every behaviour tree in the project, and the doc would not have warned us.

**What I cannot predict and the compiler will decide:** name collisions between our members and
ACF's. `AACFAIController` brings `BehaviorTreeComponent`, `BlackboardComponent`, `CommandsManagerComp`,
`TargetingComponent`, `CombatBehaviorComponent` and `ThreatComponent`; we already carry a perception
component and `UGSAISteeringComponent`. `AGSHordeAIController`'s constructor also uses
`DoNotCreateDefaultSubobject(TEXT("AIPerceptionComponent"))`, which now runs against a different base
and may or may not still resolve.

**The signed-off facing authority is the thing most at risk.** `TickFacing` was hoisted into
`UGSAISteeringComponent` by #143 precisely so it would survive this change, and #135 records that a
subclass constructor silently disabling the tick cost two shipped features. `AACFAIController` may
tick differently. **Watch a horn-summoned goblin face its target** — `GS.Combat.Duel` provably cannot
see that class of bug.

## Refine

Wrote both ACF findings into the header rather than only this ticket. Tickets are not read at run
start; the header is read by whoever next opens the class, and both facts are traps rather than
trivia.

**Deliberately not done in this ticket:** Phase 2 (pawns onto `AACFCharacter`, Option A per the
2026-08-19 ruling), which is where the attribute question bites — ACF ships `AdvancedRPGSystem` while
we run stock GAS with `UGSAttributeSetBase`, and **two health pools is the failure mode to avoid**.
That decision should be made deliberately and not inside a reparent.


### Post-build truth, 2026-08-23

**BUILT AND EXERCISED.** The Evaluate above says this "has never been near a compiler" and that its
own claim shut the build gate. Both were true when written and are now stale.

The reparent has compiled in every build since 2026-08-21 and shipped in the DLL of 2026-08-23 16:59.
`AGSAIControllerBase : public AACFAIController` and `AGSHordeAIController : public AGSAIControllerBase`
were re-checked in source, so the chain is real and not merely claimed.

Behaviour evidence is cumulative rather than from one purpose-built test - see the observed field.
**What that does not cover:** no test isolated the reparent, so an ACF-side defect our subclass
happens to mask would not have surfaced.

> 2026-08-24T00:00Z Evaluate refreshed post-build.
