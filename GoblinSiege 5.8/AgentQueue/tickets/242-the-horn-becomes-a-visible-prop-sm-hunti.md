---
id: 242
title: The horn becomes a visible prop: SM_HuntingHorn attaches to the hand, and the goblin blows it instead of shouting
agent: claude-warren
status: done
claimed: 2026-08-21T19:39Z
build: required
waiting_on:
evaluated: 2026-08-22T01:22:19Z
observed: 2026-08-22T01:21:16Z | Michael watched the horn appear in the goblins left hand at the correct size and orientation, held to his mouth, with the note sustaining across a long blast and the hold pose solid - no loop jitter. He accepted it: it is good as it is for now.
scenario: PIE in L_CombatArena, player pawn (BP_GSPlayerCharacter on GOB_Scout_v3), horn blown on middle mouse as taps and as long holds, across several sessions between 22:13 and 23:00 UTC.
files: 
  - Source/GoblinSiege/Weapons/GSWeaponDataAsset.h
  - Source/GoblinSiege/Weapons/GSWeaponDataAsset.cpp
  - Source/GoblinSiege/Weapons/GSWeaponComponent.h
  - Source/GoblinSiege/Weapons/GSWeaponComponent.cpp
  - Content/Characters/ScoutV2/Montages/AM_GS_HornBlast.uasset
  - Content/Characters/ScoutV2/Anims_LocoSet/A_GS_HornBlow_Gob.uasset
---

## Goal

The horn becomes a visible prop: SM_HuntingHorn attaches to the hand, and the goblin blows it instead of shouting

## Generate

**`hand_l_horn` socket, authored on `GOB_Scout_v2`** — bone `L_Hand`, seeded from `hand_l_torch`'s
transform (loc -1.32, -27.0, 11.66 / rot -30, 0, 180) so the horn starts in the palm rather than
inside the wrist. All six pre-existing sockets were already spoken for: `hand_r_weapon` is the melee
blade, `hand_l_weapon` is the drawn bow, `hand_l_torch` is the readied torch, and the three `back_*`
are holsters. Reusing any of them puts two props in one hand the first time someone blows the horn
with a torch up.

**`GSWeaponDataAsset.h`** — `HornMesh` / `HornSocket` / `HornMeshOffset`, an exact copy of the
`HeldTorchMesh` trio including its accepted per-asset redundancy. Header carries an explicit note
that this is expected to be deleted when ACF Phase 2 reaches equipment, and that Michael ruled for it
knowing that, so a later reader does not treat it as an oversight.

**`GSWeaponComponent.{h,cpp}`** — `SetHornRaised` / `IsHornRaised`, `HornMeshComponent`,
`bHornRaised`, `bHornMeshResolveFailed`. Mirrors `SetTorchReadied` with one deliberate difference:
**no slot guard**. The torch needed one because the Torch SLOT owns its prop and an ability must not
take it away; the horn is owned by nothing and belongs to the blast that raised it. `RebuildWeaponMeshes`
re-points a raised horn on a weapon change; `DestroyWeaponMeshes` clears it.

