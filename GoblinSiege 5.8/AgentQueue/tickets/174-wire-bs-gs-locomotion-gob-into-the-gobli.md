---
id: 174
title: BS_GS_Locomotion_Gob measured dead in PIE and reverted - #133's blend-surface fix was never applied to it
agent: claude-axe
status: done
claimed: 2026-08-17T23:55Z
build: none
waiting_on: 
evaluated: 2026-08-18T06:25:33Z
observed: 2026-08-18T00:55:08Z | After the revert I watched the same fight again: all ten horn-summoned goblins and the player were animating normally, walking and turning under the AI, with the six human defenders animating alongside them as before. Nothing was frozen. GS.Anim.Snapshot counted 17 pawns with 0 in reference pose, and the goblins were genuinely in motion when sampled (speeds 5 to 124, directions -72 to +89) rather than standing still, so it is not a stationary false pass.
scenario: PIE on L_CombatArena with GS.Horde.SpawnTest, same ten goblins plus player plus six human defenders as the pre-revert run
files: 
  - Content/Characters/ScoutV2/Animations/ThirdPerson_AnimBP_Gob.uasset
---

## Goal

Wire BS_GS_Locomotion_Gob into the goblin AnimBP (Direction to X, Speed to Y) - SAVED BUT RUNTIME-UNVERIFIED

## Generate

**Net file change: none. This ticket ends where it started, and that is the result.**

The goblin has no strafe and no backward locomotion; `BS_GS_Locomotion_Gob` (8-way, built in
`965c980`) sits in the project referenced by nothing. This ticket tested whether it is usable.

Wiring, in `ThirdPerson_AnimBP_Gob`, graph `Idle/Run`, via `unreal.AnimGraphService`:
- `set_blend_space_asset` on node `10B7A22F...` -> `BS_GS_Locomotion_Gob`
- `disconnect_anim_node` the old `Get Speed` -> X; `connect_anim_nodes` `Direction` -> X, `Speed` -> Y

**No nodes were created.** The node is a generic `AnimGraphNode_BlendSpacePlayer` that already had
X/Y/Pose pins, and #133 had left a disconnected `Get Direction` node in the graph. That keeps this
outside #128's blast radius, which is about adding/rewriting AnimGraph structure.

## Evaluate

**The asset is dead. Measured, in PIE, not inferred.** `GS.Anim.Snapshot`: 17 pawns, **11 in
reference pose** - all ten horn-summoned goblins *and* the player, every one on
`ThirdPerson_AnimBP_Gob_C`; all six humans on `ABP_Human_C` read `no`. Several frozen goblins were
reading real movement (speeds 91-182, directions -71 to +129) and still did not move a bone.
Worse than #137's 9 of 15 on this same asset.

**What this ticket proves that #137 did not:** #137 used the asset as a known-dead control. This
run establishes it is *still* dead **after** #133's Refine, and that every readable repair marker is
present anyway - 5 direction columns including an explicit +/-180, speed axis 0..800 with the sprint
row, `axis_to_scale_animation = BSA_Y`, `rate_scale` 1.000 on all 20 samples,
`target_weight_interpolation_speed_per_sec` 5.0 matching the known-good asset. So #133's fixes 1-3
landed and **fix 4 did not**.

**The trap, stated precisely for the next agent.** #133's fix 4 is
`notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS` - an argument passed to
`set_editor_property` *at write time* to fire `PostEditChangeProperty`, which is what builds the
blend surface. It is **not** a stored property and it is **not** the asset's `notify_trigger_mode`
(I conflated the two earlier; `notify_trigger_mode` reads `HIGHEST_WEIGHTED_ANIMATION` on the
known-good asset too, so it is not a defect marker). **There is no way to read whether the blend
surface exists.** A structural read-back of samples, axes, rates and skeleton passes completely on a
corpse - I ran exactly that check and called the asset sound, and it was not.

**What I got wrong, on the record:** I reported this asset as "well-formed, not dead" from a
sample/axis read-back, contradicting the ticket record. The ticket record was right.

**Touched outside the goal:** nothing. But the *ordering* was wrong - the AnimBP was edited before
this ticket existed, under #171 which claimed only the two weapon data assets. Claim first.

**Also worth recording:** driving PIE and loading maps here disrupted #173 `claude-grapple`, who was
active in the same editor. No data was lost (dirty package count was zero throughout), but the board
should be read before taking the editor.

## Refine

**Reverted in-editor, not by git.** `git restore` fails while the editor holds the package
(`unable to unlink ... Invalid argument`), and blanking the map plus `collect_garbage` does not
release it. So the graph was put back through the same service calls: asset -> 
`ThirdPerson_IdleRun_2D_Gob`, `Speed` -> X restored, `Direction` and the second `Get Speed`
disconnected, Y left unconnected. Read-back confirms the wiring matches the pre-change state
exactly; recompiled `BS_UP_TO_DATE` and saved.

**Note for whoever closes this:** the file will still show modified in `git status` because a
re-saved `.uasset` is not byte-identical to HEAD. A true `git restore` needs the editor closed, and
is worth doing to keep the diff clean.

**Revert CONFIRMED IN PIE (2026-08-18T00:53Z).** Re-ran the same scenario after the revert:
`GS.Anim.Snapshot` reads **17 pawns, 0 in REFERENCE POSE**. All ten horn-summoned goblins and the
player animate normally, and this time they were genuinely moving when sampled (speeds 5-124,
directions -72 to +89), so it is not a stationary false pass. The revert is good and the goblins are
back on their original locomotion.

**SUPERSEDED, 2026-08-18 (#182).** The revert described above was correct at the time, but the
AnimBP no longer points at `ThirdPerson_IdleRun_2D_Gob`. Michael hand-authored a new 2D blendspace,
`BS_GS_Loco_Pack`, built on retargeted CombatMasterBundle clips, and it is now wired into this same
`Idle/Run` node and **works** - `GS.Anim.Snapshot` reads 0 of 17 in reference pose with goblins
sampling right across the Direction axis. So the "next attempt" below actually happened, and the
answer was the second option: author it by hand.

`BS_GS_Locomotion_Gob` is still a corpse and is still referenced by nothing. It should be deleted
rather than left as a trap for the next agent who reads its samples back and concludes it is fine -
which is exactly the mistake recorded above.

**What the next attempt must do:** rebuild the blend surface, not the samples. Either re-write
`sample_data` / `blend_parameters` / `axis_to_scale_animation` with `notify_mode=ALWAYS`, or author
the asset by hand in the editor per AGENT_STATE. Then verify with `GS.Anim.Snapshot` on horn-summoned
goblins - **REFPOSE must read 0 of N** - and never on the player pawn alone, whose
`bOrientRotationToMovement` pins Direction near zero and samples one column of five.
