---
id: 134
title: Eyes sit outside the head on Erika and the Knight: per-character bind pose is discarded by Animation translation retargeting
agent: claude-eyes
status: review
claimed: 2026-08-12T02:38Z
build: none
waiting_on:
evaluated: 2026-08-12T02:44:50Z
files: 
  - Content/Characters/Humans/SK_Human_Skeleton.uasset
  - GoblinSiege 5.8/AGENT_STATE.md
---

## Goal

Eyes sit outside the head on Erika and the Knight: per-character bind pose is discarded by Animation translation retargeting

Michael, 2026-08-11, while reviewing #133: *"the eyes are out of the head for some of the human
models (Erika Archer, the knight)."*

Raised during #133 and deliberately given its own ticket: it is a rigging defect, it long predates
that work, and #133's human locomotion was reverted to its original state machine before this was
noticed - so the human animation path at the time of the report was byte-identical to what it had
been all week.

## Generate

### Why only those two characters

Every human defender wears a `_baked` mesh re-bound from its own per-character skeleton
(`SK_ErikaArcher_Skeleton`, `SK_KnightDPelegrini_Skeleton`, ...) onto the single shared
`SK_Human_Skeleton`. Measured eye placement, expressed against each rig's own head size so the
comparison survives the fact that the shared rig is roughly 1.8x the size of the source rigs:

| rig | head Z | eyes above Head | eyes forward of Head | eye X |
|---|---|---|---|---|
| `SK_Human_Skeleton` (shared) | 310.71 | **0.185** of head height | 11.76 | +4.80 / **-5.79** |
| ErikaArcher (own) | 159.93 | **0.365** of head height | 9.23 | +3.07 / -3.06 |
| KnightDPelegrini (own) | 185.98 | - | - | **`LeftEye` absent** |
| CastleGuard01 (own) | 170.19 | **no eye bones at all** | - | - |
| CastleGuard02 (own) | 178.23 | **no eye bones at all** | - | - |

Two facts fall out of that table and together they explain the whole report:

1. **The guards have no eye bones**, so their eye geometry is rigidly weighted to `Head` and cannot
   be displaced by anything. That is why only Erika and the Knight are wrong - it is not that those
   two are broken, it is that they are the only two with eye bones to get wrong.
2. **Every bone on `SK_Human_Skeleton`, eye bones included, was on `Animation` translation
   retargeting.** That mode takes the bone's translation from the animation / shared reference pose
   and throws away the target mesh's own bind pose. Erika's eyes belong at 0.365 of her head height;
   the shared rig places them at 0.185 and 2.5uu further forward. Her eye geometry is therefore
   posed at coordinates that belong to a different skull, and comes out through the face.

The shared rig's eyes are also **asymmetric** (+4.80 vs -5.79), which no real head is, and the
Knight's source rig is missing `LeftEye` entirely - both signs that `SK_Human_Skeleton`'s head chain
was inherited from whichever character was imported first rather than authored.

**This is the same root cause as #133's strafe-import collapse**, where the identical `Animation`
setting applied a 0.6x-scaled rig's translations verbatim and telescoped the whole skeleton. That
ticket dodged it by retargeting through `RTG_MixamoToHuman` instead of touching the skeleton. Here
there is nothing to dodge: the defect IS the retargeting mode.

### The change

`Head`, `LeftEye`, `RightEye`, `HeadTop_End` set to **`Skeleton`** translation retargeting on
`SK_Human_Skeleton`. In that mode UE takes the translation from the target mesh's own bind pose and
only the rotation from the animation, so each character's eyes sit where that character's mesh puts
them, while the head still animates.

**Head chain only, on Michael's call.** `Hips`, `Spine`, `Neck`, the limbs and the hands are
untouched and remain `Animation`. That is the smallest change that can address the report, and it
deliberately cannot regress the body locomotion that had just been signed off in #133.

## Evaluate

**NOT VERIFIED BY EYE. Nobody has looked at Erika since the change.** The mechanism is measured and
the arithmetic is above, but the entire claim is "the eyes will now be posed from each mesh's bind
pose instead of the shared one", and whether that puts them in the right place depends on those bind
poses being correct - which I have not measured, because the baked meshes' bind data is not readable
through the tools available here.

**What is verified:**

- The eye/head placement table above: read off five skeletons.
- The guards genuinely have no eye bones: the probe returns identity for `LeftEye`/`RightEye` on both
  `SK_CastleGuard01_Skeleton` and `SK_CastleGuard02_Skeleton`, and real, distinct values on the two
  rigs that do have them. Given `AGENT_STATE`'s warning that bone probes lie by returning success for
  any name, existence here is argued from the VALUES differing per rig, not from the call succeeding.
- The four modes now read `Skeleton` and the five control bones still read `Animation`.
- **The write reached disk**: `SK_Human_Skeleton.uasset` mtime moved, and `git status` shows it
  modified. Checked because `AGENT_STATE` records an editor write that returned `True`, read back
  correctly, and never dirtied the package.

**What is NOT verified, beyond the eyes themselves:** that `Skeleton` retargeting on `Head` does not
subtly change how the head sits during the retargeted combat montages. It should not - the human
clips carry no translation animation on any bone but `Hips` (measured in #133: spread exactly 0.0000
across walk, run, attack and idle) - but "should not" is the phrasing this project has been burned by
three times, and the head is the most-watched bone on the model.

**Owed to `AGENT_STATE.md`:** the `Animation`-retargeting gotcha, which has now caused two distinct
visible bugs (the #133 import collapse and this one) and will cause a third. It belongs in FAILED as
a property of `SK_Human_Skeleton`, not as a footnote to either ticket.

## Refine

**Considered and rejected: converting the whole skeleton to `Skeleton` retargeting** (all non-`Hips`
bones, with `Hips` on `AnimationScaled`). That is the canonical UE setup for one skeleton shared
across differently-proportioned characters, it would permanently disarm the trap, and it is what I
would choose on a fresh project. Rejected here because Michael had just finished signing off goblin
locomotion after two broken passes from me in one session, and a change that alters how every human
animation resolves its proportions is not what to put in front of him next. The head chain is the
part with a reported defect; the rest can follow once someone has watched all six characters move.

**Deliberately left undone:**

- **The Knight's missing `LeftEye` bone.** A source-asset defect in his original rig that no
  retargeting mode can repair - his left eye geometry has no correctly-placed bone to follow. If his
  left eye is still wrong after this change while his right is fixed, that is the reason, and the fix
  is a re-import or a hand-authored bone rather than anything in this ticket.
- **The shared rig's asymmetric eye bones** (+4.80 / -5.79). Now bypassed for posing rather than
  corrected, since `Skeleton` mode stops those values being used. Left in place because correcting
  them would change the skeleton's reference pose, which is a much larger blast radius than the
  problem justifies.
