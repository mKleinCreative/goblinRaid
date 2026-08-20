---
id: 168
title: Smashing a container opens it: bUnlockInteractableOnBreak
agent: claude-lidcrate
status: done
claimed: 2026-08-17T02:38Z
build: none
waiting_on:
evaluated: 2026-08-17T23:18:25Z
observed: 2026-08-17T23:17:44Z | Michael swung at the crate and watched it visibly become SM_CrateBroken in place - same pivot, no jump or sink - and the log recorded 'broken open - its interactable is now available' on the same frame, so the locked interactable was switched on by the break. Whether the subsequent loot channel completed is NOT confirmed: nothing on screen or in the log reports a completed interaction, which is the gap #169 exists to close.
scenario: PIE in L_CombatArena, BP_LootCrate_TEST placed 292uu ahead of the PlayerStart with SmashHitPoints=1, BrokenMesh=SM_CrateBroken, an Interact.Loot interactable shipping bIsAvailable=false, and bUnlockInteractableOnBreak=true; one player sword swing
files: 
  - Source/GoblinSiege/Destruction/GSBreakableComponent.h
  - Source/GoblinSiege/Destruction/GSBreakableComponent.cpp
---

## Goal

Smashing a container opens it: bUnlockInteractableOnBreak

## Generate

`bUnlockInteractableOnBreak` on `UGSBreakableComponent`. On break it finds the owner's
`UGSInteractableComponent` and calls `SetAvailable(true)` - and warns loudly if the flag is set and
there is no interactable to unlock.

Michael's design: **smashing a container is how you open it.** A crate is `SM_CrateOpen` with
`SM_CrateLid` sitting on top; the lid is nominated through `IntactMeshComponentName`, so breaking
hides only the lid and reveals the contents. The interactable ships with `bIsAvailable = false` and
the break switches it on.

Three reasons this beats fracturing crates:

1. **Zero new art.** `SM_CrateOpen`, `SM_CrateLid` and `SM_CrateSquare` all ship with Dreamscape.
   Against 414 static meshes the project has exactly 2 pre-broken variants, so mesh-swapping
   generally is a dead end - but a *lid* is a state change, not a broken variant.
2. **It chains two verbs.** Smash, then loot. That is more gameplay than watching gravel, and it is
   what a goblin prising open a crate should look like.
3. **It exercises the multi-mesh path** that #165 scoped `RetireIntactMesh` for. Before that fix,
   breaking a two-mesh crate would have hidden the entire actor - body, loot and all.

Chaos fracture is then reserved for things that genuinely want to shatter, like the statue.

## Evaluate

**Built, and the break half is OBSERVED.** Michael swung once at `BP_LootCrate_TEST` and watched it
visibly become `SM_CrateBroken` in place - no jump, no sink, which is the measured pivot/bounds match
paying off - while the log recorded on the same frame:

```
'BP_LootCrate_C_0' broken (hidden + FX).
'BP_LootCrate_C_0' broken open - its interactable is now available.
```

So `bUnlockInteractableOnBreak` does what it claims: a container shipping `bIsAvailable = false` is
switched on by the hit. And `BrokenMesh` swaps rather than hides. **No warnings fired**, which
matters - `IntactMeshComponentName = Mesh` resolved, so this did not fall into the multi-mesh
refuse-to-guess path or the no-interactable path. Both of those would have been silent successes in the
log and visible failures on screen.

**The loot half is NOT confirmed, and could not be.** Michael: *"wasn't able to tell if holding F on it
worked."* That is not a defect in this ticket's code - it is the total absence of interaction feedback.
Nothing draws a prompt, nothing draws progress, and `UGSInteractionComponent` **emits no log line on a
completed channel**, so a successful loot and a dead keypress are indistinguishable from inside the
game AND from the log afterwards. This is the second time an interaction has been unfalsifiable this
way; the first was the chest in #163, where a working feature was reported as broken.

**So the verb chain is proven up to the point where feedback ends.** Smash → open → available: watched.
Available → looted: unknowable until #169 lands the channel ring and #170 makes completion loggable.

**Also worth recording:** the same swing broke `StaticMeshActor_2`, the older bare test crate, one
millisecond earlier. `SmashBreakablesInArc` is hitting multiple props per swing as designed - the first
evidence of that from #165 running in anger rather than in reasoning.

**Deliberately not chased:** replication. An interactable going available on clients is still reasoned
by symmetry with the looted-chest path (server-only setter, OnRep) rather than tested, because this is a
single-player PIE session. Unchanged from the first draft, and still an assumption.

## Refine

**Changed after self-review:** added the warning branch. The first draft set the flag and silently
did nothing when no interactable was present - the exact silent-failure shape this session has paid
for repeatedly. A flag that is on with nothing to act on is a misconfiguration and should say so.

**Left undone:** `BP_LootCrate` itself, and whether the lid should tumble off rather than vanish.
Vanishing is what the code does now; a lid that falls would read better, and is a natural use for
#164's `bCollapseOnComplete` pointed at the lid instead of the body. Worth trying once the basic
chain has been watched working - not before.
