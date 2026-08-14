---
id: 101
title: Humans T-pose in combat: goblin-skeleton montages play on the human rig
agent: claude-gobkit
status: done
claimed: 2026-08-09T18:35Z
build: none
waiting_on:
evaluated: 2026-08-09T18:37:46Z
files: 
  - Source/GoblinSiege/Characters/GSCharacterBase.h
  - Source/GoblinSiege/Characters/GSCharacterBase.cpp
---

## Goal

Humans T-pose in combat: goblin-skeleton montages play on the human rig

## Generate

Michael: "when they get hit they revert into a tpose", and "there's no sword in the grip of the guard".
**One root cause under both.**

**Every montage in the project lives on `GOB_Scout_v2_Skeleton`** - all 36 of them, all under
`/Game/Characters/ScoutV2/Montages/`. There are no human montages at all. The six human defenders run
`SK_Human_Skeleton` with `ABP_Human`, which references exactly three animations: Idle, WalkF, RunF.

**UE does not refuse the mismatch.** `PlayAnimMontage(AM_GS_Atk_Light)` on a castle guard returns a
cheerful **1.150** and drives the slot - but the tracks map to nothing on that skeleton, so the slot
evaluates to the **reference pose**. Every attack, block and guard break snaps a guard into a T-pose
for the montage's length.

That is also the "no sword in the grip": in ref pose the arms go straight out, and the sword - which
is correctly attached to `RightHand` - goes out sideways with the arm, reading as detached.

**Fix:** `AGSCharacterBase::PlayAnimMontage` override that refuses a montage whose skeleton differs
from the character's own and returns 0. One gate for every caller - abilities, flinches, horn, torch -
because the mismatch is a property of the character, not of any one caller. Refuses only when **both**
skeletons are known and differ, so an unknown skeleton cannot silently kill the player's animations.

## Evaluate

**Reproduced on demand, then measured**, on a live guard in PIE:

```
play_anim_montage(AM_GS_Atk_Light) -> 1.150     # NOT refused by the engine
hand lateral offset: 104.6uu (idle)  ->  148.9uu (during montage)
```

Played at 0.04x to hold the pose still and photographed it: `HighresScreenshot00024` is a textbook
T-pose, arms straight out. The 148.9uu matches the 149uu measured on a guard mid-combat earlier, which
ties the reproduction to the thing Michael actually saw.

**The sword attachment was never broken.** Settled from the vertex data rather than by eye: bucketing
`GS_Sword_Guard`'s 5814 verts by Z shows the crossguard's XY radius peaking at 10.7-11.8 around
z 60-80 with the grip above it, and a 3.0-3.6 radius taper down to z=0. **z=0 is the tip**, so #100's
translation was right, and `GRIP 0.0uu from RightHand` holds. I had doubted it when the hand looked
empty on camera; the T-pose was why.

**BUILT AND VERIFIED** (Michael approved closing the editor). `BUILD SUCCEEDED in 01:51`, no new
warnings - only the two pre-existing `C4996 AbilityTags` deprecations. The same test that reproduced
the bug now shows it gone, **with a control that proves the fix is not too broad**:

```
HUMAN guard + goblin montage -> returned 0.000   (was 1.150)   hand 103.8uu -> 103.8uu unchanged
CONTROL player (goblin rig)  -> returned 1.150               # still plays - player animation intact
live fight, 10 goblins on a guard: worst hand lateral 103.8uu  (T-pose is ~149uu)
```

The control matters: refusing montages is exactly the kind of fix that can silently disable the
player's own animations, and it does not.

Sword attachment re-measured after the build: `GRIP 0.0uu / TIP 144.7uu from RightHand`, and the guard
is visibly holding it in `HighresScreenshot00025`.

**Not addressed, and it is the real fix:** the humans have no combat animations. This stopgap trades a
T-posing guard for an unanimated one - strictly better, still wrong. Retargeting the goblin montages to
`SK_Human_Skeleton`, or authoring human ones, is a content decision.

**Correction owed to AGENT_STATE:** #097 recorded "each guard has its OWN skeleton, not a shared one".
**That is wrong** - all four defenders share `SK_Human_Skeleton`. It matters: a weapon socket added
once to that skeleton would serve every human, instead of the `RightHand`-bone attachment #097 settled
for on the strength of the wrong belief.

## Refine

**Made the check conservative in the right direction.** The first shape refused whenever the skeletons
were not equal; if either is null that is ignorance, not mismatch, and refusing on it would have
silently stopped the player animating. It now requires both to be known.

**Put the gate on the character, not on the callers.** Guarding each `PlayAnimMontage` call site would
have meant five edits and a sixth caller added later that forgets.

**Deliberately left undone:** human combat animations, the guard blade's resting angle, Erika's bow.
