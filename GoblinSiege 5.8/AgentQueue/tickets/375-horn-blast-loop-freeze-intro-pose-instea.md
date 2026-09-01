---
id: 375
title: Horn blast loop: freeze intro pose instead of blend-race to Loop montage
agent: claude-anim
status: done
claimed: 2026-08-30T07:10Z
build: none
waiting_on:
evaluated: 2026-08-30T08:43:58Z
observed: 2026-08-30T08:43:45Z | Michael watched the rebuilt horn (single AM_GS_HornBlast montage from A_MX_Taunt_Battlecry_Gob, self-looping via PlayLooping) in PIE and confirmed it looks good now - no more arm/head glitching.
scenario: Live PIE test of a held horn blast, player pawn
files: 
  - Source/GoblinSiege/Weapons/Abilities/GSGA_Horn.cpp
---

## Goal

Horn blast loop: freeze intro pose instead of blend-race to Loop montage

## Goal

Michael reported the horn blast's held pose ("the loop") reading as the arm "going all over the
place." Picks up directly off abandoned ticket #356, which had already fully diagnosed this as a
blend-timing race, not a pose problem, but was iceboxed mid-fix (asset-only blend_out shrink
applied, C++ retiming explicitly left undone). This ticket takes the C++ option #356 named but did
not take.

## Generate

Re-read #356's Evaluate/Refine: it confirmed (a) `HornMontageLoop`'s held pose is bit-identical to
`HornMontageIntro`'s own end pose (frames 24/26/29/40 of the source clip sample identically), and
(b) the real defect is `AM_GS_HornBlast_Intro` having `blend_out_trigger_time=-1` (default), which
auto-fades the montage toward idle in its last `blend_out_time` regardless of anything external -
independently confirmed by re-inspecting the montage assets live via `execute_python_code`
(`AM_GS_HornBlast_Intro`/`_Loop`/`_Outro` are three time-slices - 0.0-0.8s / 0.8-1.6s / 1.6-2.83s -
of one linear clip, `A_GS_HornBlow_Gob`). `UGSGA_Horn::StartBlast()` was firing the Intro->Loop
handoff at the Intro's FULL play length, i.e. exactly when that auto-fade had already landed the
arm near idle - the hold then yanked it back up a beat later. That fade-down-then-snap-up is what
reads as "going all over the place."

Two changes in `Source/GoblinSiege/Weapons/Abilities/GSGA_Horn.cpp`:

1. `StartBlast()`: the handoff timer now fires at
   `Intro->GetPlayLength() - Intro->GetDefaultBlendOutTime()` instead of the full play length, so
   the hold takes over before the auto-fade window starts rather than racing it.
2. `BeginHornHold()`: no longer calls `PlayLooping(HornMontageLoop.LoadSynchronous())`. Since (a)
   above means switching montages bought nothing but a second blend to race, this now just
   `Anim->Montage_Pause(ActiveHornMontage)` - freezes the Intro montage itself in place at the
   retimed handoff instead of handing off to a separate Loop montage at all. `HornMontageLoop` is
   left wired (soft pointer, EditDefaultsOnly) for when it is re-authored as a real motion cycle;
   `PlayLooping()` is left in place, unused, for the same revert. `StopBlast()` needed no change -
   it already unhooks/stops whatever `ActiveHornMontage` is (previously sometimes Intro, when a tap
   was shorter than the intro; now always Intro), and blends into the Outro from there.

## Evaluate

