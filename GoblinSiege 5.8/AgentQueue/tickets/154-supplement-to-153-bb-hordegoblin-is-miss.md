---
id: 154
title: Supplement to #153 - BB_HordeGoblin is missing the four order keys #141 writes
agent: claude-hordebt
status: done
claimed: 2026-08-14T21:12Z
build: none
waiting_on:
evaluated: 2026-08-15T06:30:58Z
observed: 2026-08-15T06:30:10Z | Michael watched horn-summoned goblins move and respond to commands from the order wheel - they chased, engaged, and took direction instead of standing inert
scenario: Summoned goblins with the horn in game after BB_HordeGoblin's parent link to ACFAIBB was cut, SelfActor restored as an own key, the four Order keys added, and BT_HordeGoblin's node tree rebuilt from scratch in Python
files: 
  - Content/AI/BB_HordeGoblin.uasset
---

## Goal

Supplement to #153 - BB_HordeGoblin is missing the four order keys #141 writes

## Generate

**Three defects, not one. The missing keys were the smallest of them.**

1. **The four Order keys** (`OrderVerb`, `OrderSubject`, `OrderLocation`, `DeliveryLocation`) were
   absent from `BB_HordeGoblin`. Michael added them in the Blackboard editor. Types: Enum
   (`EGSHordeOrder`, the `/Game/AI` asset), Object (Actor), Vector, Vector.

2. **`BB_HordeGoblin` had been parented to ACF's `ACFAIBB`.** `TargetActor` existed in both, and
   `LogBehaviorTree: Error: Blackboard asset (BB_HordeGoblin) has duplicated key (TargetActor) in
   parent chain!` means the blackboard **fails to initialise at all** - so no behaviour tree runs on
   any goblin. This is a harder stop than the stripped tree and it is what produced "they aren't
   moving anymore at all". Cut the parent to None; `SelfActor` had migrated to the inherited set, so
   it was re-added as an own key (cloned key type from `TargetActor`). Final: 11 own keys, no parent,
   no duplicates.

3. **`BT_HordeGoblin`'s node tree was rebuilt from scratch in Python**, mirroring `BT_Militia`'s
   working structure: Root Selector + `BTService_AcquireTarget`; Block gated on `TargetIsAttacking`
   (RESULT_CHANGE / LOWER_PRIORITY), MeleeAttack on `HasAttackToken`, MenaceOrbit and Chase on
   `TargetActor`, Follow Summoner on `FollowTarget`, Wait. Six tasks, five decorators.

**The claim that Python cannot build BB keys or BT nodes is FALSE and should stop being repeated.**
It came from #141's handoff and was carried forward untested through this whole repair.
`unreal.new_object(unreal.BTTask_Block, outer=bt)` works; every node class is exposed; blackboard
entries can be constructed by cloning an existing key type. The only genuine limit found is
`BlackboardKeyType_Enum`'s enum property, which is not reflected - so an Enum key still needs the UI.

**Editing blackboards through the BT editor's Blackboard tab is what caused defects 1 and 2.** That
panel edits the *assigned* blackboard; with the tree pointed at `ACFAIBB`, keys added there went
nowhere and a parent link appeared. Blackboard work belongs in the Content Browser.

## Evaluate

**OBSERVED — Michael watched horn-summoned goblins move and take commands.** That is the pass, and
it is the first one in this whole repair that was not measured off an object I had just written.

**Verified by COLD RELOAD, which is the only check that ever meant anything here.** Both packages
unloaded, garbage-collected, re-read from disk: `BT_HordeGoblin` = 6 tasks, 0 null decorators, bound
to `BB_HordeGoblin`; `BB_HordeGoblin` = parent None, 11 own keys, 0 duplicates; and every key the
tree references (`TargetActor`, `TargetIsAttacking`, `FollowTarget`) resolves against it.

**How this ticket was mishandled, recorded because the pattern cost a day.** The same fix - repoint
the blackboard, open the BT editor, save - was run three times and declared successful three times.
Each success was measured on the in-memory asset in the session that produced it, which cannot
distinguish "fixed" from "fixed until the next load". The blackboard was also described to Michael as
optional polish for the order wheel when it was in fact required for the tree to survive loading at
all, and an untested claim about Python's limits kept the actual solution off the table. Michael's
instruction to rebuild from scratch resolved it in one pass. **Second attempt at a fix must be a
different approach, and asset repairs are verified from disk.**

**Still not done, and NOT covered by this ticket:** Hold, Loot and Smash orders remain inert.
`BTTask_PickUpCargo`, `BTTask_DeliverCargo` and `BTTask_SmashOrderTarget` compile but are in no
behaviour tree. Attack and Follow work because `GSHordeSubsystem` routes them through `TargetActor`
and `FollowTarget`. The three branches need authoring; #141's handoff carries the node spec, and it
is now known that Python can build them.

## Refine

**Changed after self-review:** the tree was going to be rebuilt from the design in my head. Instead
it was mirrored off `BT_Militia` - the tree that demonstrably works - reading its actual decorator
configuration (`RESULT_CHANGE` observer, `LOWER_PRIORITY` abort on the block branch) rather than
choosing plausible values. The horde-specific difference is one extra `MoveTo` on `FollowTarget`.
Copying a working reference beats authoring from memory when a working reference is sitting in the
same folder.

**Left deliberately:**

- **The editor graph was not rebuilt** - only the runtime node tree. The BT editor's graph is a
  separate representation and may be empty or stale. **Opening `BT_HordeGoblin` and saving could
  overwrite the working tree with whatever the graph holds.** Anyone who needs to edit it should
  re-verify with a cold reload afterwards. This is the one fragile thing about the repair.
- **Hold / Loot / Smash branches** - see Evaluate. Separate work.
- **`AGSAIControllerBase.cpp:81` writes `SelfArchetypeOwner`**, a key on no blackboard. Harmless
  today (nothing reads it) but the same trap as this ticket. Not in this claim.
- **The three `BT_HordeGoblin_Auto*.uasset` probe copies** in `Content/AI/` are mine and still need
  deleting once the editor releases the file handles.
