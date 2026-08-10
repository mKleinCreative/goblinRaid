---
id: 100
title: RETROACTIVE - guard upside down, sword held by the tip, player sword to 1.6
agent: claude-gobkit
status: done
claimed: 2026-08-09T08:30Z
build: none
waiting_on:
evaluated: 2026-08-09T08:31:06Z
files: 
  - Content/Data/Weapons/DA_Weapon_Guard.uasset
  - Content/Data/Weapons/DA_Weapon_Scout.uasset
  - Content/Maps/Test/L_CombatArena.umap
---

## Goal

RETROACTIVE - guard upside down, sword held by the tip, player sword to 1.6

## Generate

**PROCESS NOTE: this claim is RETROACTIVE, the same rule break as #097.** #099 was closed, then
Michael reported two faults and I fixed them without claiming. Recorded rather than backdated.

**1. The guard was standing on his head - my bug, from the previous ticket.** In UE Python
`unreal.Rotator(a, b, c)` is **`(roll, pitch, yaw)`**, not `(pitch, yaw, roll)`. Placing the guard
with `Rotator(0, 180, 0)` intending yaw 180 set **pitch 180**. Confirmed rather than assumed:
`unreal.Rotator(1,2,3)` reports `pitch=2 yaw=3 roll=1`. Fixed to `Rotator(0, 0, 180)`.

**2. The guard held the sword by the point.** `GS_Sword_Guard` and `GS_Sword` both have the pivot at
one end of the mesh (z spans 0..99.8), and I assumed that meant the same end. It does not: rendered
side by side with a sphere marking the attach pivot, the **goblin sword's pivot is at the hilt** and
the **guard sword's is at the tip**. Two downloads, opposite conventions.

**No rotation can fix that** - the pivot is what sits on the socket, so it stays in the hand however
it is turned. It needs a **translation along the blade**, the same fix the arrow needed in #096:

```
blade_dir  = Rotation.RotateVector(0,0,1)        # blade axis, in socket space
Translation = blade_dir * -(MeshLength * Scale)   # derived from scale, not a second constant
```

Also flipped the rotation `pitch +90 -> -90` so the blade leaves the hand the way it hung before.

**3. Player sword `MeleeMeshOffset.Scale3D` 1.2 -> 1.6** (`DA_Weapon_Scout`), Michael: still reads as
a dagger. Rendered length 160uu on a ~158uu goblin.

**4. `Arena_Guard`** placed in `L_CombatArena` at (-100, 0, 130) facing the player start, level saved.

## Evaluate

**The anchor fix is measured, not eyeballed** - the decisive number, in PIE:

```
guard sword: relative trans = (-144.71, 0, 0)  rot = pitch -90
             GRIP  0.0uu from hand      TIP 144.7uu from hand
player sword: scale 1.60   GRIP 0.0uu from hand   TIP 159.7uu from hand
```

Both swords now hang from the handle. Confirmed by eye as well in `HighresScreenshot00021`.

**I twice reported something as fine that was not, and both were the same error - trusting a
measurement whose assumption I had not checked:**

- I said the guard sword's attachment was "verified geometrically" in #097 on the strength of
  `0.0uu from RightHand`. That number is true of the tip-in-hand case too. **The distance from a
  socket to a component's ORIGIN says nothing about which end of the mesh the origin is.** The check
  that would have caught it is transforming the mesh's two local endpoints and measuring both.
- I reported the player's sword as 76% of body height and treated that as contradicting "reads as a
  dagger". The size was real; what I could not see from a number was the mesh's proportions - a short
  blade under a large ornate hilt.

**Not verified:** whether 1.6 is the size Michael wants, and whether the guard's blade now leaves the
hand at a good ANGLE - it is held correctly but points out roughly horizontally. Both are taste, one
knob each, no rebuild.

**Owed to AGENT_STATE:** the Rotator argument order and the pivot-convention trap.

## Refine

**Derived the translation from `MeshLength * Scale`** rather than writing `-144.71` beside `1.45` -
the #097 arrow lesson: two constants that must move together are a bug waiting for whoever changes
one of them. Re-scaling the guard sword now moves its anchor automatically.

**Did not touch `DA_Weapon_HordeGoblin`.** It uses `GS_Sword`, whose pivot is at the hilt, so it was
never affected - and Michael asked about the player's sword specifically, so the goblins stay at 1.45
until he says otherwise.

**Deliberately left undone:** the guard blade's resting angle, Erika's missing bow, and the goblin
capsule/mesh mismatch.
