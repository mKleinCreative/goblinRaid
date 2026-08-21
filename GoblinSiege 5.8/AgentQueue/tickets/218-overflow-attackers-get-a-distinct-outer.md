---
id: 218
title: Overflow attackers get a distinct outer-ring bearing instead of stacking
agent: claude-acf
status: done
claimed: 2026-08-20T23:47Z
build: none
waiting_on:
evaluated: 2026-08-20T23:51:44Z
observed: 2026-08-21T00:35:57Z | Overflow goblins now hold exclusive outer-ring places instead of stacking on their own bearing: 5 of 16 CrowdStats samples show real overflow, up to 4 outer slots claimed at once (engaged 10 = 6 inner + 4 outer).
scenario: GS.Horde.SpawnTest, GS.Combat.LogAI 1, GS.Horde.Order Attack on BP_CastleGuard02_C_0, CrowdStats sampled repeatedly DURING the melee. L_CombatArena, 2026-08-21 00:34-00:35.
files: 
  - Source/GoblinSiege/AI/Tasks/BTService_AcquireTarget.cpp
  - Source/GoblinSiege/AI/Tasks/BTService_AcquireTarget.h
  - Source/GoblinSiege/Combat/GSEngagementComponent.cpp
  - Source/GoblinSiege/Combat/GSEngagementComponent.h
---

## Goal

Overflow attackers get a distinct outer-ring bearing instead of stacking

## Generate

An **exclusive outer ring** on `UGSEngagementComponent`, and the overflow path now claims a place on
it instead of aiming at its own bearing.

- `ClaimOuterSlot` / `ReleaseOuterSlot` / `GetOuterSlotLocation` / `GetClaimedOuterCount`, backed by
  `OuterRingSlots` — same size as `RingSlots`, radius `RingRadius * OuterRingRadiusScale`
  (200 x 1.8 = 360uu, reproducing the distance the old fallback used), angle offset **half a step**
  so an overflow agent stands in the gap between two attackers rather than behind one.
- `BTService_AcquireTarget.cpp` overflow path: `ClaimOuterSlot` first, own-bearing hold only if BOTH
  rings are full (the twelfth-plus agent on one victim).
- `ReleaseAll` now frees the outer claim too, and `PruneStale` sweeps both rings.

**The watchdog was NOT forked.** `ClaimRingSlot` and `ClaimOuterSlot` both delegate to one
`ClaimSlotIn(Slots, Claimant, bOuter)`, and `GetRingSlotLocation`/`GetOuterSlotLocation` both
delegate to one `SlotLocationIn`. Copying the claim search was the obvious way to build this and the
wrong one: the re-stamp rule inside it ("arrived OR still closing", not "time since claim") was paid
for across #105-#132, and a second copy would drift from it silently.

## Evaluate

**NOT COMPILED, NOT RUN. No behavioural claim.** The editor was open, so the build gate was shut.

What this is grounded in, rather than reasoned from: a 45-second `GS.Combat.LogAI` capture taken by
Michael on 2026-08-20 after `GS.Horde.Order Attack`. All 9-10 goblins reached the target inside
2.4s, but only **4 distinct goblins swung, 7 swings across 19 seconds** (+1.26, +3.37, +3.43, +5.28,
+7.54, +10.66, +19.06). The engagement readout in the same session showed `engaged 8 ... slots 6`,
i.e. two agents with no place.

**What this fix does NOT address, and must not be credited with:** the dominant cause of that
trickle is almost certainly not stacking. The same capture shows `holding off - is staggered/open`
x7 and `PUNISH - is recoiling` x6, and `TryAcquireToken` refuses outright while `CanBeAttacked()` is
false (`GSEngagementComponent.cpp:225-228`) - so the whole gang is locked out together whenever the
victim is flinching, and a bigger gang causes more flinching. That is a separate ticket and a design
question about recoil, not a positioning bug.

**Known imperfection, accepted:** `ClaimSlotIn` derives its preferred slot from
`atan2(bearing) / StepRadians` without accounting for the outer ring's half-step offset, so an outer
claimant can start its search up to half a slot from its true nearest place. It still lands on a
free exclusive slot; it may just not be the closest one. Not worth the complication.

**Never watched:** whether overflow agents actually reach the menace orbit rather than standing at
the outer point. The subtask that diagnosed this could not settle it read-only and neither can this.

## Refine

Added `GetMaxEngaged()` here rather than in #219 because it is a component accessor, but it exists
only for #219's readout - `MaxEngagedAttackers` remains advisory and `RegisterEngaged` still does not
enforce it, per ruling 34.
