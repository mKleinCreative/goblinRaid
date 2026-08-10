---
id: 098
title: Horde goblins hold the sword too
agent: claude-gobkit
status: done
claimed: 2026-08-09T07:09Z
build: required
waiting_on:
evaluated: 2026-08-09T07:17:08Z
files: 
  - Source/GoblinSiege/Horde/GSHordeGoblin.h
  - Source/GoblinSiege/Horde/GSHordeGoblin.cpp
  - Content/Data/Weapons/DA_Weapon_HordeGoblin.uasset
  - Content/Blueprints/BP_HordeGoblin.uasset
---

## Goal

Horde goblins hold the sword too

## Generate

The last bare-handed combatants in the game. `UGSWeaponComponent` reached the player long ago and the
defenders in #097; the horde was still swinging fists.

**`AGSHordeGoblin`** gained `WeaponComponent` + a `DefaultWeapon` slot, equipped in `BeginPlay`
inside the existing `HasAuthority()` block with `SetSlot(Sword)` - a goblin has no weapon wheel to
choose with, and the horde carries no bow or torch of its own.

**`DA_Weapon_HordeGoblin`** - built FRESH, never duplicated, for the reason recorded in #097: the
soft-object mesh fields cannot be cleared from Python, and unlike the humans the goblin rig genuinely
HAS `back_bow` and `spine_quiver`, so a duplicate of `DA_Weapon_Scout` would have hung a bow and a
quiver on every summoned goblin's back. Assigned to `BP_HordeGoblin`.

It carries **`GS_Sword` - the player's own sword** - with the player's `hand_r_weapon` socket and the
player's tuned `MeleeMeshOffset` copied verbatim.

## Evaluate

**VERIFIED IN PIE.** Four summoned goblins, immediately after a horn blast:

```
BP_HordeGoblin_C_0  StaticMeshComponent_0  0.0uu from hand_r_weapon  mesh=GS_Sword
BP_HordeGoblin_C_1  StaticMeshComponent_0  0.0uu from hand_r_weapon  mesh=GS_Sword
BP_HordeGoblin_C_2  StaticMeshComponent_0  0.0uu from hand_r_weapon  mesh=GS_Sword
BP_HordeGoblin_C_3  StaticMeshComponent_0  0.0uu from hand_r_weapon  mesh=GS_Sword
```

Exactly one static mesh each - no stray bow or quiver, which is the trap #097 fell into.

**This one's ORIENTATION is genuinely correct, not reasoned.** That is the difference from the
guards: same mesh, same socket, same skeleton as the player's already-tuned sword, so the rotation
transfers exactly rather than being derived from bounds and hoped for. The goblins should need no
eyeball pass; **the guards still do.**

**Not verified:** how a 1.2-scaled sword reads against a goblin whose head sits 40-70uu above its
feet (the capsule/mesh mismatch from #094) - the blade may look enormous in hand for the same reason
the arrows did. That is the content problem, not this ticket, but it is where it will show.

## Refine

**Copied the player's offset rather than re-deriving it.** The guards' rotation had to be reasoned
from bounds because their mesh, socket and rig were all different; here every one of those three is
identical, so re-deriving would have been a chance to get it wrong for no benefit.

**Deliberately left undone:** Erika's bow (still the one combatant holding nothing), holster sockets
for the humans, and the guards' rotation pass.
