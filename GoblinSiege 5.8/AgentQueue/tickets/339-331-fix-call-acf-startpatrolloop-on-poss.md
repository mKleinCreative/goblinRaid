---
id: 339
title: #331 fix: call ACF StartPatrolLoop on possess so placed guards seed their own patrol
agent: claude-acf
status: done
claimed: 2026-08-28T00:26Z
build: required
waiting_on: 
evaluated: 2026-08-28T23:14:51Z
observed: 2026-08-28T01:41:08Z | All 7 placed guards report IsPatrolLoopActive()=true at BeginPlay with no manual kick, confirming OnPossess StartPatrolLoop fires. But NO guard advanced a single patrol waypoint in ~4 minutes across 970 samples, so patrolling is still broken - the fix is necessary but not sufficient.
scenario: PIE on L_Tutorial_Island, 18:15 build, slate-post-tick sampler on all 7 placed guards, 254 samples over 75s plus 716 over 119s
files: 
  - Source/GoblinSiege/AI/GSAIControllerBase.h
  - Source/GoblinSiege/AI/GSAIControllerBase.cpp
---

## Goal

#331 fix: call ACF StartPatrolLoop on possess so placed guards seed their own patrol

## Generate

`AGSAIControllerBase::OnPossess` now calls `Patrol->StartPatrolLoop(true)` for any possessed pawn
that has a `UACFAIPatrolComponent` with a usable route
(`Source/GoblinSiege/AI/GSAIControllerBase.cpp:179-193`). Guarded so a spline patroller with no path
is skipped and horde goblins, which have no such component, are untouched. The header comment above
`AGSAIControllerBase` was corrected in the same change: it claimed our `RunBehaviorTree()` calls were
"load-bearing", which has been false since Phase 2a.

ACF ships `StartPatrolLoop` callers only in its three ROUTINE tasks, so a guard placed in the level
with `PathToFollow` authored on the component was never seeded by anything. The component header
states the contract: *"Call StartPatrolLoop() once the owning pawn has a valid AACFAIController."*

## Evaluate

**Verified in PIE:** all 7 placed guards report `IsPatrolLoopActive() == true` at BeginPlay with no
manual kick. Before this change all 7 reported false.

**The fix is necessary but was not sufficient**, and that is the honest result. With the loop seeded
the guards still did not patrol, because the waypoints themselves were unreachable - see #341, where
the `GS_Junction_*` orbs turned out to be QUERY_AND_PHYSICS meshes sitting on the road points.

**The double-advance objection is REFUTED by observation.** `.claude/skills/gs-ai-patrol-advancers`
predicted that seeding the loop while `BT_Defender` also runs `BTTask_GoToNextWayPoint` would make
guards visit every OTHER spline point. Watched with both advancers live: `BP_CastleGuard01_C_2`
walked WP1->WP2->WP0->WP1->WP2->WP0, and after the #341 fix `GS_Guard_A1` alternated WP0<->WP1 for
nine consecutive samples. No skipping. Both the pack and the `ai-framework` addendum were corrected.

## Refine

Kept exactly as written. The alternative that was on the table - drop the C++ and move to
`UACFAIStateFragment` - would have regressed this ticket: without the `StartPatrolLoop` call the loop
is never seeded for a placed guard, which is the state all 7 were found in. Nothing changed in
response to the review; what changed was the confidence, from "written, never compiled" to
"observed on 7 guards across several PIE sessions".


> 2026-08-28T01:00Z Adopted by claude-acf (was claude-anims). Michael reassigned: this session has the 40 ACF skill packs listed, the previous one did not.
