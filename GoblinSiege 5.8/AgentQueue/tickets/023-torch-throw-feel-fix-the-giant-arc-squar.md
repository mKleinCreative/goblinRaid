---
id: 023
title: Torch throw feel: fix the giant arc square, show the reticle, more range, visible in flight
agent: claude-ranged
status: done
claimed: 2026-08-06T05:29Z
build: required
waiting_on: build gate
files: 
  - Source/GoblinSiege/Combat/GSAimComponent.h
  - Source/GoblinSiege/Combat/GSAimComponent.cpp
  - Source/GoblinSiege/Destruction/GSTorchProjectile.h
  - Source/GoblinSiege/Destruction/GSTorchProjectile.cpp
evaluated: 2026-08-06T05:45Z
---

## Goal

Torch throw feel: fix the giant arc square, show the reticle, more range, visible in flight

## Generate

Michael's report: "a big square appears and no reticule... we don't throw really impressively
at all... there's no way to tell where your torch is or the arc it's being thrown at."

**The big square - my bug, and a certain diagnosis.** `USplineMeshComponent::SetStartScale`
takes a MULTIPLIER on the mesh's cross-section, not a width. `ArcSegmentWidth` was 4, the
assigned mesh is `/Engine/BasicShapes/Cube` at 100uu, so every arc segment drew **400uu
across** - a giant box straddling the trajectory. `UpdateArcVisual` now derives the scale
from `ResolvedArcMesh->GetBounds().BoxExtent`, so the property means world units for any
mesh. Default 4 -> 8.

**Range.** `AGSTorchProjectile` InitialSpeed/MaxSpeed 1400 -> 2400. Range goes as the SQUARE
of speed (v^2/g), so this is roughly 20m -> 59m, not a 70% bump. Gravity deliberately left at
1.0 - the lob is what makes the arc worth previewing at all.

**The reticle.** Two candidate causes, both addressed because I cannot tell which it was:
- It may simply have been inside the 400uu box.
- `LandingDecalSize.X` is the decal's PROJECTION DEPTH, and it was 40 - so a predicted impact
  point sitting even slightly off the real surface painted nothing. 40 -> 250.
- The "no LandingDecalMaterial" message was `Log` verbosity. Raised to `Warning` and it now
  names the asset and the property to set.

**Trackable in flight.** New `FlameFX` (`UNiagaraComponent`) + `FlameSystem`
(`TSoftObjectPtr<UNiagaraSystem>`, C++-defaulted to `/Game/VFX/NS_GS_TorchFlame`) on the
projectile, resolved and activated in `BeginPlay` on the module's warn-once pattern. A torch
is legible in the air because it is ON FIRE; nothing was drawing fire until it landed.

**Second-order fix I nearly shipped a regression on.** `MaxSimSeconds` 3 -> 5. Time of flight
at 45 degrees is `2*v*sin(45)/g` = ~3.5s at 2400uu/s, past the old 3s window. The prediction
would have ended mid-air, `PredictProjectilePath` would report no hit, and `UpdateArcVisual`
HIDES the decal when there is no hit - so raising the speed alone would have deleted the
reticle on exactly the long throws that most need one.

## Evaluate

**NOT COMPILED AND NOT RUN.** The build gate is closed (#022, #024, and this ticket). Every
claim below is reasoning, not observation.

**Confidence varies sharply across these fixes, and it should be read that way.**
- The square is *certain*: 4 x 100uu = 400uu is arithmetic, not a hypothesis, and it matches
  the reported symptom exactly. This is unlike the earlier muzzle fix (#004), which I
  flagged as a guess and which remains unconfirmed.
- The reticle is *not* certain. I fixed two plausible causes without knowing which was real.
  If it was neither - if the material was never assigned - then nothing here fixes it, but
  the new Warning will finally say so out loud instead of hiding at Log level.
- The range number is a judgement call with no playtest behind it. 2400 may overshoot; it is
  one `EditDefaultsOnly` value on the projectile, so it is cheap to walk back.

**Possible duplicate of #022.** claude-perf has spent 0.3h searching L_Tutorial_Island for an
"empty gigantic blocking volume" and five searches found nothing. Michael's phrasing here was
"when I try and throw a torch, a big square appears" - tied to the throw, not to a place. I
believe #022 is chasing this same arc-ribbon bug as a level actor. Worth checking before that
agent spends more time on it; if so, #022 closes as `abandoned` with no level edit.

**New UPROPERTYs** (`FlameFX`, `FlameSystem`) mean Live Coding cannot apply this - it needs an
editor-closed build.

**Owes AGENT_STATE.md** - FAILED: `SetStartScale` on a spline mesh is a multiplier, not a
width, and getting that wrong is invisible in code review and enormous on screen.

## Refine

- Caught the `MaxSimSeconds` regression by arithmetic before shipping it, rather than after
  Michael reported "the reticle disappeared when you made it throw further". Raising the
  speed and the sim window are one change, not two.
- Raised the decal-material message from `Log` to `Warning`. The whole reason this round-trip
  cost a report is that the code already knew the answer and filed it below the level anyone
  reads.
- Did NOT flatten gravity to buy range. It is the obvious lever and it would have worked, but
  it deletes the arc - and the arc is the feature this whole framework exists to draw.

**Deliberately left undone.** The radial weapon wheel Michael asked for is not in this ticket;
it is a separate design with two answered questions (torch becomes a held weapon; hold-Q,
drag, release) and one open one (who builds the widget). Folding it in here would have mixed
a feel-fix with an input-model change.

> 2026-08-06T05:45Z RETRACTION: my Evaluate claimed #022 might be chasing this same arc bug. That was WRONG - do not abandon #022 on my account. Evidence: GSAimComponent draws no wireframe (only a DrawDebugSphere behind GS.Aim.Debug); no red DrawDebugBox exists anywhere in Source; and in Michael's recording the red box and ellipse stay world-anchored to a wall structure and a distant tree while the goblin moves past them. That is a level actor, which is #022's subject. The ArcSegmentWidth scale bug in this ticket is still real arithmetic (4 x 100uu cube = 400uu) but is NOT what Michael saw in that video.
