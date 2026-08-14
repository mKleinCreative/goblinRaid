---
id: 129
title: Upper-body split for humans: move the attack montages onto the UpperBody slot ABP_Human already has
agent: claude-animsplit
status: done
claimed: 2026-08-11T02:02Z
build: none
waiting_on:
evaluated: 2026-08-11T04:01:01Z
files: 
  - GoblinSiege 5.8/Content/Characters/Humans/Anims_Combat/AM_HU_Atk_Light.uasset
  - GoblinSiege 5.8/Content/Characters/Humans/Anims_Combat/AM_HU_Atk_Spin.uasset
  - GoblinSiege 5.8/Content/Characters/Humans/Anims_Combat/AM_HU_Atk_Flurry.uasset
  - GoblinSiege 5.8/Content/Characters/Humans/Anims_Combat/AM_HU_Atk_Heavy.uasset
  - GoblinSiege 5.8/Content/Characters/Humans/Anims_Combat/AM_HU_Atk_Chop.uasset
  - GoblinSiege 5.8/Content/Characters/Humans/Anims_Combat/AM_HU_Atk_Overhead.uasset
  - GoblinSiege 5.8/Content/Characters/Humans/Anims_Combat/AM_HU_Atk_Kick.uasset
---

## Goal

Upper-body split for humans: move the attack montages onto the UpperBody slot ABP_Human already has

## Generate

The last item on Michael's animation list: feet slide during swings, because the attack montage is
full-body and its near-static legs override the run while the capsule keeps moving.

`ABP_Human` already carried the plumbing - `Slot 'UpperBody'` feeding a `LayeredBoneBlend` at weight
1.0, wired to Output, with nothing playing into it. So for humans this was a **montage property
change only**, which never opens the AnimGraph package. That distinction matters: scripted AnimGraph
edits corrupted the goblin rig twice earlier tonight (#128 Refine), while montage edits are clean.

**Done in two stages deliberately.**

*Stage 1 - one montage as a probe.* `AM_HU_Atk_Light` moved to `UpperBody`. This was simultaneously
the fix and the only available test of whether the blend node's `LayerSetup` had a usable branch
filter - a field no API here can read. Result: **the attack went invisible and the guards ran into
each other**, which is exactly the empty-filter signature. With no branch filter every bone takes the
base pose, so the UpperBody slot output nothing, and with no attack pose overriding anything the
guards kept playing locomotion straight through their swings. Reverted in one command.

*Stage 2 - after Michael set the filter.* He added Branch Filter Bone Name = `spine` (the human rig's
lowest spine bone; the goblin's is `spine01` and the two are not interchangeable) and compiled clean.
All seven `AM_HU_Atk_*` montages then moved to `UpperBody`.

Blocks and hit-reacts deliberately stay on `DefaultSlot` - a flinch should stop the legs.

## Evaluate

**Verified:**
- Each of the seven montages re-read after writing: all report `slots=['UpperBody']`.
- `ABP_Human` compiled with **zero errors** after the change, and the session log shows
  `Missing allocated node` count 0 - confirming montage-slot edits do not damage the package the way
  scripted graph surgery did.
- Blocks and hit-reacts confirmed still `DefaultSlot`.
- **Michael's footage shows the guards' upper bodies animating with no invisible attacks**, so the
  filter took and the split is live.

**NOT verified:** whether the feet actually stop sliding. The footage that came back showed a
different problem loudly enough (guards clipping into each other) that the original symptom was not
assessed. That is still open.

**A second cause of sliding this does NOT address**, measured earlier: guards run at 1210 uu/s while
their fastest clip `A_HU_Std_RunF` carries 408 uu/s - a 2.96x mismatch. The split stops the attack
pose freezing the legs; it cannot make a run cycle keep up with a speed it was never authored for.
The unused `A_HU_Sprint` clip (993 uu/s) is the lead there.

**The player is untouched.** `ThirdPerson_AnimBP_Gob` has only `DefaultSlot` and no layered blend, so
the goblin rig still needs the graph authored by hand - and per #128 that must NOT be done with
`AnimGraphService`.

## Refine

**Changed after the stage-1 failure:** nothing about the approach, which was sound - the probe did
its job by failing cheaply and revealing that "the plumbing exists" meant the nodes existed and had
never been configured. I had assumed configured; one montage and one revert cost two minutes to
learn otherwise.

**Deliberately left undone:** the goblin/player rig, pending Michael hand-authoring the nodes; the
locomotion speed mismatch above; and `AM_HU_Block_Idle` staying full-body even though blocking also
pins the legs - worth revisiting once the attack split is judged.
