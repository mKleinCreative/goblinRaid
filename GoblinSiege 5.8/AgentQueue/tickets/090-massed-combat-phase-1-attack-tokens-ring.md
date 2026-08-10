---
id: 090
title: Massed combat phase 1: attack tokens, ring-slot reservation, engagement capacity, menace orbit
agent: claude-crowd
status: done
claimed: 2026-08-09T01:35Z
build: required
waiting_on:
evaluated: 2026-08-09T01:51:50Z
files: 
  - Source/GoblinSiege/Combat/GSEngagementComponent.h
  - Source/GoblinSiege/Combat/GSEngagementComponent.cpp
  - Source/GoblinSiege/AI/Tasks/BTDecorator_HasAttackToken.h
  - Source/GoblinSiege/AI/Tasks/BTDecorator_HasAttackToken.cpp
  - Source/GoblinSiege/AI/Tasks/BTTask_MenaceOrbit.h
  - Source/GoblinSiege/AI/Tasks/BTTask_MenaceOrbit.cpp
  - Source/GoblinSiege/Characters/GSCharacterBase.h
  - Source/GoblinSiege/Characters/GSCharacterBase.cpp
  - Source/GoblinSiege/AI/Tasks/BTTask_MeleeAttack.h
  - Source/GoblinSiege/AI/Tasks/BTTask_MeleeAttack.cpp
  - Source/GoblinSiege/AI/Tasks/BTService_AcquireTarget.h
  - Source/GoblinSiege/AI/Tasks/BTService_AcquireTarget.cpp
  - Source/GoblinSiege/Horde/GSHordeSubsystem.h
  - Source/GoblinSiege/Horde/GSHordeSubsystem.cpp
  - Source/GoblinSiege/Combat/GSDamageExecCalculation.cpp
  - Source/GoblinSiege/Combat/GSDebugCommands.cpp
  - Source/GoblinSiege/Weapons/Abilities/GSGA_SwordLight.cpp
---

## Goal

Massed combat phase 1: attack tokens, ring-slot reservation, engagement capacity, menace orbit

## Generate

Phase 1 of the massed-combat plan: rationing who may swing, who may approach, and where they stand.

**`Combat/GSEngagementComponent.{h,cpp}` (new UCLASS)** - everything about attacking THIS actor.
Created on `AGSCharacterBase`, so the player is rationed by the same rules as everyone else.
- **Weighted token budget** (Kingdoms of Amalur, not a simple count): budget 2, light costs 1, so
  two jabs or one heavy but never both. Caps incoming DPS by construction.
- **`CanBeAttacked()`** - false while the owner holds `State.Dead`, `State.HitReact`,
  `State.GuardBroken` or `State.Recoil`. One check applied to every attacker at once; this is DOOM's
  glory-kill rule and it is what stops a stumbling defender being deleted by four simultaneous
  sweeps. `State.Recoil` is ours from #087 - a fighter already opened up by a blocked swing should
  not also be piled on.
- **Preemption by camera relevance** (DOOM): an on-screen attacker may steal an *unlocked* grant
  from an off-screen one. Locked grants (committed frames) are untouchable.
- **Engagement capacity** - a wider gate than the token budget: how many may be assigned at all.
  3 by default; the extras are the menace ring, not a queue.
- **Reserved ring slots** - 8 angles at radius 180, exclusive claim, preferring the slot nearest the
  claimant's current bearing so nobody crosses the pack. Replaces the *computed* lattice from #083,
  which spread agents out but could not stop two on a similar bearing picking the same angle.

**`AI/Tasks/BTDecorator_HasAttackToken.{h,cpp}` (new UCLASS)** - gates the melee branch. Acquires in
the condition, **releases in `OnCeaseRelevant`** so every abort path gives the token back.

**`AI/Tasks/BTTask_MenaceOrbit.{h,cpp}` (new UCLASS)** - what an agent does with no token: hold a
300uu ring, strafe, step-in feint every 1.5-2.5s. Rationing attacks without this makes the fight
*worse* than not rationing them - two swinging and eight in a rest pose reads as broken AI.