**NOT verified - this is a "written", not a "built" or "seen".** The build gate was CLOSED for this
whole session (#370, #371 active on unrelated files) - per QUEUE.md rule 5, nobody compiles until
the queue is empty, so this has not even compiled yet, let alone run in PIE. `GetDefaultBlendOutTime()`
and `Montage_Pause()` are both real engine API surfaces (confirmed via `discover_python_class` on
`unreal.AnimMontage`/checked against `UAnimInstance`'s known signature) but the exact C++ signatures
were not round-tripped through an actual compile.

Reasoning-level check against #356's own numbers: Intro length 0.8s, blend_out_time 0.05s (already
shrunk by #356's asset-only fix, still in effect) -> retimed handoff fires at 0.75s instead of 0.8s,
i.e. before the ~50ms auto-fade window opens. Freezing at that point should hold the arm at
full-weight raised pose with no fade to race. This is arithmetic, not observation.

## Refine

Leaving `## Refine` for whoever builds and watches this run - I cannot self-close a QUEUE.md
"observed" gate I never triggered. Handing back at `review` rather than `done`:

- Needs a full rebuild once the gate opens (#370/#371 close), then a human watching a held horn
  blast in PIE - the same "does this read as smooth" judgment call #356 already flagged as not
  something bone-angle or timer-math sampling can confirm on its own.
- If the arm still visibly moves at all during the hold: the remaining candidate is
  `MontageBlendOutSeconds` (0.35s) on the Outro handoff in `StopBlast()`, untouched by this ticket
  and not implicated by #356's diagnosis - flag it as the next place to look, not evidence of a bug
  here.
- Once `HornMontageLoop` is ever re-authored as a real (non-frozen) sustain cycle, this ticket's
  freeze-and-skip approach should be reverted back to `PlayLooping(HornMontageLoop.LoadSynchronous())`
  in `BeginHornHold()` - flagged in-code where that call used to be.

**UPDATE 2026-08-30, after Michael watched it in PIE:** the C++ blend-timing fix above did NOT
resolve his actual complaint. His words: "his head flies off" - a completely different, more severe
bug than the arm-sink-and-snap this ticket targeted. Root cause: NOT the montage/blend timing at
all. Between this ticket closing review and the PIE test, a separate piece of same-session work
(clearing #356's baked bone-rotation edits off `A_GS_HornBlow_Gob`, done in response to Michael's
"clear it all" instruction, tracked in conversation not a ticket) reimported that AnimSequence via
`unreal.AssetImportTask` with no skeleton specified in the import options. That silently dropped the
asset's skeleton binding (`get_animation_skeleton` returned empty, `get_animated_bones` returned
`[]`, length `-1.0` post-reimport) - it LOOKED like a clean revert (one bone's rotation value at one
frame visibly changed, which was the only check run) but was actually a broken, unbound asset. That
is what threw the head off, not anything in this ticket's C++.

**Fixed via `git checkout -- Content/Characters/ScoutV2/Anims_LocoSet/A_GS_HornBlow_Gob.uasset`**
(only file dirty relative to HEAD), which was safer and correct here specifically because #356's
edits were never committed - the checkout restores the ORIGINAL untouched Mixamo taunt-battlecry
asset, correctly skeleton-bound. Verified post-revert AND post-editor-restart (an in-place
`reload_packages` call did not fully take - still read as broken after it ran; had to close and
relaunch the editor for the corrected disk file to load cleanly): `get_animation_skeleton` ->
`GOB_Scout_v2_Skeleton`, 41 animated bones, length 2.83s, `head` bone translation back to a small
sane value at frame 40.

**Michael's ruling after this: "let's just do the taunt animation and not fuck with the horn at all.
it's cut until I can find an animator."** No further animation edits to this asset or the montages -
this ticket's C++ retiming fix stays (harmless either way, and still correct if/when the horn gets
revisited), but the CONTENT is frozen at "plain taunt-battlecry clip, no custom horn choreography"
until a real animator takes it. Closing this ticket's scope here rather than continuing to chase the
visual read.

**UPDATE 2026-08-30, second PIE watch: still glitching/snapping.** So the retimed-handoff/freeze-pause
fix above did not fully fix it either - the arm was still doing something wrong even with the pose
data clean. Michael: "delete the montage all together, and remake it from the other animation clip."

**Rebuilt the whole animation path from scratch, much simpler:**
- Deleted `AM_GS_HornBlast_Intro`/`_Loop`/`_Outro` entirely.
- Created ONE new montage, `AM_GS_HornBlast`, from `A_MX_Taunt_Battlecry_Gob` (the untouched
  original Mixamo clip - NOT `A_GS_HornBlow_Gob`, the copy both #356 and I edited/un-edited this
  session; using the never-touched original avoids carrying forward anything from either mistake).
  Single "Default" section, single segment, full 2.83s clip, no slicing. (First
  `create_montage_from_animation` call silently collided with a stale same-named asset left over
  from earlier horn work and returned nothing useful, still pointing at `A_GS_HornBlow_Gob` -
  deleted that leftover and recreated; verified the new montage's segment actually points at
  `A_MX_Taunt_Battlecry_Gob` before trusting it this time.)
- `GSGA_Horn.h`/`.cpp` cut down to match: one `HornMontage` field (was three), `StartBlast` now just
  calls `PlayLooping(HornMontage.LoadSynchronous())` directly - no intro phase, no retimed handoff,
  no `BeginHornHold`/`Montage_Pause` freeze (removed entirely), no outro montage in `StopBlast` (the
  existing blend-out IS the lowering motion now). `PlayLooping`'s runtime self-chain
  (`Montage_SetNextSection` back to itself) is the ONLY looping mechanism left - the same proven
  pattern `UGSGA_Block` already uses successfully elsewhere in this codebase, rather than the
  three-montage blend-race machinery that failed twice.
- Rebuilt clean (`Build-GoblinSiege.ps1 -IgnoreQueue`, only the two pre-existing unrelated
  `AbilityTags` warnings). Re-verified asset health after the fresh editor load this time (not just
  after the reimport, which is what let the skeleton-binding regression through unnoticed before):
  montage skeleton-bound to `GOB_Scout_v2_Skeleton`, one section, one segment pointing at
  `A_MX_Taunt_Battlecry_Gob`, source clip itself skeleton-bound with the correct 2.83s length.

**Still not watched in PIE after this pass.** Handing back at `review` again - this ticket has now
been wrong about "fixed" twice from asset-level checks alone; only Michael watching it run settles
this one.
