---
id: 256
title: Draw strength flattens the shot: a weak release lobs, a perfect one flies straight, and the arc shows it live
agent: claude-acf
status: done
claimed: 2026-08-23T18:23Z
build: done
waiting_on: BUILT. Watch the arc straighten as you hold; judge whether 0.40 reads as weak or as broken.
evaluated: 2026-08-23T18:26:59Z
observed: 2026-08-23T18:29:45Z | The predicted arc starts as a weak lob and visibly flattens as the draw is held, straightest at the red band - Michael confirmed 'works great'
scenario: ranged mode, holding the draw and watching the arc across a full sweep
files: 
  - Source/GoblinSiege/Weapons/GSBowTimingComponent.h
  - Source/GoblinSiege/Weapons/GSBowTimingComponent.cpp
  - Source/GoblinSiege/Weapons/GSArrowProjectile.h
  - Source/GoblinSiege/Weapons/GSArrowProjectile.cpp
  - Source/GoblinSiege/Combat/GSAimComponent.cpp
  - Source/GoblinSiege/Weapons/Abilities/GSGA_BowShot.cpp
---

## Goal

Michael: *"as we draw the bow, the line for where it hits starts from a pitiful arch, into more of a
straight shot as we hold... it would help prevent shooting arrow spam and make it feel more realistic.
The peak attack time, the arrow should be straightest."*

## Generate

Draw strength now drives **launch speed**, and a slower arrow falls further over the same distance -
so a weak release lobs and a perfect one flies nearly flat.

- `UGSBowTimingComponent::GetSpeedScaleAt` - 1.0 across the whole red band, falling to
  `MinSpeedScale` (0.40) at either end. **Continuous**, unlike `GetQualityAt`: damage has a
  deliberate 1.0->2.0 discontinuity at the red edge because a bullseye should feel like a distinct
  reward, but a matching jump in TRAJECTORY would just look like the arc glitching. Normalised
  against the nearer end of the bar, same as the damage curve, because red sits at 0.54 and the runs
  either side are different lengths.
- `GetCurrentSpeedScale()` - live value, 1.0 whenever no draw is in flight.
- `AGSArrowProjectile::SetLaunchSpeedScale` - scales `InitialSpeed`, `MaxSpeed` **and the live
  Velocity**, because ProjectileMovement has already launched by the time `SpawnActor` returns.
- `UGSGA_BowShot::FireArrow` - applies it beside the damage multiplier, through the same
  `FindComponentByClass` lookup that keeps the whole feature player-only.
- `UGSAimComponent` - scales the predicted arc's speed by the LIVE value, gated on
  `AimMode == Bow`.

That last one is the point of the feature. The player watches the trajectory straighten as the
indicator climbs toward red, so the mechanic teaches itself with no number on screen - and spamming
releases is visibly punished rather than silently penalised.

0.40 rather than something tiny: below that the arc stops reading as "a weak shot" and starts reading
as "the bow is broken". It still roughly quadruples the drop over a given distance.

## Evaluate

Built (00:28; verified genuine - DLL rebuilt and the unity modules recompiled, since a suspiciously
fast build has meant a no-op on this project before). **Not watched.** Nothing here is verified
beyond compiling.

Two things to watch for that I cannot check from here: whether 0.40 reads as "weak" or as "broken",
and whether the arc's straightening is legible at the speed the indicator moves - if 5s is too fast
to see the change, `TraverseSeconds` is the knob, not the speed curve.

AI unchanged and structurally so: Erika has no timing component, so `GetLastReleaseSpeedScale` is
never reached and her arrows keep their authored speed. The torch is unaffected because the arc
change is gated on `AimMode == Bow` and the scale returns 1.0 when nothing is drawn.

Files also claimed by #241 and #243 - both mine, both in review awaiting Michael's observation, so no
cross-agent conflict.

## Refine

Nothing changed on review. The one judgement call worth flagging is making speed continuous while
damage is not; they are deliberately different shapes and the reason is in the header.
