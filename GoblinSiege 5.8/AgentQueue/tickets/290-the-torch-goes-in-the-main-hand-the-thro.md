---
id: 290
title: The torch goes in the main hand - the throw animation uses the right
agent: claude-warren
status: done
claimed: 2026-08-24T22:05Z
build: none
waiting_on:
evaluated: 2026-08-24T21:37:30Z
observed: 2026-08-24T21:40:54Z | Michael selected the torch and it came up in his right hand - the hand the throw animation actually swings, where before it sat in the left while the right mimed the throw. He judged the grip angle slightly off but fine for now, and attributed it to how the character hand is rotated rather than to the socket.
scenario: Michael playing in PIE with the torch slot selected.
files: 
  - Content/Data/Weapons/DA_Weapon_Scout.uasset
---

## Goal

Michael: *"The torch needs to go into the main hand too, the throwing animation uses the right hand
instead of the left."*

`AM_GS_ThrowTorch` swings the right arm while the torch sat in `hand_l_torch` on the LEFT hand - so
the throw was mimed by an empty hand while the torch stayed put in the other one.

## Generate

`DA_Weapon_Scout.HeldTorchSocket`: **`hand_l_torch` -> `hand_r_weapon`**. Data only, no build.

**`hand_r_weapon` is the only socket on `R_Hand`.** The full list on `GOB_Scout_v3` is
`hand_r_weapon`, `back_bow`, `spine_quiver`, `hand_l_weapon`, `hand_l_torch`, `back_sword`,
`hand_l_horn` - every held socket except one is left-handed. There is no `hand_r_torch`.

**No new socket was authored, deliberately.** Adding one is the cleaner engineering and it is also
how this project has twice lost a socket to me - and a dedicated socket only earns its place once we
know the shared one looks wrong. Pointing at the existing socket is reversible, needs no asset edit
on the skeleton, and leaves `HeldTorchMeshOffset` as the tuning knob it already is.

**Sharing the socket with the axe is safe**, and this is worth stating because it looks like a
conflict: the axe only occupies `hand_r_weapon` while it is DRAWN, and selecting the torch slot
sheathes it to `back_sword` (#287). The hand is free whenever the torch is in it. Verified below.

## Evaluate

Runtime, PIE on `L_CombatArena`, torch slot selected:

```
OURS  GS_Torch    socket=hand_r_weapon   visible=true     <- right hand, as asked
OURS  GS_Quiver   socket=spine_quiver    visible=true
ACF   ScoutPrimary socket=back_sword     shown            <- sheathed, hand free
ACF   ScoutBow     socket=back_bow       shown
```

**The measurement that matters for the look, handed over rather than guessed at.** The torch's
rotation now comes from a socket oriented for an axe:

| | location | rotation |
|---|---|---|
| `hand_l_torch` (old) | `-1.3, -27.0, 11.7` | pitch **-30**, yaw 0, roll **180** |
| `hand_r_weapon` (new) | `-6.7, -13.0, -7.0` | pitch **49.3**, yaw **-173.7**, roll **15.8** |

`HeldTorchMeshOffset` is currently **identity** - no translation, no rotation, scale 1 - so the torch
takes the axe's grip exactly. Whether that reads as "holding a torch" or "holding a torch like an axe"
is an eye judgement, and **`HeldTorchMeshOffset` on `DA_Weapon_Scout` is the field to nudge**; it is
per-weapon and affects nothing else.

**Not established: how the throw looks now.** The functional half is right - the torch is in the hand
the animation swings. But nobody has watched a throw since the change, and the torch's *departure*
point is `UGSAimComponent`'s muzzle transform rather than the hand socket, so the torch leaving the
hand and the projectile appearing are two different positions that have never had to agree before.

## Refine

**The obvious mirror was rejected.** Copying `hand_l_torch`'s transform onto `R_Hand` as a new
`hand_r_torch` looks like the tidy answer, and a left-to-right mirror is not a sign flip with
rotations like these - it would have been a guess wearing the costume of a measurement.

**Deliberately not changed:** `TorchWindupSeconds` and its coupling to `AM_GS_ThrowTorch`'s release
moment. The hand changed, not the timing, and the spawn is still driven by the wind-up timer rather
than an AnimNotify - a coupling `UGSGA_TorchToss` already documents as owed work.


---

### Accepted slightly off, 2026-08-24 - and his diagnosis is the useful part

Michael, having looked: *"it's off by a little bit, but that's ok for now, it's more based off how
the characters hand is rotated."*

**Accepted below the line under ruling 59.** It is in the right hand, the throw reads, and the
residual is a grip angle - it does not stop the primary player doing anything.

**His diagnosis is worth more than the observation, and it points somewhere I would not have looked
first.** The natural assumption is that `hand_r_weapon`'s socket rotation is wrong for a torch and
that `HeldTorchMeshOffset` can counter it. He is saying the **hand's animated pose** is the source -
the socket rides `R_Hand`, and that bone is posed by whatever animation is playing, which for this
character is a grip shaped around an axe haft rather than a torch shaft.

If that is right, and it fits what is on screen, then **a fixed offset cannot fully fix it**: a
constant rotation added to a bone whose own rotation changes per animation will look right in one
pose and wrong in the next. The honest fixes are a torch-specific grip pose or an anim-layer overlay,
which is animation work rather than a number on a data asset.

**Recorded so the next person does not spend an afternoon tuning `HeldTorchMeshOffset`** and
concluding the number is simply hard to find. It is not hard to find; it may not exist.

**Not investigated further**, deliberately - ruling 59, and Michael said "ok for now".
