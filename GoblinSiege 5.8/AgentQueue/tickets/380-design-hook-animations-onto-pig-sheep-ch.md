---
id: 380
title: Design: hook animations onto Pig, Sheep, Chicken so they become raidable livestock
agent: claude-anim
status: done
claimed: 2026-08-30T09:35Z
build: none
waiting_on:
evaluated: 2026-08-30T10:42:35Z
observed: UNOBSERVED 2026-08-30T10:42:37Z - Design/scope ticket only, no code or content touched by this ticket itself; its scope was executed as #381 which carries its own unobserved closure
scenario: none - never run
files: []
---

## Goal

Design: hook animations onto Pig, Sheep, Chicken so they become raidable livestock

## Goal

Michael: "we need to add animations to the pig... [then] maybe in the ACF animations? we can hook up
the sheep and the chicken, therefore opening up the sheep and the chicken to being targets to raid."
A subagent scoped this earlier tonight but was blocked mid-run (Unreal MCP unreachable while the main
session had the editor busy rebuilding) - this ticket is that scoping, written up properly so the
next session can execute directly instead of re-discovering it.

## Generate

**All animation clips already exist, unlinked.** Confirmed via filesystem listing (not editor query,
since MCP was down for this pass):
`/Game/DreamscapeSeries/DreamscapeFarmlands/Meshes/Chars/{Pig,Sheep,Chicken}/Animations/` each hold a
full clip set - Pig has 13 (Idle/Walk/Run/Eat/Hurt/Death/Sit/Sleep/Roll variants), Sheep and Chicken
comparable. **None of the three has an AnimBlueprint at all** - the vendor pack ships raw
`AnimSequence` assets only, no state machine, no Blueprint wiring anything to anything.

**The three animals are NOT at the same starting point, and that changes scope per-animal:**
- **Pig**: has a placed actor, `Content/Blueprints/Interactables/BP_Livestock_Pig.uasset`. Needs an
  AnimBP only.
- **Sheep, Chicken**: have skeletal mesh + skeleton in their respective folders but **zero Blueprint
  actors**. Need a new actor (following `BP_Livestock_Pig`'s pattern) AND an AnimBP each - a bigger
  lift than Pig, not the same task repeated three times.

**Existing pattern to copy for the actor side:** `BP_Livestock_Pig` is already referenced by name as
"the" livestock pattern in two places - `Source/GoblinSiege/Interaction/GSInteractableComponent.cpp:97`
and `Source/GoblinSiege/Raid/GSRaidLibrary.h:133` (the carryable-object machinery, same system
`UGSRaidLibrary::MakeActorCarryable` uses for loot). Read both before building Sheep/Chicken
equivalents - whatever makes Pig carryable/raidable today should extend the same way, not get
reinvented per-animal.

**Michael's "maybe in the ACF animations?" question - answer, not yet fully investigated:** this
project's own AnimBP convention (confirmed independently earlier tonight while working the horn
animation bug) is that `ThirdPerson_AnimBP_Gob` and every other GS character AnimBP derive from
plain `AnimInstance`, NOT any ACF template (`ACF_Template`/`ACF_MMTemplate`) - ACF's animation stack
is not adopted anywhere in this project. A pig/sheep/chicken AnimBP should almost certainly follow
that SAME plain-`AnimInstance` pattern (a simple Idle/Walk/Run state machine driven by speed, no ACF
dependency) rather than reach for ACF's animation system for the first time on a farm animal -
consistent with "animals aren't ACF combat characters" and avoids introducing a second animation
convention into the project. Not a hard ruling - flagging for Michael to confirm, since it's a
design choice, not something the filesystem research alone can settle.

## Evaluate

**Nothing built - this is scope, not implementation.** No `.uasset` created, no code touched.
Filesystem findings above are real (directory listings), but nothing about actual animation quality,
blend behaviour, or whether these clips even retarget cleanly onto their own skeletons has been
checked - unlike the goblin/human rig work earlier tonight, these are vendor-native clips on their
own vendor-native skeletons, so retargeting risk should be low, but "should be low" is not "verified."

## Refine

**Build order for whoever picks this up:**
1. Reconnect Unreal MCP, confirm it responds (`execute_python_code` ping) before starting.
2. Build the Pig AnimBP first (existing actor, existing clips, smallest scope) - Idle/Walk/Run state
   machine minimum, driven by speed off `GSCharacterMovementComponent` or whatever `BP_Livestock_Pig`
   actually uses for locomotion (check before assuming it matches the player/goblin pattern).
3. Verify Pig moving with animation in PIE before touching Sheep/Chicken - the same
   "capture_animation_pose renders real frames, CaptureAssetImage does not" lesson from tonight's horn
   work applies here too; get an actual look, not just a compile.
4. Only then build Sheep and Chicken actors (copying `BP_Livestock_Pig`'s carryable/raidable wiring)
   plus their own AnimBPs.
5. Confirm with Michael whether "targets to raid" means the same carry-and-steal-value loop
   `BP_Livestock_Pig` already has, or something new - not investigated here, don't assume.
