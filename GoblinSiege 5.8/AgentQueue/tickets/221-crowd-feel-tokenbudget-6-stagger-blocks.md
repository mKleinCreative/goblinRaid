---
id: 221
title: Crowd feel: TokenBudget 6, stagger blocks only its causer, ordered overflow spills to another enemy
agent: claude-acf
status: done
claimed: 2026-08-21T00:38Z
build: none
waiting_on:
evaluated: 2026-08-21T00:41:00Z
observed: 2026-08-21T00:50:31Z | All three changes confirmed across 47 CrowdStats samples. SPILL-OVER: zero OVER ENGAGED rows, max engaged exactly 6 = cap, and up to 9 victims engaged simultaneously across 12 distinct victims - where the pre-change run had one victim carrying 10 goblins. TRICKLE: swinging on gangs of 4+ now spans 0-6 with 15 of 21 rows taking a middling value and a mode of 5; before the change it was strictly bimodal, only 0/1 or 4/4. PER-ATTACKER VETO: 17 of 22 (immune) rows still show swinging greater than zero - previously immune meant the whole gang stopped. No defender died; 6 goblins did, so the melting risk did not materialise.
scenario: GS.Horde.SpawnTest, GS.Combat.LogAI 1, a single GS.Horde.Order Attack on BP_CastleGuard02_C_1, then CrowdStats sampled 9 times over the following ~25 seconds with no further orders. L_CombatArena, 2026-08-21 00:49.
files: 
  - Source/GoblinSiege/Combat/GSEngagementComponent.h
  - Source/GoblinSiege/Combat/GSEngagementComponent.cpp
  - Source/GoblinSiege/Characters/GSCharacterBase.h
  - Source/GoblinSiege/Characters/GSCharacterBase.cpp
  - Source/GoblinSiege/Weapons/Abilities/GSGA_SwordLight.cpp
  - Source/GoblinSiege/Horde/GSHordeSubsystem.cpp
---

## Goal

Crowd feel: TokenBudget 6, stagger blocks only its causer, ordered overflow spills to another enemy

## Generate

Three changes, all Michael's calls on 2026-08-21, all taken against measurement rather than taste.

**1. `TokenBudget` 4 -> 6.** With 9-10 goblins on one guard, 4 was the hard ceiling on simultaneous
swings; six of the gang were idle at the best moment the fight could produce.

**2. The flinch/recoil veto now refuses only the attacker that caused it.** New
`CanBeAttackedBy(Requester, bRecoilCountsAsOpening)` - death and a broken guard still veto everyone;
`State.HitReact` / `State.Recoil` refuse only `FlinchCauser`. `TryAcquireToken` uses it instead of the
blanket `CanBeAttacked()`. Authorship is recorded by `NoteFlinchCausedBy`, called from
`PlayHitReact`, which gained an optional `AActor* Instigator`. All three call sites now name one:
the damage path passes `Attacker`, the blocked-swing flinch passes `Blocker`, the sword passes its
avatar.

**3. An Attack order past capacity SPILLS to the nearest other enemy with room.**
`GetAssignedTargetFor` used to return the ordered victim unconditionally; it now returns it only when
`HasEngagementRoom(Goblin)` passes, and otherwise falls through to the ambient path directly below -
which already skips saturated candidates and takes the nearest with room. That path was written in
#132 and has been sitting unused by ordered goblins the whole time.

## Evaluate

**NOT COMPILED, NOT RUN at time of writing.**

**The evidence for (1) and (2)**, from 16 CrowdStats samples taken during a real fight on
2026-08-21 00:34-00:35: on gangs of 6+, `swinging` was **bimodal** - either `4` with `weight 4/4`
saturated (3 samples) or `0-1` (3 samples), never between. The mob ran full-throttle, stalled, ran
full-throttle. (1) raises the throttle; (2) removes the stall.

**THE COST OF (2), STATED PLAINLY.** The blanket veto was not an accident - piling onto a flinching
target is the deletion `UGSEngagementComponent` exists to prevent, and #087's recoil design assumes
it. With the veto per-attacker AND the budget at 6, a flinching defender can now be hit by six
goblins at once. **If defenders start melting, this ticket is why**, and the honest response is to
tune `TokenBudget` down rather than quietly restore the blanket veto, which would re-introduce the
stall this was built to remove.

**(3) REFINES ruling 34, it does not reverse it.** The order still converges the warband on the named
victim; nothing caps the first six. Only the surplus spills. An agent already registered passes
`HasEngagementRoom` (`GSEngagementComponent.cpp:401`), so incumbents are never displaced and there is
no target rotation on every scan.

**Consequence for #218 worth predicting before the watch:** with spill-over live, `OVER ENGAGED`
should become rare and the outer ring should sit mostly empty - it degrades to what it should always
have been, the fallback for when every nearby enemy is full. If `OVER ENGAGED` still appears
constantly, (3) did not take.

**Unknown-causer policy:** a flinch with no named instigator refuses nobody. Falls, fire volumes and
anything else that never claimed authorship will not gate the melee. The alternative - unknown blocks
everyone - would have quietly preserved the stall on any damage path that forgot to name itself.

## Refine

Considered and rejected: making `CanBeAttackedBy` fall back to blanket refusal when `FlinchCauser` is
stale rather than null. The tags own the lifetime already - `State.HitReact` and `State.Recoil` expire
on their own timers - so a stale causer cannot outlive the window it gates, and adding a second
expiry would be two clocks disagreeing about one event.
