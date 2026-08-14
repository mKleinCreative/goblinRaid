---
id: 044
title: Aim video: arc material missing SplineMeshes usage flag, and the aim camera sits inside the grass canopy
agent: claude-cam
status: done
claimed: 2026-08-06T20:23Z
build: none
waiting_on:
evaluated: 2026-08-06T20:26:10Z
files: 
  - Content/VFX/Aim/M_GS_AimArc.uasset
  - Content/Blueprints/BP_GSPlayerCharacter.uasset
  - decision-queue-kanban.html
---

## Goal

Aim video: arc material missing SplineMeshes usage flag, and the aim camera sits inside the grass canopy

## Generate

Parsed `C:\Users\Michael\Videos\aiming bug.mp4` (8.2s, 1684x576) by extracting frames with the
ffmpeg that ships inside `imageio_ffmpeg`. Recorded 13:20, five minutes AFTER the editor picked up
#043's build - so it shows the fixed build still failing. Two separate findings.

**1. The camera is not in the ground. It is inside the WHEAT.** Frames 3-6 are backlit foliage
filling the screen with the goblin as a silhouette, for about two seconds, then it recovers.
Measured the level's ground cover rather than guessing again: `SM_VillageWheat_01` and `_02` are
**158 uu** tall at ~91,500 instances each, `SM_VillageWheat_Grass` 117 uu at 91,930 - roughly 275,000
instances of 117-158 uu canopy. #043 held the camera at 141-167 uu. That is inside it.

**2. I broke my own fix in #043 and the video is the proof.** That ticket's table was computed with
`ARM = 250`, and the same ticket set `AimArmLength = 320` without recomputing. Drop is
`ArmLength * sin(pitch)`, so lengthening the arm to fix crowding made the dive **worse**, and a lift
calibrated for 250 could not pay for 320. As shipped:

| pitch | camera Z | vs 158uu wheat |
|---|---|---|
| 0 | 165 | clear |
| 15 | 141 | **in the grass** |
| 30 | 116 | **in the grass** |
| 45 | 95 | **in the grass** |
| 60 | 36 | **in the grass** |

Exactly the two seconds the video shows.

**Fix - all three are Blueprint values, no rebuild:**
- `AimSocketOffset.Z` 45 -> **110** (the base camera was only 7uu above the wheat tips; any pitch at
  all dived in)
- `AimHighPitchLift` 190 -> **240**
- `AimHighPitchArmScale` 1.15 -> **1.0** (the arm is already 320; lengthening it further only
  deepens the drop, which is the mistake this ticket exists to undo)

Now 230 / 230 / 244 / 193 at 0 / 30 / 45 / 60 degrees - clear of the canopy across the whole range.

**3. `M_GS_AimArc` was missing `bUsedWithSplineMeshes`.** Visible in the video's own Output Log:
*"missing usage flag SplineMeshes! Default Material will be used in game... will recompile every
editor launch until resaved."* The arc ribbon IS spline meshes, so it has been drawing with the
engine default material since it was built - and paying a shader recompile every launch. Set and
saved. #026 fixed the swapped arc/decal material assignment; this flag was never part of that.

## Evaluate

**Both changes are LIVE with no rebuild** - every value is `EditDefaultsOnly` on a Blueprint-backed
class, and both were re-read off a fresh load after saving (`used_with_spline_meshes: True`;
arm 320 / socketZ 110 / lift 240 / scale 1.00).

**The honest headline: #043 was wrong, and it was wrong in a way I had the numbers to catch.** I
computed a table, then changed one of its inputs in the same ticket, and shipped the table. The
Evaluate even listed "320 may be too far" as a risk - while the actual defect was that 320 invalidated
every row above it. **A derived table must be recomputed from the values actually shipped, not from
the ones it was derived with.**

**Still unverified:** nobody has aimed since. The 230uu figure is again trigonometry - it says the
camera is above a 158uu canopy on flat ground. It does not say the framing feels right, and 110uu of
base socket lift is a real change to the aim look: the goblin will sit noticeably lower in frame than
in the video.

**Known remaining holes:**
- **Terrain, again.** All of this assumes flat ground at the pawn's feet. On a downslope the camera
  is over lower ground and clears easily; on an upslope, or with the goblin in a dip, the wheat
  behind him is higher than his feet and can still swallow it.
- **The trees.** `SM_VillageBirch_02` is 1828 uu; foliage that tall is not something a camera height
  can solve, and the spring arm probe may or may not collide with it.
- **60 degrees is 193uu**, only 35 above the canopy. A near-vertical lob is still tight.
- I did not check whether the wheat has collision at all. If it does not, the probe never sees it,
  which is precisely why raising the camera - rather than trusting `bDoCollisionTest` - is the fix.

**Owed AGENT_STATE.md** - two FAILED entries, written: the recompute-from-shipped-values lesson, and
the spline-mesh usage flag (a material used on a `USplineMeshComponent` silently renders as the
engine default and says so only in the log).

**Touched outside the goal:** none; all three files were claimed. This time the claim was taken
before the edits rather than widened after - which is the thing I got wrong twice earlier today.

## Refine

- **Measured the grass instead of raising the camera until it looked right.** "The camera is in
  foliage" invites guessing a bigger number. Reading the instanced-foliage bounds gave 158uu, which
  turns the fix from a guess into a clearance requirement - and explains why 165uu at level aim was
  always marginal rather than fine.
- **Recomputed against the SHIPPED arm, not the one I designed with.** That is the whole defect in
  #043. The proposed table now reads `AimArmLength` off the CDO rather than a literal.
- **Stopped lengthening the arm.** It was added in #043 to fix crowding, but crowding was already
  solved by 250 -> 320; the extra 15% at pitch was buying nothing and costing 40uu of drop.
- **Parsed the video rather than asking what it showed.** The foliage diagnosis, the two-second
  duration, and the material warning all came out of frames and the Output Log visible in them. The
  material bug was not what I was looking for and is arguably the more valuable of the two - the arc
  has been rendering with the wrong material since it was written.
- **Did not touch `bDoCollisionTest` or the probe.** The wheat may well have no collision, in which
  case the probe was never going to save this and camera height is the only lever. Changing probe
  behaviour to chase a foliage problem would have been motion without a mechanism.

**Deliberately left undone:** slopes and dips; the 1828uu birches; tuning 110/240 against how the
framing actually reads; and checking whether the wheat has collision - which matters for the next
camera problem but not for this fix.
