---
id: 102
title: Point the human weapons at the new hand_r_weapon socket
agent: claude-gobkit
status: done
claimed: 2026-08-09T18:51Z
build: none
waiting_on:
evaluated: 2026-08-09T18:54:16Z
files: 
  - Content/Data/Weapons/DA_Weapon_Guard.uasset
---

## Goal

Point the human weapons at the new hand_r_weapon socket

## Generate

Michael authored weapon sockets on `SK_Human_Skeleton` and renamed them to the project convention.
Confirmed present, on the right parents, before touching anything:

```
hand_r_weapon  parent=RightHand  relLoc=(0, -20, 10)  relRot=(0,0,0)
hand_l_weapon  parent=LeftHand   relLoc=(0, -30, 10)  relRot=(0,0,0)
```

`DA_Weapon_Guard.MeleeSocket` `RightHand` -> **`hand_r_weapon`**, retiring the bare-bone attachment
#097 settled for.

**The offset did not need re-tuning, and that is a property of how Michael authored the socket, not
luck.** Its rotation is identity relative to `RightHand`, so the socket's axes ARE the bone's axes and
the `pitch -90` tuned against the bone carries over unchanged. The translation is recomputed from the
same expression as before - `blade_dir * -(MeshLength * Scale)` - still required, because
`GS_Sword_Guard`'s pivot is at its tip.

## Evaluate

**Verified in PIE against the socket itself**, not the bone it hangs off:

```
attached to hand_r_weapon:  GRIP 0.0uu   TIP 144.7uu
```

Both endpoints measured, which is the check #100 established after `0.0uu from RightHand` turned out
to prove nothing about which end was held.

**Not re-photographed.** Four capture attempts put the camera inside a goblin - the arena holds four
`BP_GSPlayerCharacter` pawns that crowd the lens - and I stopped rather than keep burning attempts on
framing. The visual was already confirmed in `HighresScreenshot00025` after #101, and this ticket
changed only the attach point from a bone to a socket sitting 20uu into the same palm with identical
rotation. **The blade's resting ANGLE is therefore unchanged and still unjudged by Michael.**

**Untouched, deliberately:** `DA_Weapon_Scout`. The goblin rig already had these sockets; nothing about
the human ones affects it.

## Refine

**Kept the rotation in the data asset rather than asking for it in the socket.** I had advised the
socket as the cleaner home for it, and it still is - but Michael authored identity, the data-asset
offset already works, and moving it now would mean re-tuning a thing that is correct for a tidiness
that no one can see. If the blade's angle wants changing, the socket is where to change it.

**Now unblocked and left undone:** `hand_l_weapon` exists, so Erika can finally hold a bow - she is
the last combatant with empty hands. `hand_l_torch`, `back_sword`, `back_bow` and `spine_quiver` are
not yet authored on the human skeleton; only `spine_quiver` matters before Erika can carry arrows.
