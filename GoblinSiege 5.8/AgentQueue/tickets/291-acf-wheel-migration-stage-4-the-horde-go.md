---
id: 291
title: "ACF wheel migration stage 4: the horde goblins move onto ACF"
agent: claude-warren
status: review
claimed: 2026-08-24T22:20Z
build: none
waiting_on: "Michael: look at a summoned goblin. Ten of them now hold an ACF axe instead of ours, at the same 1.45 scale - but scale is the one thing a socket check cannot see."
evaluated: 2026-08-24T21:54:24Z
observed:
scenario:
files: 
  - Content/Items/BP_ACFWeapon_GoblinAxe.uasset
  - Content/Blueprints/BP_HordeGoblin.uasset
---

## Goal

Stage 4 of ruling 53, **goblins only**. Michael's call after being shown that the defenders are a
handful of characters seen at a distance while the horde is ten-a-raid and always in shot.

No build: the slots, the allowed types and the starting item were all authored by #270, and stages 2-3
already built every piece of C++ this needs. This stage is two data changes.

## Generate

**`BP_ACFWeapon_GoblinAxe` static mesh scale `1.0` -> `1.45`.**

**This is the whole reason to measure before flipping a flag.** `DA_Weapon_HordeGoblin.MeleeMeshOffset`
has always drawn the goblin axe at **1.45**; ACF's actor was authored at **1.0** in #270 and never
looked at. Turning the flag on without this would have shrunk every goblin's axe by a third - a change
nobody asked for, arriving as a side effect of a refactor, and one that reads as "the goblins look a
bit off" rather than as a migration bug.

**`BP_HordeGoblin.bUseACFEquipment` -> true.**

**It also fixes a double axe that has been there since #270.** That ticket gave goblins an
auto-equipping ACF axe while our mesh path went on building its own, so every goblin has been carrying
two - ours in hand at 1.45 and ACF's on `back_sword` at 1.0. Nobody noticed because #270 verified that
the axe *equipped*, not what the goblin *looked like*. Stage 2's suppression is what resolves it: with
the flag on, `IsSlotOwnedByACF` destroys ours and leaves ACF's.

## Evaluate

Runtime, PIE on `L_CombatArena`, ten goblins summoned by `GS.Horde.SpawnTest`:

```
goblins holding an ACF axe with NO duplicate from our path:   10/10
sample: ACF main weapon = BP_ACFWeapon_GoblinAxe_C_0
        socket = hand_r_weapon        scale = 1.45
        our static meshes on the goblin = []          <- the double axe is gone
weapon actors blocking the camera (whole world) = 0   <- with ten goblins around the player
slot = WeaponSlot.Primary, equipped weapon data = DA_Weapon_HordeGoblin
```

The camera check matters here more than it did for the player: #287 found that goblin axes had been
blocking `ECC_Camera` since #270, and **ten of them around the player is exactly the crowd case #145
was about**. Zero blockers now, with the horde standing in the arena.

**The goblins keep their own weapon data.** `DA_Weapon_HordeGoblin` is still the equipped weapon -
ACF holds the *mesh*, our data asset still supplies stats, turn rate and granted abilities. Melee
damage is GAS and never went through the mesh, so it is untouched.

**NOT ESTABLISHED: what a goblin looks like holding it, and whether they still fight.** The scale
matches the number our path used, which is the strongest thing a measurement can say and is still not
the same as looking. Nobody has watched a goblin swing since the change - the swing is GAS-driven and
should be indifferent to who owns the mesh, but "should be" is the word that has cost this project
whole sessions.

**PIE is running with ten goblins.** Two things: **do their axes look the same size as before**, and
**do they still land hits on the guards**.

## Refine

**The scale was very nearly missed.** Michael had already said "stage 4 is fine, I looked at it" -
which referred to the goblins as they are *today*, drawn by our path at 1.45. It would have been easy
to read that as approval of ACF's actor and flip the flag. Reading `DA_Weapon_HordeGoblin` first is
what turned an accepted-looking asset into a one-line fix.

**Deliberately not done:** the defenders. Guards, Erika, Pelegrini, the peasant and Uriel all still
run entirely on our mesh path, which is why **stage 5 still retires nothing** - `MeleeMeshComponent`,
`RangedMeshComponent`, `EnsureWeaponMeshComponent` and the rest stay live for five characters plus the
torch, horn and quiver props. Stage 5 remains a no-op until either the defenders move or someone rules
that they never will.
