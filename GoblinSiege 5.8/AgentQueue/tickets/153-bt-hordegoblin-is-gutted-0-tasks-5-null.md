---
id: 153
title: "BT_HordeGoblin is gutted: 0 tasks, 5 null decorators, blackboard repointed to ACFAIBB"
agent: claude-hordebt
status: done
claimed: 2026-08-14T04:50Z
build: none
waiting_on:
evaluated: 2026-08-14T20:34:39Z
observed: 2026-08-14T21:12:42Z | Michael watched horn-summoned goblins move, orbit and attack - the warband closed on the enemy and swung instead of standing still
scenario: Summoned goblins with the horn in game after BT_HordeGoblin was repointed to BB_HordeGoblin and re-saved; before the fix the same goblins turned to face and took hit reactions but never moved or swung
files: 
  - Content/AI/BT_HordeGoblin.uasset
---

## Goal

BT_HordeGoblin is gutted: 0 tasks, 5 null decorators, blackboard repointed to ACFAIBB

## Generate

**Resolved by restore, not rebuild** (cloud Cowork session, 2026-08-14, at Michael's direction after
the close-reopen-recompile workflow failed to clear the errors).

Root cause, established from the raw package name tables: the file at HEAD (`df0d91d`, "switching to
ACF stacked framework") was a scaffold corpse, not a damaged copy of the real tree. Its graph nodes
referenced three C++ classes that have never existed in /Script/GoblinSiege
(`BTTask_SetKeyValueEnum/Object/Vector`), its 6 decorator graph nodes carried **no class name at
all**, and not one real combat task (`BTTask_MeleeAttack`, `MenaceOrbit`, `Block`,
`HasAttackToken`...) appeared in the file or in any of the three `BT_HordeGoblin_Auto*` iterations
sitting untracked beside it (all bound to ACFAIBB — the scripted-scaffold trail). There was nothing
for the editor to re-resolve: "Class not found, make sure it's saved!" was literally true.

The real tree was in git-LFS one commit earlier: `688ad92` ("Checkpoint: the warband ..."), oid
`d32ac93f...`, 7,995 bytes, bound to `BB_HordeGoblin`.

Actions:
- Backed up the gutted working-tree file → `D:\goblinRaid\_PristineBackups\BT_HordeGoblin.pristine-20260814.uasset`.
- Extracted the `688ad92` blob from `.git/lfs/objects` → `_PristineBackups/BT_HordeGoblin.688ad92.uasset`.
- Moved `BT_HordeGoblin_Auto1/2/3.uasset` (untracked junk) → `_PristineBackups/`.
- With the BT editor tab closed and PIE off: copied the blob over `Content/AI/BT_HordeGoblin.uasset`
  and reloaded the package in-editor (`EditorLoadingAndSavingUtils.reload_packages` → success).

## Evaluate

**Verified, with evidence:** post-restore runtime-tree dump shows root children **0 → 6**, every
node a real class with its authored name: Block (Target Is Winding Up) / MeleeAttack (Has Attack
Token) / MenaceOrbit (Has A Target) / MoveTo stand-off (TargetActor Is Set) / MoveTo follow
(FollowTarget Is Set) / Wait (Scamper). Blackboard = `/Game/AI/BB_HordeGoblin` (7 keys intact).
Root service `BTService_AcquireTarget` present. This is the TASKS=0 → TASKS>0 proof criterion the
ticket's claimant defined.

**Not verified:** no PIE run yet — a horn-summon smoke test (goblins follow, frenzy on published
threat) is owed before this counts as observed. The restored asset is the pre-ACF checkpoint; if any
in-editor BT edits were made after `688ad92` and before the gutting, they are not in this file (no
evidence any were).

**Known gaps, out of scope here:** (1) BB_HordeGoblin lacks the four #141 order-board keys
(`OrderVerb`, `OrderSubject`, `OrderLocation`, `DeliveryLocation`) that `RefreshStimulus` writes
every 0.2s — Hold/Loot cannot reach the tree until they exist, and the tree has no Hold/Loot/Courier
branches (`BTTask_SmashOrderTarget`/`PickUpCargo`/`DeliverCargo` are unreferenced). Attack works
regardless via `GetAssignedTargetFor` funneling into `TargetActor`, per #141's design note. (2)
`/Game/AI/EGSHordeOrder.uasset` is an **empty** UserDefinedEnum (zero entries) that only the gutted
scaffold referenced — dead weight, candidate for deletion. Both deserve their own tickets.

## Refine

Restore over rebuild survives scrutiny: the militia-pattern tree in `688ad92` is the design #141
explicitly endorsed ("combat branches must stay gated on TargetActor Is Set, not HordeState ==
Frenzy — do not tidy them onto the state key"). Deliberately left undone: the order-board BB keys and
Hold/Loot branches (new ticket), the EGSHordeOrder junk-asset deletion, and the git commit of the
restored file (Michael's call — file currently shows M vs df0d91d, which is correct and wants
committing as the revert of the gutting).

Process lesson for the queue: **BT authoring via script remains inspection-only** (VibeUE service map
was right). The corpse was produced by a scripted scaffold session; the tell is graph nodes with
empty ClassData. Author trees by hand in the editor; script only reads and file-level operations.
