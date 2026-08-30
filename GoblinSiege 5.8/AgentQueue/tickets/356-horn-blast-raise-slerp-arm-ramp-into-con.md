---
id: 356
title: Horn blast raise: slerp arm ramp into confirmed frame-29 hold pose
agent: claude-anim
status: abandoned
claimed: 2026-08-29T19:18Z
build: none
waiting_on: Paused by Michael to work other priorities. Flatten-the-loop attempt is failing at the tooling level (see ticket body) - needs either a working Python fix or Michael manually flattening frames 24-48 in the editor before this can close.
evaluated: 2026-08-29T19:46:54Z
observed:
scenario:
files: 
  - Content/Characters/ScoutV2/Anims_LocoSet/A_GS_HornBlow_Gob.uasset
  - Content/Characters/ScoutV2/Montages/AM_GS_HornBlast_Intro.uasset
---

## Goal

Horn blast raise: slerp arm ramp into confirmed frame-29 hold pose

## Generate

Confirmed with Michael before editing: the defect is the "wrong gesture path" (the raise still
reads as `A_MX_Taunt_Battlecry_Gob`'s taunt flourish, not a clean lift-to-mouth), and the ramp
should start from the sequence's own frame 0 (not the character's separate idle pose) and land on
frame 29, which he confirmed by eye as the correct held horn-at-mouth pose.

Edited `A_GS_HornBlow_Gob` (`/Game/Characters/ScoutV2/Anims_LocoSet/A_GS_HornBlow_Gob`) via
`AnimSequenceService`. The horn is held in the LEFT hand (`GSWeaponDataAsset::HornSocket =
"hand_l_horn"`), so the left-arm chain is what was re-keyed: `l_clavicle`, `l_upperarm`,
`l_upperarmtwist01/02`, `l_forearm`, `l_forearmtwist01/02`, `l_hand` (8 bones).

For each bone, captured the ORIGINAL local-space rotation quaternion at frame 0 and frame 29
(both left untouched), then for frames 1-28 replaced whatever the source taunt clip had with a
quaternion `slerp(q0, q29, frame/29)`, written back with
`AnimSequenceService.apply_bone_rotation(path, bone, rot, "local", frame, frame, is_delta=False)`
— one absolute-rotation call per frame per bone (224 calls, 0 failures). Saved via
`EditorAssetSubsystem.save_loaded_asset`.

This only touches frames 0-29 (the intro raise + first 5 frames of the loop's already-static
hold). Frames 29-85 (loop + outro) were never touched and remain the pre-existing constant pose
- confirmed identical at frames 24/29/48 before editing, so there was and is no discontinuity at
the Intro->Loop montage handoff itself; the fix is entirely about the SHAPE of the raise.

## Evaluate

**Verified (numeric, not visual):** re-sampled `l_upperarm` and `l_hand` euler rotation at frames
0/6/12/18/24/29 after the edit - both now show a single monotonic arc from the frame-0 start to
the frame-29 target (e.g. `l_hand` pitch: -40 deg -> -23 -> -4 -> 15 -> 34 -> 49 deg), replacing
the old non-monotonic wobble that came from the taunt clip. `AnimSequenceService.validate_pose`
against a learned skeleton profile reports `is_valid: True` - no constraint violations, nothing
clamped.

**NOT verified - this is a "built", not a "seen":** I could not get a real per-frame visual check.
`CaptureViewport` in PIE only frames the lower body/legs no matter how I positioned
`captureTransform` (spent several tries), and `CaptureAssetImage` on the anim sequence returned
the IDENTICAL PNG regardless of `set_preview_time` - it is reading a cached asset thumbnail, not a
live scrub, so those four screenshots are not evidence of anything. I did activate `UGSGA_Horn` on
the live PIE player pawn via `TryActivateAbilityByClass` and confirmed it runs (no error), but
nobody has watched the actual motion play.

Per QUEUE.md rule 4, this is exactly the kind of thing that needs a human to watch it run, not a
number - "does this read as raising a horn" is a judgment call, not something bone-angle sampling
can confirm on its own.

## Refine

Michael watched it live (2026-08-29): "it still holds it up to his neck, and does a double start,
it looks like one of the animations in the montage isn't clipping properly at the end."

