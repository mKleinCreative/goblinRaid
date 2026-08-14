---
id: 112
title: Humans get combat animations: retarget the goblin montage set onto SK_Human_Skeleton
agent: claude-gobkit
status: done
claimed: 2026-08-10T03:31Z
build: none
waiting_on:
evaluated: 2026-08-10T04:01:22Z
files: 
  - Content/Characters/Humans/Retarget
  - Content/Characters/Humans/Anims_Combat
  - Content/Blueprints/Abilities/Human
---

## Goal

Humans get combat animations: retarget the goblin montage set onto SK_Human_Skeleton

## Generate

The real answer to #101: the humans had no combat animations because every montage in the project is
on `GOB_Scout_v2_Skeleton`.

**Retarget pipeline, all built from Python:**
- `IK_Goblin` and `IK_Human` (`/Game/Characters/Humans/Retarget/`), each via
  `apply_auto_generated_retarget_definition()`. All 11 source chains came out with names matching the
  target - Spine, Neck, Head, and the four limb/clavicle pairs. The human has 4 extra finger chains
  with no source, which simply do not retarget.
- `RTG_Goblin_To_Human`, chains auto-mapped. Verified by `get_source_chain`: Spine<-Spine,
  RightArm<-RightArm, LeftArm<-LeftArm, Head<-Head.
- **11 combat montages retargeted** to `/Game/Characters/Humans/Anims_Combat/` as `AM_HU_*`, pulling
  9 referenced sequences with them. All 20 assets verified on `SK_Human_Skeleton`, 0 wrong.
- **4 human ability variants** (`GA_HU_SwordLight/SwordHeavy/GuardBreak/Block`) with the retargeted
  montages swapped in. Duplicated rather than edited in place because the originals are **shared with
  the player** - editing them would have put human animations on the goblin.
- **All 6 human Blueprints** wired to those abilities, plus `HitReactFront/Left/Right` and
  `BlockReact`, which had been **None on every human since the project started**.

## Evaluate

**The C++ skeleton guard from #101 now does exactly what it should**, confirmed on a live guard:

```
goblin montage on a human -> 0.000    (still refused - the guard still works)
HUMAN  montage on a human -> 1.150    (accepted, because the skeletons now match)
```

**IT STILL DOES NOT ANIMATE, AND I KNOW WHY.** With the montage reporting `playing=True`, the guard's
hand pose was **byte-identical across samples** - (45.0, -92.7, -14.4) twice running. `ABP_Human`'s
AnimGraph has **no Slot node**, so a montage plays into a slot nothing reads. That is the last link in
the chain and it is not done.

**I broke ABP_Human twice trying to add it, and the second time I caught it before saving.**
`AnimGraphService.add_slot_node` + two `connect_anim_nodes` calls all return True, and the Blueprint
then compiles to `BS_ERROR`. The first attempt was saved by the editor on shutdown and I reverted it
with `git checkout`; the on-disk asset is back to the committed, compiling version (verified
`BS_UP_TO_DATE`, AnimGraph node count 8).

**A process failure worth naming:** I saved that first attempt without checking the compile result.
The second attempt gated on `Blueprint.status == BS_UP_TO_DATE` before saving, which is what caught it.
That check should have been there from the first call.

**Also lost and re-applied:** killing the wedged editor discarded the Blueprint wiring even though the
save call had returned. Re-applied and re-verified from freshly loaded CDOs - all six now read
`GA_HU_SwordLight_C` and `AM_HU_HitReact_Front`.

## Refine

**Duplicated the abilities instead of retargeting in place.** The montage fields live on abilities the
player shares; a blanket swap would have animated the goblin with human motion.

**Mapped `hit_react_right` to the LEFT montage.** There is no right-hand variant in the source set. A
mirrored flinch is wrong-footed; no flinch at all is worse.

**LEFT UNDONE, and it is the one thing between this and working:** one Slot node in `ABP_Human`'s
AnimGraph, between the Locomotion state machine and the Output Pose, slot name `DefaultSlot` (which is
what every retargeted montage uses - verified on `AM_HU_Atk_Light`). Fifteen seconds by hand in the
Anim Blueprint editor; three failed attempts through the service. Handed to Michael rather than
burning more of his session on it.
