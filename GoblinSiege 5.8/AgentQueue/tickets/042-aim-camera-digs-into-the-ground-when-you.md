---
id: 042
title: Aim camera digs into the ground when you pitch up to lob a torch
agent: claude-cam
status: done
claimed: 2026-08-06T19:55Z
build: required
waiting_on:
evaluated: 2026-08-06T19:57:43Z
files: 
  - Source/GoblinSiege/Characters/GSPlayerCharacter.h
  - Source/GoblinSiege/Characters/GSPlayerCharacter.cpp
  - decision-queue-kanban.html
---

## Goal

Aim camera digs into the ground when you pitch up to lob a torch

## Generate

Michael, 2026-08-06, after the wheel went in: *"it's a little hard to throw a torch right now, because
aiming and looking to arch it correctly tends to bring the camera into the ground."*

**Cause, measured not guessed.** A spring arm with `bUsePawnControlRotation` swings its far end DOWN
by `ArmLength * sin(pitch)` when you look up. Reading the real values off `BP_GSPlayerCharacter`'s
CDO - capsule half height **120**, boom relative Z **0**, aim socket Z **45**, so the pivot sits
**165uu** above the goblin's feet - and the aim arm at 250:

| control pitch | camera Z above feet, BEFORE |
|---|---|
| 0 deg | 165 |
| 30 deg | 40 |
| **45 deg** | **-11.8 (below the ground plane)** |
| 60 deg | -51.5 |

45 degrees is the maximum-range launch angle, i.e. exactly where you aim to lob. The camera never
visibly passed through the floor because `bDoCollisionTest` is on - the probe dragged it in against
the goblin's back instead, which is the "into the ground" feel.

**Fix: pull the camera IN and lift the pivot UP as pitch rises**, rather than clamping how far up you
may look. A pitch clamp was the smaller change and I rejected it - the arc IS the weapon, and capping
the look angle caps the throw.

Three new tunables on `AGSPlayerCharacter`:
- `AimHighPitchDegrees = 45` - pitch at which the correction is fully applied. 45 because past the
  max-range angle you are lobbing shorter, not further.
- `AimHighPitchArmScale = 0.5` - arm multiplier at full correction. This is the part that does the
  work; the drop is proportional to arm length.
- `AimHighPitchLift = 90` - extra `SocketOffset.Z`, giving the shortened arm headroom.

All three scale by the aim `Eased` alpha as well as by pitch, so nothing leaks into the hip camera.

**One structural change was required.** `UpdateAimCamera` early-returned once the blend settled -
deliberately, so the boom is not rewritten every frame of a raid. That early-out also meant a
pitch-dependent correction could never run. It now returns only when settled **and** fully hip, so
the "don't touch the boom all raid" guarantee is kept exactly where it mattered and dropped exactly
where it was wrong.

## Evaluate

**NOT COMPILED, NOT RUN** - the editor is open.

**The diagnosis is measured; the fix is only computed.** Capsule half height, boom offsets and arm
lengths were read off the live CDO over MCP, so "the camera is 11.8uu below the goblin's feet at 45
degrees" is arithmetic on real values rather than an estimate. The corrected column of that table is
the same arithmetic applied to the new constants:

| pitch | after |
|---|---|
| 0 / 15 / 30 / 45 / 60 deg | 165 / 141 / 142 / 167 / 147 |

Camera height stays within a 26uu band across the whole range instead of falling 216uu. **But that is
geometry, not gameplay** - it says the camera is off the floor, not that aiming feels good.

**What I cannot predict and did not try to:**
- **Whether pulling to 125uu at full pitch feels claustrophobic.** Half the aim arm is close, and the
  goblin's back will fill more of the frame exactly when you are trying to judge an arc.
- **Whether the correction moving continuously with pitch reads as drift.** The camera now slides in
  and up as you look up; smooth, but it is motion the player did not ask for.
- **Terrain.** The numbers are against flat ground at the goblin's feet. On a slope, or standing on a
  roof - which is a real torch-throwing position in this game - the ground under the camera is not at
  the pawn's foot height and the probe can still bite.

**Not addressed, deliberately:** the pre-existing note in the constructor that `SocketOffset` is
applied AFTER the collision probe, so the -55 lateral aim offset can push the camera into a wall the
probe already cleared. `AimHighPitchLift` rides the same path - upward is far safer than sideways,
but it is the same known hole and I have not closed it.

**Owed AGENT_STATE.md** - FAILED, once this is confirmed in play: *a spring arm with
`bUsePawnControlRotation` drops its camera by `ArmLength * sin(pitch)`; any aim mode that shortens
the arm for framing makes looking UP worse, not better, because the collision probe then has less
room.* Holding it until it is verified rather than writing memory from a computation.

**Touched outside the goal:** none - both source files were claimed. The board card is claimed too.

## Refine

- **Measured the rig before writing the fix.** My first estimate of the pivot height used a guessed
  capsule half height of ~88; the real value is 120. That would have put the "underground" threshold
  at the wrong pitch and produced a correction curve tuned to a goblin that does not exist. Reading
  the CDO cost one MCP call.
- **Rejected the pitch clamp**, which was the obvious one-line fix. Clamping `ViewPitchMax` stops the
  camera dipping by stopping you looking up - and looking up is how a lob is aimed. It would have
  fixed the symptom by removing the feature.
- **Scaled the correction by `Eased` as well as by pitch.** Without it, the hip camera would also
  pull in and rise when you looked up, changing traversal and combat framing to fix a torch problem.
- **Narrowed the early-out instead of deleting it.** The obvious move was to drop the
  `IsNearlyEqual` return so the correction runs every frame. That return exists for a reason the
  comment states plainly - the boom would otherwise be rewritten every frame of the whole raid - so
  it now returns only when settled AND fully hip, which preserves the guarantee where it applies.
- **Left the three numbers as EditDefaultsOnly on a Blueprint-backed class.** `BP_GSPlayerCharacter`
  exists, so unlike `AGSArrowProjectile::ArrowMeshOffset` these are genuinely tunable without a
  rebuild - which matters, because they are feel values and my only evidence is trigonometry.

**Deliberately left undone:** the SocketOffset-after-probe hole; any handling for sloped ground or
throwing from a rooftop; and tuning the three constants, which cannot honestly be done until someone
has thrown a torch off a hill.