**Diagnosed the double-start:** `AM_GS_HornBlast_Intro` plays sequence frames 0-24
(0.0-0.8s) and `AM_GS_HornBlast_Loop` plays frames 24-48 (0.8-1.6s) - confirmed earlier from
`anim_start_time`/`anim_end_time` on the montages' slot segments. The first pass slerped all the
way to frame 29, which is 5 frames INSIDE the Loop segment, not at the Intro/Loop boundary (frame
24). Before any of this ticket's edits, frames 24-85 were bit-identical (the loop was a frozen
hold, verified). The first pass left frames 25-28 mid-ramp instead of static, so the Loop segment
itself now had motion in it - when the engine handed off from the Intro montage to the Loop
montage at frame 24, the tail of the raise effectively played again inside the loop. That reads
exactly as "a double start."

**Fix applied:** re-keyed the same 8 left-arm bones. Frames 1-23 now slerp from the frame-0 start
to the (unchanged, still original) frame-29 target, arriving fully AT frame 24 instead of 29.
Frames 24-28 are pinned back to the exact target rotation, restoring the loop to a frozen hold
matching frames 29-85 (verified: frame 24 == frame 26 == frame 29 == frame 40, exactly, after the
fix). The visible raise now completes within the Intro segment's own length, so the Intro->Loop
handoff has zero motion either side of it - the same property the ORIGINAL unedited asset had
before this ticket touched anything.

**"Holds it up to his neck"** - Michael's own read: this is not a bug, it is because there is no
node/socket to aim at the mouth (the skeleton has nothing there to target), so "the neck" is as
close as the rig can currently get without adding one. Not something this ticket fixes.

**Second round - stutter NOT fixed by the frame-24 boundary change.** Michael watched again:
"stutter isn't gone... plays to the beginning where goblin is blowing out of the horn, then he
brings it down again about half way and continues with the horn blast into his neck." That is an
up-down-up pattern the arm keyframes cannot produce (the slerp is provably monotonic, verified by
sampling). Loaded the VibeUE `animation_montage` skill (had only loaded `animation_editing` before
- wrong domain for this bug) and used `AnimMontageService.get_montage_info` to read the actual
blend settings, which found the real cause:

`AM_GS_HornBlast_Intro` and `AM_GS_HornBlast_Loop` both have `blend_in_time=0.25`,
`blend_out_time=0.25`, `blend_out_trigger_time=-1` (auto = blend-out starts automatically
`blend_out_time` before the montage's own natural end, independent of anything else happening).
`UGSGA_Horn::StartBlast()` times the Intro->Loop handoff off `Intro->GetPlayLength()` - the FULL
0.8s - so the real weight timeline on the arm slot was:
- 0.0-0.55s: Intro at full weight (the raise, now correct)
- 0.55-0.8s: Intro auto-blending OUT toward idle (nothing else active on the slot yet - arm sinks
  back down)
- 0.8s: Loop starts, blends IN from idle over its own 0.25s, pulling the arm back up to the hold

That is exactly "plays to blowing, comes down about halfway, continues into the neck" - a
fade-to-idle-then-back-up, not a keyframe problem. Note (2026-08-27 tooling incident, unrelated to
the bug itself): `get_montage_info`/`does_asset_exist` briefly returned `None`/`False` for assets
that definitely exist, including the one I'd just saved - traced to PIE still being running from
an earlier live test blocking the asset registry (matches the "parallel sessions" gotcha re:
`save_asset` failing during PIE). `StopPIE` before further asset-registry queries fixed it; noting
in case another agent hits the same false "asset missing" read.

**Fix applied (Michael's choice - asset-only, not the C++ handoff-timing option):** shrank
`AM_GS_HornBlast_Intro`'s `blend_out_time` from 0.25s to 0.05s via
`AnimMontageService.set_blend_out(path, 0.05, "Linear")`, saved. The auto-blend-out-to-idle window
is now ~50ms (about 1.5 frames at 30fps) instead of 250ms, which should read as imperceptible
rather than eliminated. If it is still visible, the real fix is changing
`UGSGA_Horn::StartBlast()` to fire the Loop hand-off timer at `(Handoff - blend_out_time)` instead
of the full `Handoff`, so Loop's blend-in genuinely crossfades against Intro's blend-out instead of
racing a fade-to-idle - that is a C++ change plus a full rebuild, deliberately not taken this round
since Michael asked for the asset-only shrink first.

> 2026-08-29T22:11Z Iceboxed 2026-08-29 - see AgentQueue/ICEBOX.md #356. Work is real but UNCOMMITTED (unlike the file convention this entry describes).
