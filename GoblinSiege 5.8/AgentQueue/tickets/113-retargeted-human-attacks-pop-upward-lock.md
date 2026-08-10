---
id: 113
title: Retargeted human attacks pop upward: lock the root; archers never shoot while kiting
agent: claude-gobkit
status: done
claimed: 2026-08-10T05:28Z
build: none
waiting_on:
evaluated: 2026-08-10T05:33:10Z
files: 
  - Content/Characters/Humans/Anims_Combat
  - Content/AI/BT_Archer.uasset
---

## Goal

Retargeted human attacks pop upward: lock the root; archers never shoot while kiting

## Generate

Michael, after the retarget landed: "The humans looked like they jumped up when they swung, and the
archers still kinda ran away but didn't take shots of opportunity."

**1. The jumping - FIXED.** `force_root_lock = True` on all 9 retargeted sequences in
`/Game/Characters/Humans/Anims_Combat/`. Not a flag the retarget flipped: original and retargeted both
read `enable_root_motion=True`, `root_motion_root_lock=REF_POSE`, identical. What differs is the
CONTENT of the root track - the retargeter maps a ~158uu goblin onto a ~180uu human and the pelvis
height difference ends up in the root, so the human lifts off the floor when the montage drives it.
Root-locking pins the root to the ref pose and keeps the body animation. These are AI attacks played
in place; none of them is meant to travel.

**2. The archers - DIAGNOSED, NOT FIXED.** Measured in PIE:

```
BP_ErikaArcher_C_0   target at 1165uu   speed 0   acceleration 0
                     goal 472uu away, correctly 700uu from the target
                     find_path_to_location_synchronously -> NO PATH
```

No path means `Hold bow range` fails, the Selector falls through to `Watch`, and the archer stands
still. And at **1165uu it is outside `BTTask_RangedAttack`'s MaxRange of 1100**, so there is no shot
either. That is exactly "ran away but didn't take shots" - it is not a decision, it is a stuck pawn
that cannot reach the range band where it would shoot.

## Evaluate

**A wrong conclusion I reached and then disproved, recorded because I nearly acted on it.** I found
that the player's position would not project onto the navmesh and concluded the arena's navmesh was
not built. Rebuilding it changed the extent not at all - **2964 x 2964 is already the whole floor**
(the floor is 3000 and the agent radius accounts for the rest). The projection failure was an artifact
of the query's default vertical extent: the navmesh sits at z=-60 and pawns stand at z=52. **The
navmesh is fine.** Had I stopped at the first result I would have "fixed" a working navmesh.

**What the evidence actually points at:** every pawn that failed to path was created by
`GS.Combat.SpawnPatrol` (#110). That command ground-snaps each spawn with a line trace but **never
projects the point onto the navmesh**, so a defender can land fractionally off-mesh, and an off-mesh
pawn can path nowhere. It is my command and my omission.

**NOT VERIFIED:** the root lock has not been seen in play, and the off-navmesh theory has not been
confirmed by projecting a spawn point and comparing. Both need another pass.

## Refine

**Root-locked rather than disabling root motion.** `enable_root_motion=False` leaves the animated root
motion in the pose, which can still visibly slide the mesh; `force_root_lock` pins it outright, which
is what "attacks in place" means.

**Only the HUMAN copies are locked.** The goblin originals are untouched, so the player's own attacks
keep whatever root motion they were authored with.

**Deliberately stopped rather than pushing on.** The archer fix wants
`UNavigationSystemV1::ProjectPointToNavigation` in `GS.Combat.SpawnPatrol` plus another editor-closed
build, and I had already talked myself into one wrong navmesh conclusion this pass. Handing the
diagnosis over rather than spending Michael's session on a second guess.
