---
id: 164
title: Looted containers collapse: bCollapseOnComplete on the interactable
agent: claude-collapse
status: done
claimed: 2026-08-17T00:47Z
build: none
waiting_on:
evaluated: 2026-08-17T02:53:34Z
observed: UNOBSERVED 2026-08-17T02:53:34Z - Compiled and in the binary, but no asset sets bCollapseOnComplete so it has never run. Superseded in practice by BrokenMesh and bUnlockInteractableOnBreak (#168); left switched off deliberately.
scenario: none - never run
files: 
  - Source/GoblinSiege/Interaction/GSInteractableComponent.h
  - Source/GoblinSiege/Interaction/GSInteractableComponent.cpp
---

## Goal

Looted containers collapse: bCollapseOnComplete on the interactable

## Generate

`bCollapseOnComplete` + `CollapseImpulse` on `UGSInteractableComponent`, with `ApplyCollapse()`.

On completion it takes the owner's ROOT primitive - not "the first primitive found", because a
container's root IS its mesh and toppling an arbitrary child would knock over a decoration while the
body stayed put - widens collision to `QueryAndPhysics` if needed (a query-only body simulates
straight through the floor), enables physics, and applies a mass-scaled impulse forward and slightly
up so it topples rather than settling in place.

Physics rather than a mesh swap or a fracture, because at the time neither existed: a swap needed a
second authored mesh per container and the fracture path needed a GeometryCollection per container.

Michael's question drove it - a looted chest was indistinguishable from an unlooted one, since
`bConsumeOnComplete` only flips an availability flag. The first playtest could not tell a successful
loot from a failed one, and neither could I.

## Evaluate

**Compiled and in the binary. NEVER OBSERVED, and no asset sets the flag** - it defaults to false, so
nothing in the project collapses on loot today.

**Superseded in practice, and the honest thing is to say so rather than quietly leave it.** Two
better answers landed within hours:

1. **`BrokenMesh`** (#168) - measurement showed `SM_CrateSquare` -> `SM_CrateBroken` and
   `SM_Barrel_01` -> `SM_BarrelBroken` are properly authored swap pairs sharing pivot, base height
   and bounds to within a centimetre. A mesh swap reads better than a topple and costs one property.
2. **`bUnlockInteractableOnBreak`** (#168) - smashing a container is how you OPEN it, which chains
   two verbs instead of making looting cause a physics event.

`bCollapseOnComplete` remains correct for its actual case - a container that should slump when
emptied, or a lid that should tumble rather than vanish - but it is no longer the plan for crates and
chests.

## Refine

**Changed after self-review:** the first pass took any primitive component; narrowed to the root.

**Deliberately left unset on every asset.** Turning it on for `BP_LootChest` would have pre-empted
the design conversation that produced the lid-and-swap approach, and a chest that flops over when
looted is a strictly worse read than one that visibly breaks open. Shipping a capability nobody uses
is the right call here; shipping it switched on would not have been.
