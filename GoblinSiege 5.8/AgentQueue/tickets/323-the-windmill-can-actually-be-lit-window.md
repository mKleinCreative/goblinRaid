---
id: 323
title: The windmill can actually be lit: window trigger, lit-torch tag, and it counts as an objective
agent: claude-crumble
status: done
claimed: 2026-08-26T19:15Z
build: none
waiting_on:
evaluated: 2026-08-26T23:20:57Z
observed: 2026-08-26T19:30:19Z | Michael threw a torch through the windmill's upper window and it took: the mill lit, ran its fuse and detonated, and the raid director logged 'Objective.Burn.Mill now 1/1. TYPE SATISFIED', demoted the second windmill to Optional and scored +100 deeds. Before this the mill could not be lit by any means a player has - neither half of the window overlap existed.
scenario: PIE on L_Tutorial_Island, GS_Windmill (the ground one), thrown torch from ~1900uu back aimed at the window band at offset z=1610 that Michael positioned with a marker
files: 
  - Source/GoblinSiege/Destruction/GSMillObjective.h
  - Source/GoblinSiege/Destruction/GSMillObjective.cpp
  - Source/GoblinSiege/Destruction/GSTorchProjectile.h
  - Source/GoblinSiege/Destruction/GSTorchProjectile.cpp
---

## Goal

The windmill can actually be lit: window trigger, lit-torch tag, and it counts as an objective

## Generate

**The windmill could not be lit by any means a player has.** Not stubborn - unlightable.

`AGSMillObjective::IgniteAtLocation` is a deliberate no-op (stone base, sails out of reach), so
`IgniteInterior` is the only route in, and it is reached by an overlap between a component tagged
`WindowComponentTag` and an actor tagged `LitTorchActorTag`. **Neither half existed.** No placed
windmill carried a component with that tag, and the string "GS.LitTorch" appeared exactly twice in
the whole project: the mill's own property, and the check that reads it. Nothing ever applied it. The
only surviving caller of `IgniteInterior` was the `GS.Burn` debug command.

It read as a design rule rather than a bug because the REFUSAL path still worked: a torch to the
outside fired `OnExteriorIgnitionRefused`, so the mill looked like it was saying no on purpose while
having no way to say yes.

Three changes:
- `WindowTrigger`, a UBoxComponent created by the class and tagged on construction, so the existing
  BeginPlay tag scan finds it with no special case. In code rather than hand-placed, because
  hand-wiring is exactly what had been forgotten on both mills.
- `Tags.Add("GS.LitTorch")` on `AGSTorchProjectile`. Tagged on the projectile, not the held torch:
  the mill wants "something burning came through the window", which is the thrown thing.
- `WindowTriggerOffset` z=1610, and see Evaluate - that number is not mine.

## Evaluate

**OBSERVED, by Michael, and it counted:** he threw a torch through the upper window, the mill lit,
ran its fuse and detonated, and the raid director logged `Objective.Burn.Mill now 1/1. TYPE
SATISFIED`, demoted the second windmill to Optional and awarded +100 deeds.

**THE HEIGHT IS MEASURED, NOT REASONED, AND THAT IS THE LESSON.** I set the trigger to z+2600 from
bounds arithmetic - "the mesh is 4434 tall, so the upper third is about here". Michael: "it's too
high, we need it right above the 1st round section", which is a thing you can see and cannot compute
from a bounding box. He put a marker where it belonged and it read back 1610 - nearly a metre and a
half below my guess. He then made it a standing instruction: *"if you have a question about something
that requires precision, place a marker in the world and I'll take a look at it"*.

**A band, not a window, and it is a deliberate compromise.** The trigger encircles the tower, so any
face works. The fiction is one window; but the mills carry no window marker and are rotated
differently, so a single-face volume needs per-instance authoring and is silently wrong wherever
nobody does it. More forgiving than the fiction, never silently broken. Narrow it when somebody
decides which face the window is.

**I REPORTED A BUG THAT DID NOT EXIST.** I told Michael the mill's empty `ObjectiveTypeTag` meant a
burn would score nothing. Burn objectives are typed by the `ObjectiveType` ENUM, set to `Windmill` in
the constructor; that tag is a different field and the mill counted correctly all along. I checked
the wrong thing and stated it as fact.

**I also proposed a change that would have been a real regression:** adding a `UGSFlammableComponent`
so the mill would char. The header argues at length against exactly that - a flammable would give the
mill a second, contradictory ignition path, since `AGSFireVolume::SpreadTick` ignites every flammable
in reach, so a hedge fire next door would light it through a rule the design specifically refuses.

**Not observed:** the hill windmill. Never tested, and its tower sits 2213uu from its objective.

## Refine

Changed after evaluation: the geometry search that #324 added was corrected from a 3D distance test
to a horizontal one, because a windmill is a vertical stack and its sails measured 4361uu away while
standing directly overhead.

Left undone deliberately: the single-face window volume, and the hill mill.

> 2026-08-26T23:20Z Writing G/E/R

> 2026-08-26T23:20Z G/E/R written against the shipped tree.
