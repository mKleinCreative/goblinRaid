---
id: 121
title: Animation smoothness pass 1: swings play at real speed, with press-to-contact timings rescaled to match
agent: claude-animsmooth
status: done
claimed: 2026-08-10T23:01Z
build: none
waiting_on:
evaluated: 2026-08-10T23:13:00Z
files: 
  - GoblinSiege 5.8/Content/Blueprints/Abilities/Human/GA_HU_SwordLight.uasset
  - GoblinSiege 5.8/Content/Blueprints/Abilities/Human/GA_HU_SwordHeavy.uasset
  - GoblinSiege 5.8/Content/Blueprints/Abilities/GA_GS_SwordLight.uasset
  - GoblinSiege 5.8/Content/Blueprints/Abilities/GA_GS_SwordHeavy.uasset
---

## Goal

Animation smoothness pass 1: swings play at real speed, with press-to-contact timings rescaled to match

## Generate

Michael's brief was a whole-game animation pass - all four of feet-slide, action popping, facing
snap and rushed attacks, on both rigs, one change at a time with him judging each. This ticket is
step 1 of that pass.

Every swing stage played its montage at 1.5x-1.8x. Mixamo clips authored at normal speed, played
half again as fast, read as rushed. But `MontagePlayRate` cannot be changed alone: `FGSSwingStage`
documents `WindupSeconds` as *"Press-to-contact. Set it so the damage window straddles the montage's
swing apex"*, so windup/window/recovery are real-time values tuned against the montage AT ITS PLAY
RATE. Drop the rate without rescaling and the hit lands before the blade arrives.

Michael chose to slow the swings properly rather than keep the cadence, knowing it lengthens combat.
Applied to all 8 stages across both rigs: rate -> 1.00x, and windup / damage window / recovery each
multiplied by the OLD rate so contact stays on the apex.

| ability | stage | was | now |
|---|---|---|---|
| HU/GS_Light | 0 | 1.50x, 0.220/0.160/0.220 | 1.00x, 0.330/0.240/0.330 |
| HU/GS_Light | 1 | 1.50x, 0.180/0.200/0.220 | 1.00x, 0.270/0.300/0.330 |
| HU/GS_Light | 2 (Flurry) | 1.80x, 0.550/0.500/0.420 | 1.00x, 0.990/0.900/0.756 |
| HU/GS_Heavy | 0 | 1.15x, 0.680/0.300/0.550 | 1.00x, 0.782/0.345/0.633 |

Full 3-stage combo goes 2.67s -> 4.45s. Both rigs got identical values, so humans and goblins stay
in step.

## Evaluate

**Verified:**

- **Michael watched it and accepted it, including the case I flagged as riskiest.** I called out that
  the Flurry becoming a 2.65s commitment was well beyond "smoother" and offered to cap that stage at
  1.3x instead; his answer was *"the flurry is fine"*.
- Re-read every stage from the CDO after saving: all 8 at rate=1.00 with the intended timings.
- **Checked before applying that this is not secretly a balance change.** A 0.50 -> 0.90s damage
  window would multiply damage if the sweep could re-hit; `HitActorsThisSwing`
  (`GSGA_SwordLight.cpp:334`) dedups per swing, so the longer window widens the catch window only.
- No rebuild required - these are Blueprint CDO defaults.

**NOT verified:** AI cadence at the new timings. Defenders now commit for 0.9s per light swing where
they used to commit for 0.6s, which interacts with attack tokens and the block probability. Nobody
has measured whether fights still resolve at a sensible rate; TTK was already flagged in #087 as
under the recommended band and this change moves it the other way.

## Refine

**Nothing changed on re-reading.** The alternative - leave the play rate and chase the "rushed" read
through blend times alone - was put to Michael explicitly with the cadence cost spelled out, and he
chose the retune.

**Deliberately left undone, and still the rest of this pass:** montage blend in/out (the popping
between actions), the melee facing snap (`BTTask_MeleeAttack` turns up to `MaxFacingSnapDegrees=120`
in a single frame with no DeltaSeconds, where `BTTask_Block` and `BTTask_MenaceOrbit` both step at
`TurnRate * DeltaSeconds`), and the locomotion blendspace (`ABP_Human` runs discrete Idle/Walk/Run
states, which is the usual cause of foot-slide and threshold popping).