**Wiring.** `GetAssignedTargetFor` and `BTService_AcquireTarget` both consult `HasEngagementRoom`
and release the ledger on target change or drop; `BTTask_MeleeAttack` re-checks `CanBeAttacked`
before swinging; `UGSGA_SwordLight` locks/unlocks the grant around the committed frames;
`GS.Combat.NPCvNPCScalar` (0.55) applies when neither party is player-controlled;
`GS.Combat.CrowdStats` reports per-victim engaged/swinging/weight/slots.

Both trees: token decorator on the melee child, menace orbit inserted **above** the chase.

## Evaluate

**VERIFIED AT THE EXIT-TEST SIZE.** 10 summoned goblins vs 6 knights on `L_CombatArena`:

```
[GS.Crowd] BP_HordeGoblin_C_8   engaged 3  swinging 2  weight 2/2  slots 3
[GS.Crowd] GS.Combat.CrowdStats: 7 victim(s) engaged, biggest gang 3 on BP_HordeGoblin_C_8
```

- **Zero `OVER BUDGET` breaches across the entire session.** The weight cap was never exceeded once.
- **Biggest gang exactly 3**, matching `MaxEngagedAttackers`. No conga line.
- **`engaged 3 / swinging 2`** is the whole design in one line: two have permission, the third is
  orbiting rather than standing still.
- **Slot claims equal attacker counts** - no two agents on one slot.
- **The damage channel works and is correctly asymmetric**: goblin-vs-knight resolves `= 13.8`
  (25 x 0.55) while knight-vs-player stays `= 25.0`.

**Two bugs of mine, both found by running it rather than reading it:**
1. **The decorator deadlocked.** It only *reported* on a token it expected `OnBecomeRelevant` to
   have acquired - but the condition gates ENTRY to the branch, so false meant the branch was never
   entered, so `OnBecomeRelevant` never ran, so the token was never acquired. A 10v10 in which
   nobody threw a punch. Acquire moved into the condition.
2. **The orbit was unreachable.** Placed after `MoveTo`, it never ran: a Selector restarts when a
   child succeeds, and `MoveTo` succeeds *instantly* once the agent is already at its slot, so the
   tree busy-looped. Same shape as the #089 facing deadlock. Fixed by putting the orbit above the
   chase, failing it beyond `MaxOrbitDistance` (it steers, it does not path), and ending it every
   0.6s so the Selector can notice a token has freed up.

**NOT verified / honest gaps:**
- **Balance is badly off: goblins lost 9-0.** 40 HP against 75/armor-6 knights, and knights block at
  0.55 with a guard break while goblins block at 0.35 with none. That is archetype tuning, not a
  systems fault - and I pitted the horde against the ELITE rather than the militia it is meant to
  swarm. Nobody should read this as "the horde is broken".
- **Preemption has never been observed firing.** The code path is written and compiled but no log
  confirms an on-screen attacker stealing from an off-screen one.
- **TTK, blocked-per-kill and the 45s ceiling were not measured** at this size - the fight was
  one-sided enough that the numbers would not mean anything.
- Nothing ran on `L_Tutorial_Island`.

**Owed to `AGENT_STATE.md`:** a DECISION line (permission and position are rationed by the victim,
and the player is rationed by the same component), and a FAILED line about the Selector-restart trap
now that it has bitten twice.

## Refine

**Folded ring slots into the token component** rather than the two components the plan named. They
share an owner, a lifetime and *every* release condition - the same three events (died, switched
target, staggered) must return both - so `ReleaseAll` being one call that cannot half-happen is
worth more than the separation, and on this machine a second UCLASS costs another editor-closed
build. The two concerns stay separate in the API.

**Added a dead-but-not-destroyed sweep.** Weak pointers are not enough: `CorpseLifespan` defaults to
0 ("never destroy"), so a dead goblin's actor stays valid indefinitely and would hold its grant for
the full three-second watchdog - three seconds of a victim's budget spent on someone lying on the
floor.

**Deliberately left undone:** archetype balance (the 9-0 above), per-archetype token budgets via
`ConfigureFromArchetype` (written, wired to nothing yet - every combatant currently uses the default
2/3), and all of Phase 2 (orders, morale, the fodder/elite split). AI LOD stays out per Michael's
ruling that performance is not a felt problem.
