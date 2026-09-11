---
id: 415
title: Rebuild character rigs from Manny's skeleton and apply ACF's name contract
agent: claude-rig-rebuild
status: done
claimed: 2026-09-10T23:02Z
build: none
waiting_on:
evaluated: 2026-09-11T02:29:06Z
observed: 2026-09-11T02:29:05Z | Michael watched Erika walk through a raid - locomotion playing instead of the idle-and-turn-in-place she had been stuck in all session, immediately after bUseAccelerationForPaths was set true on the live pawns
scenario: PIE raid on L_Tutorial_Island, Michael watching a rendered archer
files: 
  - Tools/Rig/build_manny_rig.py
  - Tools/Rig/apply_acf_contract.py
  - Tools/Rig/conform_to_manny.py
  - Source/GoblinSiege/Combat/GSAnimDebugCommands.cpp
  - Content/Blueprints/Adversaries/BP_ErikaArcher.uasset
  - Content/Characters/ACFRigs
---

## Goal

Erika would not animate on ACF's stack. This ticket began as "rebuild the rig from Manny's
skeleton" and ended somewhere else entirely: the rig was never the cause.

## Generate

**The root cause, found last and worth stating first.**
`FNavMovementProperties::bUseAccelerationForPaths` defaults to **false** (`NavigationTypes.h:430-432`),
and that default picks between two entirely different movement paths:

- false -> `UPathFollowingComponent::FollowPathSegment` calls `RequestDirectMove`
  (`PathFollowingComponent.cpp:1159`), which writes only `RequestedVelocity`
  (`CharacterMovementComponent.cpp:4038`). The requested acceleration inside `CalcVelocity` is a
  LOCAL (`:3877`), added onto Velocity (`:3946-3951`) and never stored - so the `Acceleration`
  MEMBER stays zero forever.
- true -> `RequestPathMove` (`:1149`) routes through `AddMovementInput`, and
  `ControlledCharacterMove` assigns the member at `:6451`.

`UACFAnimInstance::UpdateAcceleration` does `bIsAccelerating = Acceleration > 0`
(`ACFAnimInstance.cpp:383`), and `ACF_BaseMoveset` gates locomotion on `bIsAccelerating`,
`LocalAccel2D`, `AccelerationDirection` and `PivotStartingAcceleration`. **So every ACF character
in this project idled and turned in place correctly and never once started walking, at any speed.**
It was invisible to `ABP_Human`, which is a speed-driven blendspace and never asks.

ACF's own `ACF_Enemy_BP` ticks this box on its component instance; `GSCharacterBase.cpp:54` swaps in
`UGSCharacterMovementComponent`, so we inherited the engine default instead.

**Fix:** `NavMovementProperties.bUseAccelerationForPaths = true` in
`UGSCharacterMovementComponent`'s constructor (new), with the mechanism documented in-place.

**Also landed:** Erika re-rigged through **AccuRig** and imported at Manny scale as
`/Game/Characters/Humans/ErikaArcher/SK_Erika_Rigged` (118 bones, bone 0 `root` at the origin,
180.37 uu vs Manny's 180.54, A-pose within 1.6 deg, zero unweighted verts, physics asset
generated). `BP_ErikaArcher` runs ACF's **unmodified** `ACF_Humanoid_ABP` with no conform script,
no retarget library and no duplicated assets. 182 dead `ACFRigs` assets and `/Game/Characters/Rigs/`
deleted; the dead `SK_Human_Manny_Skeleton` entry removed from `ACF_UE5Manny.CompatibleSkeletons`.

## Evaluate

**Measured with a control, in PIE, five consecutive samples.** Two `BP_CastleGuard01_C` instances,
same class, same 280.1 uu/s, one variable:

| | flag | speed | acceleration |
|---|---|---|---|
| treated | true | 280.1 | **4131.5** |
| control | false | 280.1 | **0.0** |

Then on all four archers live: flag true -> `|accel| 3493.5`, `bIsAccelerating true` -> **Michael
watched Erika walk**.

**The rig rebuild in this ticket's original plan was abandoned and its output deleted.** The
hand-fitted Manny rig it produced was broken in six ways that all eleven of its own assertions
passed on - `spine_05` above the top of the head, `spine_04/05` carrying zero skin weight, twist
bones 26 cm off the arm, an inverted twist ramp, orphan verts bound to undriven corrective bones,
no physics asset. `Tools/Rig/build_manny_rig.py` stays on disk as a research artifact, NOT a
shipping path; `conform_to_manny.py` is retired. AccuRig solved bone fitting - the one step with no
engine support - in one pass.

**Three false premises drove this chain and are corrected in `AGENT_STATE.md`:** the #413 moveset
claim (ACF movesets ARE data-only children; the CDO probe that "disproved" it returns identical
output for an asset with 15 node types), the over-generalised `Hips`/`IsCompatibleMesh` story (real
mechanism, never fired on our asset), and the blend-mask explanation for the bow (wrong in three
links). A fourth: "she is frozen" readings were taken on OFF-SCREEN instances -
`VisibilityBasedAnimTickOption = AlwaysTickPose` refreshes bones only when rendered
(`SkinnedMeshComponent.cpp:1805`), which is normal and matches the guards.

**The Blueprint route for this flag does not work** and must not be used: setting
`nav_movement_properties.use_acceleration_for_paths` on the component template reads back True on
the CDO and **False at runtime**, because a nested struct field on a component created via
`SetDefaultSubobjectClass` does not make it into the override record. The six Blueprint edits were
reverted. The constructor is the only working home.

## Refine

Remaining, none of it rig work:
1. The bow needs a `Moveset.Bow` **moveset** entry - `ACF_Humanoid_ABP` maps `Moveset.Bow` to an
   overlay but to NO moveset, and `FindByKey` is an exact tag match (`ACFAnimTypes.h:35`), so simply
   retagging the bow to `Moveset.Bow` would silently kill locomotion. Author it in our own data-only
   child of `ACF_Template_ABP` (the horse/wyvern pattern), not by editing ACF's sample.
2. Eight twist bones are named `cc_base_l_forearmtwist01` where ACF's clips animate
   `lowerarm_twist_01_l` - weighted but never driven. Pure rename via `SkeletonModifier`. Cosmetic.
3. Regenerate the other four humans through AccuRig at 1.0 scale; they are still 3.4 m and on the
   Mixamo rig.
4. Watch for arrival overshoot: acceleration mode makes agents brake over a distance
   (`PathFollowingComponent.cpp:623-626`). BT audit found no task gating on velocity - all four use
   distance - but `BTTask_PickUpCargo` fails if outside range at check time, so it may retry.
