---
id: 279
title: Finite arrows stage 2 - walk-over resupply, AGSAmmoPickup
agent: claude-warren
status: done
claimed: 2026-08-24T04:12Z
build: required
waiting_on:
evaluated: 2026-08-24T04:28:05Z
observed: 2026-08-24T04:28:06Z | Walked onto a bundle in the arena and the quiver went from thirty to forty-five while the bundle disappeared, with no State.Carrying anywhere on the character - so the arrows arrive without taking your hands. Then filled the quiver to its weight cap and walked onto another: it refused, said so by name, and the bundle was still sitting there afterwards instead of vanishing.
scenario: PIE on L_CombatArena with three BP_ArrowBundle placed, moving the player onto them.
files: 
  - Source/GoblinSiege/Weapons/GSAmmoPickup.h
  - Source/GoblinSiege/Weapons/GSAmmoPickup.cpp
  - Content/Blueprints/Interactables/BP_ArrowBundle.uasset
  - Content/Maps/Test/L_CombatArena.umap
---

## Goal

Stage 2 of the finite-arrows plan (ruling 50, #277). Walk over a bundle and you have arrows.
**It must never interrupt a fight.**

## Generate

**`Source/GoblinSiege/Weapons/GSAmmoPickup.{h,cpp}`** - a ~120-line actor. Static mesh root with no
collision, a `USphereComponent` at `PickupRadius = 90` responding to Pawn only, and an authority-gated
begin-overlap that hands arrows to any pawn with a `UACFInventoryComponent` and destroys itself.

`BeginPlay` also **sweeps for pawns already inside the sphere**. A bundle that spawns underneath
someone - a corpse drop at the feet of the goblin who made the corpse - would otherwise sit there
until they stepped off and back on, because begin-overlap has already been and gone.

`Content/Blueprints/Interactables/BP_ArrowBundle` (15 arrows, `SM_Basket` at 0.35) and three placed in
`L_CombatArena`.

### It checks whether the arrows actually arrived

`AddItemToInventoryByClass` returns **void**, and ACF's `Internal_AddItem` returns `-1` with **no log**
when the weight budget or the slot cap refuses the add. So `TryGiveTo` reads the count before and
after. If it did not move it logs once naming both caps, **returns false, and the bundle is NOT
destroyed** - a full player can come back for it instead of watching a pickup evaporate silently.

`bAutoEquip = false`, which is load-bearing rather than tidy: `true` routes the add through
`HandleItemAdded` into `CanBeEquipped`, which returns false with no log (#270's second fault). Arrows
are never equipped and must not touch that path, and passing false explicitly means a *real* equip
failure later cannot be mistaken for this one.

### The three things this deliberately is not

Recorded in the header, because each looks like the obvious answer:

- **Not ACF's `AACFPickup`.** Its `PickUpCapsule` is created in the constructor and never attached to
  the root and never bound to an overlap delegate; `bPickOnOverlap` is read only inside
  `OnInteractableRegisteredByPawn`, whose sole caller in the plugin is **ACF's own**
  `UACFInteractionComponent` - which we do not have. A `BP_ACFPickup` in a level does nothing and
  **logs nothing**. Verified in plugin source before the decision and recorded as a ruling-50 finding.
- **Not `UGSCarryComponent`.** Carrying applies `State.Carrying`, which `UGSGA_BowShot` blocks on. A
  quiver you cannot fight while holding is not a quiver.
- **Not a new `Interact.*` verb.** `UGSInteractionComponent`'s channel **aborts on damage**, which is
  exactly wrong for restocking mid-fight.

## Evaluate

**Runtime, PIE on `L_CombatArena`:**

- Walked onto a bundle: **30 -> 45 arrows**, and the bundle was **destroyed**. Log:
  `'BP_GSPlayerCharacter_C_0' picked up 15 arrows (30 -> 45).`
- **`State.Carrying` was absent on the ASC afterwards** - the check that this is resupply and not a
  carry, and therefore that it cannot have interrupted a fight.
- Two bundles remained, so only the one walked over was consumed.

**The refusal path was forced rather than assumed**, because "silently eaten" is the failure this
code exists to prevent. Stuffed the quiver to 3600 arrows (the exact 180/0.05 weight cap), then walked
onto a bundle:

```
arrows: 3600 (at the cap)     bundles remaining: 2   <- the refused bundle was NOT destroyed
LogTemp: Warning: '...' could not take 15 arrows from 'BP_ArrowBundle_C_1' - the inventory refused
the add and still holds 3600. ... The bundle has been left in place.
```

**Not established:** the multiplayer path. The overlap and the add are both authority-gated, so a
client walking onto a bundle depends on the server's overlap firing. Not tested on a listen server -
co-op is not being exercised this pass, and this is recorded rather than assumed working.

Also not tested: picking up **while carrying a pig**. The two systems touch different components and
`State.Carrying` was confirmed absent after a pickup, but nobody has run both at once.

## Refine

**The `BeginPlay` sweep was not in the first draft** and was added while writing the corpse-supply
case: a bundle dropped by a dying archer frequently spawns *inside* the goblin that killed them, and
begin-overlap has already fired by then. Without the sweep the arrows are unreachable until you walk
away and back.

**`PickupRadius` is re-applied in `BeginPlay`.** The constructor's `SetSphereRadius` runs before any
designer override is loaded, so an edited radius on the Blueprint would have been silently ignored -
the component would keep the C++ default while the details panel showed the new number.

**Deliberately not done:** corpse drops. The plan has an archer death spawning a bundle; that touches
`BP_ErikaArcher` and belongs with an observation of an archer actually dying. The bundles are
hand-placed for now. **Not ACF's death drop**, for the reason recorded in the plan: Erika carries no
arrow item under ruling 48, so there is nothing in her inventory to transfer.
