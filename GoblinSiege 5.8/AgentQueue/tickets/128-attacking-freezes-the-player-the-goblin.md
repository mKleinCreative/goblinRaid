---
id: 128
title: Attacking freezes the player: the goblin attack clips hand movement to root motion that contributes nothing
agent: claude-animsmooth
status: done
claimed: 2026-08-11T01:18Z
build: none
waiting_on:
evaluated: 2026-08-11T01:50:18Z
files: 
  - GoblinSiege 5.8/Content/Characters/ScoutV2/Anims_LocoSet/A_MX_Atk_Horizontal_Gob.uasset
  - GoblinSiege 5.8/Content/Characters/ScoutV2/Anims_LocoSet/A_MX_Atk_360High_Gob.uasset
  - GoblinSiege 5.8/Content/Characters/ScoutV2/Anims_LocoSet/A_MX_Atk_Combo3_Gob.uasset
  - GoblinSiege 5.8/Content/Characters/ScoutV2/Anims_LocoSet/A_MX_Atk_Downward_Gob.uasset
  - GoblinSiege 5.8/Content/Characters/ScoutV2/Anims_LocoSet/A_MX_Atk_Kick_Gob.uasset
  - GoblinSiege 5.8/Content/Characters/ScoutV2/Anims_LocoSet/A_MX_Block_Idle_Gob.uasset
  - GoblinSiege 5.8/Content/Characters/ScoutV2/Anims_LocoSet/A_MX_Block_ReactLarge_Gob.uasset
---

## Goal

Attacking freezes the player: the goblin attack clips hand movement to root motion that contributes nothing

## Generate

Michael: *"when you attack and jump it pauses your momentum. Also, you're not moving forwards as you
attack still. you pause then attack."*

**Both symptoms, one cause.** Three facts had to be read together:

| | |
|---|---|
| goblin ABP | `root_motion_mode = ROOT_MOTION_FROM_MONTAGES_ONLY` |
| goblin attack clips | `enable_root_motion = True` |
| goblin attack montages | `enable_root_motion_translation = False` |

A montage carrying root motion makes the character root-motion-driven for its duration. The montage
then contributes ZERO translation. The player is not slowed - the player is pinned, and
`MaxWalkSpeed` is irrelevant while root motion owns movement. Same reason a jump loses its velocity.

**This also explains why #125 appeared to do nothing.** Raising `MoveSpeedScale` 0.55 -> 0.85 was
treating a symptom that did not exist; the character was never moving at 55%, it was moving at 0.

The project already had the right convention and the attack clips violated it:

| pattern | montage translation | clip root motion | examples |
|---|---|---|---|
| root-motion moves you | True | True | ClimbHopUp, Dodge_*_RM, Vault_RM |
| input moves you | False | False | Dodge_Fwd, LandRoll, ClimbJumpOff |
| **broken middle** | **False** | **True** | **all 7 AM_GS_Atk_*, both Blocks** |

Set `enable_root_motion = False` on 7 clips: `A_MX_Atk_Horizontal_Gob`, `A_MX_Atk_360High_Gob`,
`A_MX_Atk_Combo3_Gob`, `A_MX_Atk_Downward_Gob`, `A_MX_Atk_Kick_Gob`, `A_MX_Block_Idle_Gob`,
`A_MX_Block_ReactLarge_Gob`. Verified beforehand that none is referenced by an `_RM` montage, so
nothing that relies on root motion lost it.

## Evaluate

**Verified by Michael in play: *"that fixed it, the movement feels much better now."*** All nine
attack and block montages re-read as internally consistent afterwards ("input driven").

**Deliberately not fixed, five remaining mismatches:** both `HitReact`s (a flinch pinning you is
arguably correct), `Mantle` and `Vault` (both have working `_RM` twins and belong to the climb system
that #067/#070/#079-082 invested heavily in), and `ThrowTorch` (same freeze applies to throwing, but
Michael did not report it and it is his call).

**Consequence to watch:** with root motion off, an attack contributes no animation-driven step, so
`LungeSpeed` is now the only forward push in a swing.

**The follow-on symptom this exposed, still open:** feet slide during swings. The montage is
full-body, so the attack's near-static legs override the run while the capsule moves. That is the
upper/lower split, and it is NOT done - see Refine.

## Refine

**The upper-body split was attempted twice tonight and reverted twice. Read this before trying again.**

The intended shape is the one `ABP_Human` already uses: `Slot 'DefaultSlot'` -> `SaveCachedPose`,
then two `UseCachedPose` feeding a `LayeredBoneBlend`'s BasePose and a `Slot 'UpperBody'`. Pose pins
are one-to-one in an AnimGraph, hence the cached pose.

Every attempt produced, on compile:
`[Compiler] Missing allocated node for AnimGraphNode_BlendSpacePlayer_0 while searching for node
links - likely due to the node having outstanding errors.`

**Three hypotheses tested and all three WRONG:**
1. *Empty `LayerSetup` on the blend node makes it an error node.* Wrong - Michael set the branch
   filter to `spine01` by hand and the error was identical.
2. *The script-created cached-pose nodes do not resolve their name.* Wrong - they read
   `Use cached pose 'GobLocoPose'`, the same shape as ABP_Human's working ones.
3. *The `LayeredBoneBlend` node itself is the trigger.* Wrong - after deleting it and returning the
   graph to its original six nodes, the error still occurred.

**What IS established:** restoring the .uasset from git produces a clean compile every time; editing
the nodes back does not. The package retains something that node-level undo does not clear. So the
fault is in what script-driven graph surgery does to the PACKAGE, not in the graph shape.

**Therefore: do not build this split with `AnimGraphService`.** Author it by hand in the editor, or
find out first why a script-edited AnimGraph package fails to compile even after the nodes are
removed. The asset has been restored to 688ad92 and verified byte-identical; Michael's
hand-configured blend node is preserved at
`scratchpad/ThirdPerson_AnimBP_Gob.WITH_MICHAELS_BLEND_NODE.uasset` if it is wanted back.