**`GSGA_Horn.cpp`** (claimed by #236, same agent) — raises in `StartBlast`, lowers in `StopBlast`.
`FindComponentByClass` not `GetWeaponComponent()`, the reason `GSGA_TorchToss` already gives: the
horn is universal kit and horde goblins are not `AGSPlayerCharacter`s. The lower is unconditional
rather than guarded on a successful raise, because `EndAbility` runs on paths `StartBlast` never
finished and `SetHornRaised` already no-ops on an unchanged state.

## Evaluate

**BUILT 2026-08-21 14:41, and WATCHED.** This section originally said "NOT COMPILED, NOT RUN" and
"blocked on a compile"; both are now stale and are rewritten rather than left to mislead the next
reader.

**Verified by evidence:**
- Linked: all nine new symbols confirmed present in `UnrealEditor-GoblinSiege.dll` - the three
  montage fields, the three sound fields, `HornMesh`, `HornSocket`, `SetHornRaised`.
- `HornMesh` set and read back on all five `DA_Weapon_*` assets.
- Every hard-coded path in the constructor resolves: three montages (0.800 / 0.800 / 1.233s) and
  three sounds (0.745 / 1.400 **looping=True** / 2.800s).
- `hand_l_horn` resolves on **`GOB_Scout_v3`**, the mesh the player actually uses, at Michael's
  transform including scale 2.0.
- Hold pose frozen: key 24 vs 30/36/42/48 is 0.000000 deg across all 41 bones, read back off the
  saved asset through the engine's own sampler.
- **Michael watched it**: horn visible in the left hand, right size, right way up, at his mouth, note
  sustaining across a long hold, no loop jitter. Accepted.

**The defect that mattered, and the lesson.** For four passes the horn attached to the character root
because the socket was authored on `GOB_Scout_v2` while the player uses `GOB_Scout_v3` - a different
mesh in a different folder, sharing one skeleton. `AttachWeaponMeshToSocket` logged its fallback
**on every single blast**, in a warning written specifically to be impossible to miss. Nobody read it.
Four broken bakes, a video, a stick-figure renderer and a great deal of geometry were spent on a
question the log had already answered. **Read the log before measuring anything.**

**Also wrong, and caught only by measurement:** `fwd = cross(left, up)` is BACKWARD in Unreal's
handedness, so every early solve aimed the horn behind his head. Orthogonality (`up . fwd = 0`) was
the only axis test in place and it is blind to sign. The solve now asserts `dot(fwd, toes) > 0.5`
every frame.

**Damage done, declared:** Michael's `hand_l_horn` socket was destroyed twice by this ticket, the
second time after I had said I would stop writing to that mesh. `remove_socket` deletes EVERY copy of
a name. It was recoverable only because his values had been read into the transcript minutes before.

**Touched outside the goal:** `GSGA_Horn.{h,cpp}` (claimed by #236, same agent) and the three
`AM_GS_HornBlast_*` montages, which were new and unclaimed - declared in Refine rather than hidden.

**AGENT_STATE.md owes a DECISION line** recording that the horn prop lives on `UGSWeaponDataAsset`
under protest pending ACF's equipment migration, and that the baked arm pose is coupled to the socket
transform.

## Refine

**Still to do, in order, after the next build:**
1. Set `HornMesh = SM_HuntingHorn_Signal01` on all five `DA_Weapon_*` assets.
2. Tune `HornMeshOffset` by eye. The socket is seeded from the torch's transform, which is a
   starting guess and not a measurement - the torch is a stick held upright and the horn is a curved
   cone held to the mouth. This is Michael's call, not mine.
3. `A_GS_HornBlow_Gob` - the baked clip. Not started.

> 2026-08-21T19:44Z Source + socket done, uncompiled. Data assets cannot be populated until HornMesh exists in reflection data.

### The baked clip: `A_GS_HornBlow_Gob`

Duplicated from `A_MX_Taunt_Battlecry_Gob` (86 keys, 30fps, 41 bones) and the **left arm rewritten by
IK** so the hand holds the horn at the mouth. `AM_GS_HornBlast` now points at it.

**Three read APIs failed before one worked**, worth recording so nobody retries them:
`AnimSequence.get_data_model()` does not exist; `IAnimationDataModel.get_bone_animation_tracks()`
marshals as an empty array; and `RawAnimSequenceTrack.get_positional_keys()` / `_rotational_keys()` /
`_scale_keys()` return **zero** keys for all 41 bones on an animation that demonstrably plays. The
working reader is `unreal.AnimationLibrary.get_bone_poses_for_frame(seq, names, frame, False, mesh)`,
which returns LOCAL-space transforms (verified empirically - the values are bone-sized and run down
local -Y, not accumulating positions). Writing works fine through
`controller.set_bone_track_keys`.

**Nothing about the skeleton's orientation is assumed.** The anatomy axes are derived from the
skeleton itself each frame: character-left from `r_thigh -> l_thigh`, up from `hip -> head`, forward
from their cross product. Confirmed orthogonal (`up . fwd = 0.000`, `left . fwd = 0.000`). The mouth
target is `head + forward*10 + up*2` uu - **those two numbers are the tuning dials**, and they are
guesses about where a goblin's mouth is relative to the head bone, not measurements.

**The first solve was wrong and the measurement caught it.** Unconstrained CCD put the hand exactly
on the mouth on all 86 frames - and swung the elbow up to **23.7 uu behind the shoulder** and within
**10.1 uu of the spine**, i.e. the arm passing through the torso. "Hand on target" said nothing about
the elbow. Re-solved as analytic two-bone IK with an explicit pole (elbow biased down, out and
slightly forward):

| measure | CCD (rejected) | analytic + pole (shipped) |
|---|---|---|
| hand residual | 0.00 uu | 0.02 uu |
| elbow fwd of shoulder | **-23.7** min | **+11.7** min |
| elbow above shoulder | +7.4 max | -8.4 max (always hanging) |
| elbow to chest centre | **10.1** min | **23.1** min |

The arm moves a long way - upper arm 95 deg mean, forearm 116 deg mean - because the battlecry's left
hand starts **106 uu** from the head. This is not a nudge to the source clip; the left arm is
entirely re-authored and only the rest of the body is the battlecry.

**Loop-safe:** the source's frames 0 and 85 already measure identically, the solve is applied at full
weight across every frame with no ramp, and the solved rotations at frame 0 vs 85 differ by 9e-6. So
the montage's self-loop has no seam at the wrap.

**Verified by round-trip:** re-read the SAVED asset through the engine's own sampler and re-ran the
FK - hand-to-mouth 0.01-0.02 uu at frames 0/21/43/64/85. Montage repoint verified by read-back after
save (the first repoint attempt silently did nothing: struct arrays marshal as copies, so mutating
the loop variable is discarded).

**NOBODY HAS SEEN THIS.** It is geometry that satisfies the constraints I set. Whether a goblin
holding a horn to its face at these offsets reads as *blowing* it is Michael's eye, and the two
numbers to turn are FWD_OFF and UP_OFF (currently 10 and 2), plus `HornMeshOffset` once the build
lands and the prop can actually be seen.

### Re-bake after Michael watched it: "bunched in on his face and twisted all around"

He was right and the first bake was wrong in three separate ways. All three are now fixed and each
was found by measurement, not by staring.

**1. Wrong target entirely.** The first solve aimed the **wrist bone** at the lips. The horn does not
hang off the wrist - it hangs off `hand_l_horn`, which sits **52.7 uu** further down the hand. The
horn's pivot is at its mouthpiece end (local X spans -2.2 .. 47.2), so the correct statement of the
problem is *"put the SOCKET on the lips"*, not the hand. Solving for the socket also fixes the
bunching for free, because the hand then falls back along the horn instead of being jammed at the
face.

**2. Folded elbow.** Shoulder-to-head is 31 uu against a 78.85 uu arm, so aiming the hand at the lips
forced the elbow to **47 deg mean, 33 deg min** - shut. Now 91 deg mean, 74.5 min.

**3. The twist, which was the real defect.** Only the forearm's DIRECTION was ever constrained. Roll
about that axis was whatever the minimal-swing solve happened to produce, `l_hand` still carried its
battlecry rotation, and the four twist bones still carried battlecry values. Nothing constrained the
wrist at all.

The fix is that roll is a **free parameter that decides where the hand - and therefore the elbow -
sits**, so it is swept rather than assumed. The sweep is unambiguous:

| roll | hand above shoulder | hand fwd of shoulder | reach used |
|------|--------------------|----------------------|------------|
| **0 (what v1 used)** | **+60.7 uu** | **-8.3 uu** | 79% |
| 90 | +38.9 | +8.0 | 108% (unreachable) |
| **210 (chosen)** | **-16.7 uu** | **+49.7 uu** | 72% |

Roll 0 put the hand five and a half inches ABOVE the shoulder and behind it. That is the twisted,
bunched arm, and it was a number I picked arbitrarily out of `cross(up, hornAxis)`.

`l_hand` is now explicitly oriented (horn axis out of the mouth at forward 0.80 / up 0.60, roll 210)
and the four twist bones are set to identity. Seven tracks written: `l_upperarm`, `l_forearm`,
`l_hand`, and the four twists.

**A guard was added and it earned its place.** The solve refuses to write unless the mouthpiece lands
within 0.5 uu, the elbow stays above 70 deg, in front of the shoulder, below it, and clear of the
chest. It **blocked the roll-0 attempt** - elbow +34.6 uu above the shoulder - which would otherwise
have been the second broken bake handed back for review.

**Verified by round-trip** through the engine's own sampler on the SAVED asset, 13 samples across the
clip: mouthpiece 0.02 uu from the lips on every one, elbow 84-104 deg, elbow below and in front of
the shoulder on every sample, loop delta 1.6e-5.

### Michael's socket - overwritten by me, restored

`hand_l_horn` is no longer the socket I authored: he re-posed it (loc 52.69 uu out, rot -40.33 /
163.22 / -175.12, **scale 2.0** - the horn was too small) against the idle pose. **I overwrote its
rotation with identity and saved before realising**, then restored it - and the first restore was
also wrong, because `unreal.Rotator()` takes positional args as **(roll, pitch, yaw)**, not
(pitch, yaw, roll), which shuffled the components. Restored correctly by setting each component by
name and verified to 6 decimal places.

The socket transform is **his** and is now treated as authoritative input to the solve, not something
this ticket owns. The 2.0 scale is why the horn is 98.8 uu long and why the 52.7 uu socket offset is
correct rather than suspicious: with the pivot at the mouthpiece, the grip only lands in the palm if
the mouthpiece sits about that far out.

### The forward axis was inverted. Every solve pointed the horn behind his head.

Michael recorded the Persona preview (`goblinhornblow.mp4`, 224 frames @30fps). Decoded it with
`imageio_ffmpeg` (which ships its own binary - there is no ffmpeg on PATH) and looked at the frames.
The horn was projecting up and BACKWARD over the shoulder like a bazooka, bell in the air, mouthpiece
nowhere near the mouth.

**Root cause:** `fwd = cross(left, up)`. In Unreal's handedness that is **backward**;
`cross(up, left)` is forward. Verified against an independent anatomical reference - the heel-to-toe
vector, which cannot be argued with:

```
cross(left,up) . toes = -0.997      <- what every solve used
cross(up,left) . toes = +0.997
```

**Why nothing caught it for four passes:** the only axis test ever run was orthogonality
(`up . fwd = 0.000`), and **orthogonality is blind to sign**. Every downstream metric inherited the
flip and therefore agreed with itself: "elbow 23-33 uu in front of the shoulder" was measured along a
backward vector, so the elbow was actually *behind* it, and the roll sweep picked 210 against
mirrored geometry. A self-consistent set of confident numbers, all wrong.

Fixed: forward is now `cross(up, left)` and the solve **asserts `dot(fwd, toes) > 0.5` on every
frame**, so this cannot silently flip again. Roll re-swept on corrected geometry: **180**, not 210.

New decisive metric, which should have existed from pass one:

| | v3 (corrected) |
|---|---|
| mouthpiece -> lips | 0.02 uu |
| **horn TIP forward of head** | **+85.5 uu** (was about -85) |
| elbow fwd of shoulder | +21.4 min |
| elbow above shoulder | -28.0 max (hanging) |

### A visual readout now exists, and it should have existed first

`pose_proj.json` + a side-view stick-figure render (`pose_check.png`) draws the skeleton, the left arm
and the horn as a line, from the SAVED asset, across all phases. It took minutes to build and it
shows the horn direction at a glance. Four broken bakes were handed over because there was no way to
LOOK at the result without asking Michael to record a video - the exact "build the instrument before
the feature" failure this project has hit before.

**Still unseen in motion by anyone.** Numbers and a stick figure are not the goblin.

### Montage sections: tested, not available. Three montages instead.

`GSGA_Block`'s comment says sections are unreachable from Python. That is about runtime CHAINING, so
authoring was tested rather than assumed - and it is also unavailable:

- `AnimMontage` exposes no writable section array. `composite_sections`, `sections`,
  `montage_sections`, `anim_composite_sections` all absent.
- `get_num_sections` / `get_section_name` / `get_section_index` are read-only accessors.
- `FCompositeSection` has no settable `StartTime` at all.

So the split lives in the ASSETS. `AM_GS_HornBlast` is duplicated into three, each retimed onto a
slice of `A_GS_HornBlow_Gob` and each carrying one `Default` section that the ability self-chains:

| montage | source slice | length | notes |
|---------|--------------|--------|-------|
| `AM_GS_HornBlast_Intro` | 0.000 - 0.800 | 0.800 | raise, plays once |
| `AM_GS_HornBlast_Loop`  | 0.800 - 1.600 | 0.800 | the hold, self-chained; `enable_auto_blend_out` OFF |
| `AM_GS_HornBlast_Outro` | 1.600 - 2.833 | 1.233 | lower, plays once |

**A trap worth recording:** retiming the segment does NOT update the montage's length.
All three still reported `sequence_length` 2.8333 after the segments were correct, and
`post_edit_change` does not exist on `AnimMontage` in Python. `controller.set_number_of_frames`
is what actually recalculates it. Left unnoticed, every montage would have run 2.83s of playback
over 0.8s of animation.

**The loop wrap is exact**, because the hold is a constant pose: key 24 vs key 48 measures
**0.0000 deg** on `l_upperarm`, `l_forearm` and `l_hand`. There is nothing to blend.

**UNCLAIMED FILES, declared rather than hidden:** the three `AM_GS_HornBlast_{Intro,Loop,Outro}`
assets are new and were not in this ticket's claim. Nothing else could have held them - they did not
exist - but the rule is claim-before-write.

### `GSGA_Horn` rewired (file claimed by #236, same agent)

`HornMontage` becomes `HornMontageIntro` / `HornMontageLoop` / `HornMontageOutro`. The animation now
has **the same three-part shape as the horn's voice**, which is deliberate: they are one event, and
drift between them would be visible.

- `StartBlast` plays the intro once and arms `MontageHandoffTimerHandle` at the intro's own
  `GetPlayLength()` - not a constant, so re-authoring the intro cannot freeze the goblin at the top
  of the raise.
- `BeginHornHold` starts the loop, but **only if the button is still down**. A tap shorter than the
  intro must not start a hold nobody asked for.
- `PlayLooping` is the shared self-chaining helper.
- `StopBlast` stops the active montage and **plays the outro unstopped, after the ability has already
  ended**. Releasing the button is the goblin lowering the horn, not the horn teleporting to his side.
- **The prop now hides on a timer at the end of the outro, not at EndAbility.** The horn is in his
  hand for the whole of that clip. `StartBlast` clears that timer first, so blowing again mid-outro
  does not hide the horn out from under the new blast.

**NOT COMPILED.** Build gate still closed. Declarations cross-checked against definitions by script;
that is a spell-check, not a compile.

> 2026-08-21T21:47Z BUILT 2026-08-21 14:41. All nine new symbols verified present in UnrealEditor-GoblinSiege.dll. Remaining: HornMesh must be set on the five DA_Weapon_* assets - needs the editor open - then observation.

### Michael watched it: "visible, but tiny and upside down"

**Root cause: the socket was on the wrong mesh, and the log had been saying so all along.**
`hand_l_horn` was authored on `GOB_Scout_v2`. The player uses **`GOB_Scout_v3`**, which lives in a
different folder entirely - `/Game/Characters/ScoutV3/` - and shares `GOB_Scout_v2_Skeleton`. With no
socket, `AttachWeaponMeshToSocket` took its documented fallback and attached the horn to the
CHARACTER ROOT, which means no socket rotation and no socket scale: exactly "tiny and upside down".

The fallback is deliberately loud and fired on every single blast:

```
socket 'hand_l_horn' does not exist on skeletal mesh 'GOB_Scout_v3'
 - attaching to the character root instead
```

**Nobody read it.** Four passes of geometry measurement, a video, and a stick-figure renderer, while
the answer sat in the log from the first blast. The instrument existed and was ignored.

This is also why the other six sockets work on both meshes: they live on the SKELETON.
`hand_l_horn` was mesh-local because the earlier add-then-prune attempt failed. Now moved to the
skeleton, verified resolving on both meshes at Michael's transform, saved 15:30.

**I destroyed his socket twice.** The second time was after saying I would not write to that mesh
again: `remove_socket` deletes EVERY copy of a name, mesh and skeleton alike, and the skeleton-outer
re-add then failed silently, so for two calls the socket did not exist. It was recoverable only
because his values had been read into the transcript minutes earlier.

**Outstanding wart:** copies now exist on `GOB_Scout_v2` AND the skeleton, currently identical.
**Author the socket on `GOB_Scout_v3`**, which only sees the skeleton copy. Editing it in the v2 mesh
editor edits the copy the game ignores - this bug again, in a subtler form.

### "The note sustains, but he doesn't hold a solid pose"

Right again, and the previous fix was half of one. Only the seven left-arm bones were frozen; the
other 34 kept the battlecry. Measured at the loop wrap (48 -> 24): `r_forearm` **20.6 deg**, both
clavicles about 10 deg, and the ROOT translating **9.5 uu** - every 0.8 seconds.

Fix computed and verified offline (`scratchpad/freeze_body.py`, cached to `anim_frozen.json`): all 41
bones take their frame-36 value across 24-48, so the wrap is the same frame twice - **0.000002 deg,
0.000000 uu**. The body still eases into the freeze over 16-24 and out over 48-56, both of which fall
inside the Intro and Outro montages, so the raise and lower keep their weight and each hands over to
the loop on an identical pose.

**NOT WRITTEN - the editor closed mid-write.** `A_GS_HornBlow_Gob.uasset` is still the 14:26
arm-only version. One call to apply once the editor is back.

Noted while measuring: the worst frame-to-frame in the clip is 55.5 deg on `r_forearm` at frames
15-16, and it is PRE-EXISTING in `A_MX_Taunt_Battlecry_Gob` - the shout wind-up Michael already
flagged as reading like a crouching lunge rather than a horn raise. Separate fix, body not arm.

### Michael: "it's good as it is for now" - horn parked, 2026-08-21

Final state watched and accepted: horn visible in the left hand at the right size and orientation,
note sustaining across a held blast, hold pose solid with no loop jitter.

The last two misses were both the same root cause and it is worth naming, because it will recur:
**the baked arm pose is coupled to the socket transform.** The solve places `hand_l_horn` at the
mouth; when Michael re-authors that socket the arm keeps holding where the socket USED to be, and the
horn drifts by exactly the distance he moved it. Measured twice: 67.8 uu of socket movement produced
67.8 uu of mouthpiece error, and separately a 100 degree tilt. Nothing warns anyone; it needs the
solve re-run.

Final numbers, verified off the saved asset:

| | |
|---|---|
| mouthpiece above the head bone | +28.0 uu (skull is 88.6 uu tall above that bone) |
| mouthpiece forward of the head bone | +16.0 uu |
| horn axis | +37 deg above horizontal |
| elbow forward / above shoulder | +40.6 / -15.0 uu |
| hold frozen, key 24 vs 30/36/42/48 | 0.000000 deg, all 41 bones |

**The two dials are `UP_OFF` = 28 and `FWD_OFF` = 16** (mouth position relative to the head bone).
The head bone sits at the TOP OF THE NECK - the original 2 uu was 2% up the skull, which is why
Michael reported "he's playing from his neck".

### FOLLOW-UPS, carried out of this ticket

1. **Decouple the horn from the socket.** Make its placement an animation-time concern so the socket
   and the pose cannot disagree. Ends the re-solve loop permanently. This is the real fix.
2. **Ownership of `A_GS_HornBlow_Gob`.** It is generated from a script today, so any hand-edit
   Michael makes is one re-solve away from being erased. Agree who owns it before either party
   edits.
3. **The raise inherits the battlecry's shout wind-up** - a 55.5 deg snap on `r_forearm` at frames
   15-16, PRE-EXISTING in `A_MX_Taunt_Battlecry_Gob`. Reads as a crouching lunge rather than raising
   a horn. Torso work, not arm.
4. **Remove the stray `hand_l_horn` copy on the `GOB_Scout_v2` MESH.** The authoritative copy is on
   `GOB_Scout_v2_Skeleton`. Until then, author the socket on **`GOB_Scout_v3`** - editing it in the
   v2 mesh editor edits the copy the game ignores. `remove_socket` deletes EVERY copy of a name, so
   this needs doing deliberately, not casually.

> 2026-08-22T01:22Z Evaluate rewritten post-build and post-observation.
