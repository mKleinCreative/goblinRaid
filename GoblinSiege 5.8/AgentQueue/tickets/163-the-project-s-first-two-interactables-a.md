---
id: 163
title: The project's first two interactables: a carryable pig and a lootable chest
agent: claude-interactables
status: done
claimed: 2026-08-17T00:12Z
build: none
waiting_on:
evaluated: 2026-08-17T02:53:32Z
observed: 2026-08-17T02:43:43Z | Michael carried the pig, put it down and picked it up again; and confirmed the chest's loot consumed - after looting, the chest no longer offers an interaction prompt
scenario: PIE in L_CombatArena with BP_Livestock_Pig and BP_LootChest placed either side of the path out of the PlayerStart; hold-F channels of 1.0s and 1.5s respectively
files: 
  - Content/Blueprints/Interactables/BP_Livestock_Pig.uasset
  - Content/Blueprints/Interactables/BP_LootChest.uasset
---

## Goal

The project's first two interactables: a carryable pig and a lootable chest

## Generate

The project's first two actors carrying a `UGSInteractableComponent`. Until these existed
`ResolveChannelTarget` could only return null - the interaction framework had nothing in the world
to focus on, which is why #161's work could not be watched.

- **`BP_Livestock_Pig`** - `SK_Pig`, `Interact.Carry`, `bIsCarryable=true`, 1.0s channel,
  `bConsumeOnComplete=false`, prompt "Carry the pig".
- **`BP_LootChest`** - `SM_ChestBox`, `Interact.Loot`, 1.5s channel, `bConsumeOnComplete=true`,
  prompt "Loot the chest".

Both minted and configured entirely from Python (`BlueprintFactory` + `BlueprintService`), then
placed in `L_CombatArena` either side of the path out of the PlayerStart.

**Collision was the part that would have silently defeated this.** `RefreshFocus` uses
`SphereOverlapActors` filtered to WorldStatic / WorldDynamic / Pawn / PhysicsBody, and a
`SkeletalMeshComponent` ships with `CollisionProfileName="NoCollision"` - so the pig would have been
invisible to focus with nothing on screen explaining why. Set to `OverlapAllDynamic` + QueryOnly (a
pig must not block the player) and `BlockAllDynamic` for the chest. The profile lives inside the
`BodyInstance` struct and is not reachable through the plain string setter, so it had to be set by
rewriting the whole struct.

## Evaluate

**OBSERVED, both, by Michael.** He carried the pig, put it down and picked it up again - which is
#162's consume-exemption fix working on precisely the case it was written for. And he confirmed the
chest's consume: after looting, it no longer offers a prompt.

That second one is worth recording carefully, because it read as a failure at the time. The channel
was watched climbing to 98% and vanishing, and "it didn't work" was the natural reading. It is in
fact what success looks like: the debug text only draws while `bIsInteracting`, so on the frame the
channel completes the text is already gone and 100% can never be seen. Nothing visible happens on
loot either, because a looted chest is not destroyed - `bConsumeOnComplete` only clears
`bIsAvailable`. **Two separate absences of feedback stacked into looking like a bug.**

**Verified by reading properties back off the assets**, not from setter return values - `DisplayName`
returned success and did nothing, because the real property is `PromptText`. That is exactly why the
read-back is not optional.

## Refine

**Changed after self-review:** `PromptText` after `DisplayName` silently failed; and the pig's
collision profile after discovering the skeletal-mesh default would have made it unfocusable.

**Left undone, and it is the biggest gap in the feature:** there is no HUD interact prompt and no
channel progress bar. The four delegates are published and unbound. A 1.5s hold with no feedback
reads as a dead key, which is what made the chest look broken. Whatever gets built next on
interaction should be that, not more interactables.
