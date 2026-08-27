---
id: 330
title: Defenders AI content moves onto ACF: ACF blackboard, ACF behaviour tree, ACF patrol/combat components
agent: claude-crumble
status: done
claimed: 2026-08-27T20:52Z
build: none
waiting_on:
evaluated: 2026-08-27T22:13:43Z
observed: 2026-08-27T22:11:49Z | Defenders stand on the ground instead of sunk knee-deep after DA_Char_Militia capsule was corrected from 88/34 to 177.5/68.6; Michael: they are not in the ground anymore
scenario: PIE on L_Tutorial_Island, screen recording of a castle guard beside the player near the houses
files: 
  - Content/AI/DA_Race_Human.uasset
  - Content/AI/BT_Militia.uasset
  - Content/AI/BB_Human.uasset
  - Content/Blueprints/Adversaries/BP_CastleGuard01.uasset
  - Content/Blueprints/Adversaries/BP_CastleGuard02.uasset
---

## Goal

Defenders AI content moves onto ACF: ACF blackboard, ACF behaviour tree, ACF patrol/combat components

## Generate

Michael: *"Let's go ahead and move the defenders AI content onto ACF. It's what I wanted from the
beginning and you've been too overly cautious and causing bugs because of these half measures."*

**Done, asset-only, no build:**
- `BT_Defender` - a project copy of ACF's `ACFBT` (the ACF skill is explicit: never edit sample
  assets), on `ACFAIBB`. It already contains a patrol branch (`BTTask_GoToNextWayPoint_C`,
  `ACFUpdatePatrolBTService`), the commands system, return-home, follow-lead, and combat via
  `BTTask_RunBehavior` -> `ACFCombatBT`.
- Militia archetype in `DA_Race_Human` repointed to it. **Archer and Knight deliberately left on
  their existing trees** - staged rollout, not a big bang.
- All 7 placed defenders given `UACFAIPatrolComponent` with their nearest `GS_Road_*` assigned;
  `BP_CastleGuard02` moved onto the ACF-initialising controller.
- Removed my own `BeginPlay -> Delay -> StartPatrolLoop` chain, which would have double-advanced the
  waypoint index against ACF's own task.

The archetype's `BehaviorTree` is `EditDefaultsOnly` and refused `set_editor_property`,
`PropertyAccessChangeNotifyMode.NEVER` and two engine toolsets. `export_text` / `import_text` on the
struct is what worked - worth knowing, it bypasses per-field edit flags.

## Evaluate

**MY DIAGNOSIS WAS WRONG AND THE WORKFLOW OVERTURNED IT.** I reported that ACF's tree could not
drive the defenders until Phase 2b armed the locomotion states, and recommended stopping until that
shipped. That was false.

`UACFCharacterMovementComponent::UpdateMaxSpeed`'s miss branch is **log-only**
(`ACFCharacterMovementComponent.cpp:677-692`): it finds no state and writes nothing - no speed
write, no state write, no blackboard write. **`"Locomotion State inexistent"` is a marker, not a
cause**, and so is `"Invalid Character - ActionsManager"`, which is an inverted-branch log in ACF's
SUCCESS path (`ACFAbilitySystemComponent.cpp:52-59`). Phase 2b is NOT required for the AI. I had
built an argument on a warning without checking whether it did anything - the same mistake as
reading a clean count off a broken measurement.

**THE REAL CAUSE IS TWO BEHAVIOUR TREES ON ONE CONTROLLER**, and it is our layering, not ACF's:

    GSAIControllerBase.cpp:133-146
      Super::OnPossess(InPawn);   // ACF: InitializeBlackboard(ACFAIBB), cache key IDs,
                                  //      SetCurrentAIState, StartTree(BT_Defender)
      ... RunBehaviorTree(BT);    // ours: the archetype tree, on BB_Human

ACF creates `BehaviorTreeComp` but **never assigns `BrainComponent`** (`ACFAIController.cpp:53`).
So the engine's `RunBehaviorTree` -> `UseBlackboard` finds a different blackboard asset, logs
"Forcing new BB" and **re-initialises to `BB_Human`, invalidating every key ID ACF just cached**;
then, with `BrainComponent == nullptr`, it allocates a **SECOND** `UBehaviorTreeComponent` and runs
`BT_Militia` looped. Two trees then issue `MoveTo` against one `UCrowdFollowingComponent`.

That single mechanism explains everything chased this session: `TargetLocation` cleared under us,
the walk-then-snap-back, the erratic speeds, and waypoints that never advanced.

**Three records are provably stale and were the source of my wrong reasoning:**
- `GSAIControllerBase.h:13-16` - "ACF's OnPossess early-returns on our pawns ... our RunBehaviorTree
  calls remain load-bearing" - **false since Phase 2a**; the cast at `ACFAIController.cpp:61-64`
  succeeds now.
- `GSCharacterMovementComponent.cpp:18-20` - "with the bands gone ... this call sets MaxWalkSpeed to
  ZERO ... the restore is not optional" - **false**; the else branch writes nothing, so the restore
  at `:23` is a redundant no-op today.
