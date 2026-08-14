---
id: 145
title: Camera lurches in and out during a crowd fight: every character blocks the spring arm's camera probe
agent: claude-cam2
status: done
claimed: 2026-08-13T05:44Z
build: required
waiting_on:
evaluated: 2026-08-13T05:54:14Z
observed: 2026-08-13T05:52:38Z | Michael stood in a ten-goblin scrum and reported the camera is fixed - no more zooming in and out as bodies cross between him and the boom
scenario: PIE in L_CombatArena, 10 horn-summoned goblins fighting a 3-man garrison with the player standing at the edge of the melee
files: 
  - Source/GoblinSiege/Characters/GSCharacterBase.cpp
---

## Goal

Camera lurches in and out during a crowd fight: every character blocks the spring arm's camera probe

## Generate

Michael, watching a ten-goblin scrum 2026-08-13: *"the camera for the player zooms in and out
though. We shouldn't zoom in when we're in a crowd, it makes it hard to see or comprehend what's
going on."*

**Cause, read off the live objects rather than guessed.** `AGSPlayerCharacter`'s spring arm probes on
`ECC_Camera` with a 12uu probe over a 450uu arm, and BOTH of a character's collision primitives block
that channel by default - the capsule via the `Pawn` profile, the mesh via `CharacterMesh`. Every body
crossing the line between camera and player therefore drags the boom in and lets it back out. One
passer-by is a shrug; ten goblins in a melee is a camera that never stops moving, at exactly the
moment the player most needs to read the fight.

**One change, in `AGSCharacterBase`'s constructor:**

```cpp
Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
MeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
```

**On the base, not on the player**, and that is the whole design of the fix: the offenders are the
OTHER characters. The spring arm already ignores its own owner, so the player was never occluding
himself - it is the crowd he stands in, and that crowd is every defender and every horde goblin. One
line on the shared base reaches all of them and cannot be forgotten on a new adversary Blueprint.

**Safety checked before touching a collision response globally:** grepped the entire module for
`ECC_Camera`, `ProbeChannel` and camera trace channels - **zero matches**. Nothing in GoblinSiege
traces on that channel; the spring arm is its only consumer. Perception uses the sight sense's own
channel and the melee sweeps use `ECC_Pawn`.

World geometry still blocks the probe, so the camera keeps pulling in at walls and doorways, which is
what `bDoCollisionTest` exists for. Only bodies became transparent to it.

## Evaluate

**Verified three ways, and the last one is the one that counts:**

1. **Builds.** Editor-closed, `Result: Succeeded` in 01:31, zero errors.
2. **The state actually changed.** Read back off four character CDOs after the build - horde goblin,
   militia, archer and player - every capsule and every mesh now reports `ECR_IGNORE` on the camera
   channel. The same read twenty minutes earlier reported `ECR_BLOCK`, so this is a genuine
   before/after rather than a claim.
3. **A human watched it.** Michael, standing at the edge of a ten-goblin melee: *"the camera is
   fixed, no more zooming."* See `observed:`.

**What is NOT claimed:** that the camera never moves. It still collides with world geometry, by
design, and nobody has walked this build into a doorway or a wall to confirm that half still works.
The change only makes bodies transparent to the probe; if the camera stops pulling in at walls too,
something else is wrong and this is where to look first.

**Owed to AGENT_STATE:** characters ignore `ECC_Camera` as of #145, set on `AGSCharacterBase` so it
covers every pawn; and the reason - a spring arm probing on a channel that every body blocks is a
camera the crowd controls.

**Found while in here, unrelated and not fixed:** `BP_HordeOrderMarker`'s beacon material has
reverted to `/Engine/EngineMaterials/DefaultMaterial`. The `M_HordeOrderMarker` assignment made in
#141 read back correctly at the time but did not survive the Blueprint recompile, so order beacons
draw grey with no per-verb colour. Cosmetic, and exactly the silent-failure mode
`AGSHordeOrderMarker::ApplyVisuals` documents.

## Refine

**Nothing changed on self-review, and the first pass survives scrutiny** for a specific reason worth
recording: the temptation was to reach for the spring arm - shorten the probe, damp the lag, disable
`bDoCollisionTest`. Every one of those treats the symptom on the player and leaves the actual cause,
which is that a hundred-odd pawns in this project each advertise themselves as something a camera
should avoid. Fixing it at the source is one line instead of a tuning exercise, and it is why the
change landed on `AGSCharacterBase` rather than `AGSPlayerCharacter`.

**Deliberately left undone:**

- **The beacon material regression** noted in Evaluate. It belongs with the order-wheel work, not
  with a camera ticket, and re-assigning it here would have been scope creep into an asset this
  ticket never claimed.
- **Confirming the camera still pulls in at walls.** Argued from the change (only bodies were made
  transparent) rather than watched. Worth ten seconds next time somebody backs into a doorway.
- **Erika's skip-and-glide**, reported in the same session. Michael's own diagnosis narrows it well:
  *"it's only her, because she wants to flee until she's cornered. it's really just her making small
  movement adjustments that cause her to look like she's skipping."* That is anim thrash from
  micro-repositioning during the kite, not a dead blendspace - each tiny correction starts and stops
  locomotion. The likely fix is a deadzone on the archer's reposition so corrections under some
  threshold are not issued as moves at all, which is the same shape as #108's two-authorities
  disagreement and the ring-slot arrival tolerance. Its own ticket.
