---
id: 097
title: RETROACTIVE claim - bow/arrow +25%, and guards finally hold a sword
agent: claude-kit
status: done
claimed: 2026-08-09T07:07Z
build: required
waiting_on:
evaluated: 2026-08-09T07:07:59Z
files: 
  - Source/GoblinSiege/Weapons/GSArrowProjectile.cpp
  - Source/GoblinSiege/Characters/GSEnemyCharacter.h
  - Source/GoblinSiege/Characters/GSEnemyCharacter.cpp
  - Content/Data/Weapons/DA_Weapon_Scout.uasset
  - Content/Data/Weapons/DA_Weapon_Guard.uasset
---

## Goal

RETROACTIVE claim - bow/arrow +25%, and guards finally hold a sword

## Generate

**PROCESS NOTE FIRST: this claim is RETROACTIVE and that is a rule break.** #096 was closed and then
the work below was done without claiming anything. Nobody else was editing, so nothing was lost, but
the queue exists precisely because "nobody else was editing" is an assumption an agent cannot verify
from inside. Recorded rather than quietly backdated.

**Bow and arrows +25%** (Michael: "a little small, arrows included"). `DA_Weapon_Scout`'s
`RangedMeshOffset` and `QuiverMeshOffset` scaled to 1.25 - the quiver too, since it is a bag of the
same arrows and would otherwise read as a different size of ammunition. The arrow itself scales in
`AGSArrowProjectile`, where the offset is now DERIVED from the scale rather than sitting beside it
as a second constant:

```
constexpr float ArrowMeshLength = 59.5f;   // measured from GS_Arrow's bounds
constexpr float ArrowMeshScale  = 1.25f;
ArrowMeshOffset = FTransform(FRotator(-90,0,0), FVector(-ArrowMeshLength * ArrowMeshScale, 0, 0), ...)
```

That matters: the arrowhead sits on the collision sphere only because the pull-back equals the
shaft's RENDERED length. Scaling the mesh alone would have slid the point off the sphere by a
quarter of its length and silently undone #096.

**Guards finally hold a weapon.** `UGSWeaponComponent` existed only on `AGSPlayerCharacter`, so every
defender in the game has been swinging, blocking and dying bare-handed. Added the component to
`AGSEnemyCharacter` plus a `DefaultWeapon` slot, equipped in `BeginPlay` with `SetSlot(Sword)`. It is
the same component the player uses, so a guard's sword goes through the same data asset, socket
resolution and holster logic rather than a parallel path.

Deliberately NOT hoisted to `AGSCharacterBase`: the player creates its own in its constructor and
would end up with two.

**Editor:** imported `medieval+sword+3d+model.fbx` as `/Game/_Import/Weapons/GS_Sword_Guard`, created
`DA_Weapon_Guard`, assigned to the five melee defenders. **Not Erika** - a sword welded to an
archer's hand while she draws a bow would look worse than empty hands.

## Evaluate

**Attachment verified geometrically in PIE**, which is the part that can be checked without an eye:

```
BP_CastleGuard01_C_0     static meshes: 1
StaticMeshComponent_0    0.0uu from RightHand   mesh=GS_Sword_Guard
BP_KnightDPelegrini_C_0  static meshes: 1
StaticMeshComponent_0    0.0uu from RightHand   mesh=GS_Sword_Guard
```

Exactly one mesh each, on the hand.

**Two real problems found and fixed on the way:**

1. **No human mesh has ANY weapon socket.** Checked all eight (`hand_r_weapon`, `hand_l_weapon`,
   `back_sword`, `back_bow`, `spine_quiver`): every one missing, while the goblin skeleton has all
   five. Rather than author sockets on skeletons that are per-character (each guard has its OWN
   skeleton, not a shared one), the sword attaches to the **`RightHand` bone** - UE attachment takes
   a bone name just as well as a socket. Human rigs are Mixamo-style: `Hips` / `Spine` / `RightHand`.
2. **The first `DA_Weapon_Guard` gave every guard a floating bow and quiver.** It was duplicated from
   `DA_Weapon_Scout`, and the mesh fields refuse to clear through Python - `None` silently no-ops and
   `SoftObjectPath('')` throws a conversion error. They then attached at the ACTOR ROOT, because the
   bow/quiver sockets do not exist on humans either. Fixed by building the asset **fresh** instead of
   duplicating, so those fields are empty by construction. Verified: 3 static meshes -> 1.

**NOT VERIFIED - needs Michael's eye, and I am not going to claim otherwise.** The sword's ROTATION
in the hand is reasoned, not seen: `Pitch -90` by the same argument the arrow needed (GS_Sword_Guard
is 99.8uu on +Z with the pivot at the base, the identical convention to the player's `GS_Sword`), and
scale 1.2 copied from the player's tuned sword. Whether the blade points along the fingers or through
the palm is a thing a person has to look at. `MeleeMeshOffset` on `DA_Weapon_Guard` is the knob and
needs no rebuild. Attempted a viewport capture; it returns the EDITOR viewport, which does not show
PIE-spawned actors, so it proved nothing.

Also unverified: the +25% bow/arrow scale has not been seen in hand either.

## Refine

**Derived the arrow offset from the scale** rather than writing `-74.375` next to `1.25`. Two
constants that must move together are a bug waiting for whoever changes one of them.

**Built the guard asset fresh rather than fighting the clear.** Two attempts at clearing the
inherited meshes failed in different ways; a third workaround would have been worse than not
inheriting them in the first place.

**Deliberately left undone:** a bow for Erika (she is an archer holding nothing - the next obvious
piece), sword rotation tuning (needs an eye), holster sockets for humans so weapons can be sheathed,
and giving `AGSHordeGoblin` a weapon component - the goblins are still bare-handed too.