- `AGENT_STATE.md:42-45` repeats it, and its "StartTree() stays quiet ONLY because no ACF
  BehaviorTree is assigned" is now false - `BP_GSAIController_Militia` carries `BT_Defender`.

**NOT OBSERVED.** ACF's tree is running and defenders reported `AIState.Patrol`, and one walked
2,181uu at 447 u/s - but that was *with* the two-tree fight still in place, so it proves the
pipeline, not the fix.

## Refine

Next, and asset-only: **clear the `BehaviorTree` soft ref on the Militia row of `DA_Race_Human`**, so
`LoadSynchronous` fails at `GSAIControllerBase.cpp:144`, `RunBehaviorTree` is never called, and ACF's
`StartTree(BT_Defender)` is the only tree on the only blackboard. Do NOT also clear ACF's
`BehaviorTree` on the controller - that is the opposite half and abandons ACF's tree.

The durable version is C++ - delete the `RunBehaviorTree` block at `GSAIControllerBase.cpp:137-152`
and rewrite the stale header note - but the content-only stand-in ships with the gate shut.

Deliberately out of scope: arming the locomotion bands (ticket #232 was abandoned on Michael's
instruction 2026-08-23, "they need a fresh ticket"), and `UGSGA_SwordLight::ApplyMoveSpeedScale`
writing `MaxWalkSpeed` raw against CLAUDE.md's rule - its own ticket.

---

## The guards were standing knee-deep in the ground - ACF's initialiser vs the authored capsule

Michael: *"they're walking on the ground, but for some reason, they're still sticking partly through
the ground. it looks bad."*

**Cause, measured:** `DA_Char_Militia` carried `capsule_half_height 88 / radius 34`, while the guard
Blueprints author **177.5 / 68.6** (CastleGuard01) and **181.7 / 70.2** (CastleGuard02).
`UACFCharacterInitializerComponent` - switched ON in Phase 2b-1 (#226) - applies the DataAsset at
runtime and **overwrites the Blueprint's capsule**. The mesh keeps its authored `-177.5` offset, so
the capsule bottom sits **89.5uu above the mesh's feet** and the guard renders knee-deep.

**Fixed:** `DA_Char_Militia` -> 177.5 / 68.6, and `BP_CastleGuard02`'s mesh offset moved
-181.665 -> -177.525 so both guards align to one capsule.

**Why size the capsule to the mesh rather than shrink the mesh:** the player goblin is 240uu tall
and these meshes are 355uu. Humans taller than goblins is correct; the reverse would have made
goblins tower over the humans.

**OBSERVED by Michael** on a screen recording: *"great, they're not in the ground anymore."*

**THIS RETROACTIVELY EXPLAINS AN OBSERVATION I DISMISSED.** Michael said earlier *"the navmesh is
about the height above their knees"*, and I replied that navmesh height cannot affect where
characters stand. That was true and useless: the navmesh was at the true floor all along and the
GUARDS were drawn ~90uu below it. He was reporting the symptom precisely and I explained away the
measurement instead of asking what it implied.

**Two saves silently returned False before this landed.** `save_loaded_asset` reported failure and
the values existed only in memory; the same thing had already happened to `DA_Race_Human` earlier,
where the change was lost on editor restart. `EditorLoadingAndSavingUtils.save_packages(dirty)` is
what worked, and the fix was only confirmed by checking file mtimes and `git status`. **Never trust
a save return value here - verify on disk.**

**Left undone:** the navmesh agent radius is 35 while the guards' capsule radius is now 68.6, so
they can be pathed through gaps they cannot fit. The player has the same mismatch already (52 vs 35)
so this is not new, but doubling the guards' radius makes corner-clipping more likely.

---

## Closing state

**The migration landed and the headline bug is fixed and verified.** Militia runs ACF's `BT_Defender`
on `ACFAIBB`; clearing the archetype's `BehaviorTree` stopped our `RunBehaviorTree` from starting a
second tree, and all 7 defenders now report **BTcomps=1, BBcomps=1** where they previously carried
two behaviour trees and two blackboards driving one movement component.

ACF's patrol demonstrably works: `BP_CastleGuard01_C_3` advanced through `GS_Road_12`'s waypoints and
sat **13uu** off the road centre; after one nudge 5 of 7 were `MOVING` to real road waypoints.

**LEFT UNDONE, raised as its own ticket: the defenders do not start patrolling on their own.**
`AACFAIController::OnPossess` seeds `TargetLocation` with `homeLocation`, so the patrol MoveTo has
nothing to do, `IsCharacterNearPoint` never advances it, and they stand still until something calls
`TryGoToNextWaypoint`. It is aggravated by placement - `BP_CastleGuard02_C_2` sits **7,245uu** from
its assigned road, so its patrol would open with a 7km walk.

**Also left:** the `UCrowdManager` never builds ("Unable to find RecastNavMesh instance"), so
ACF-controlled pawns have no local avoidance and bump into each other. Pre-existing, first logged
2026-08-26 23:17. And the navmesh agent radius is 35 against the guards' new 68.6 capsule radius.
