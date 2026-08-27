---
id: 324
title: The windmill sinks instead of detonating: anchored base, burning top half comes down
agent: claude-crumble
status: done
claimed: 2026-08-26T19:49Z
build: none
waiting_on:
evaluated: 2026-08-26T23:21:12Z
observed: 2026-08-26T23:10:52Z | Michael watched the windmill burn down and give way: the charred stone stump stays standing at 61% height, the tower cap comes apart and falls onto it, the sails break up and land at the foot instead of flying off, the roof tiles come apart with everything else, and six fires burn on the wreck. His words: 'the collapse looked wonderful'. The roof also APPEARED during the break-apart, having been invisible while standing - the fractured collection renders where the original actor does not.
scenario: PIE on L_Tutorial_Island, GS_Windmill, ignited via IgniteInterior from Python and watched from the ground to the west
files: 
  - Source/GoblinSiege/Destruction/GSMillObjective.h
  - Source/GoblinSiege/Destruction/GSMillObjective.cpp
  - Source/GoblinSiege/Destruction/GSCrumbleComponent.h
  - Source/GoblinSiege/Destruction/GSCrumbleComponent.cpp
  - Content/Destruction/GC_WIndmill_Base.uasset
---

## Goal

The windmill sinks instead of detonating: anchored base, burning top half comes down

## Generate

**The windmill sinks instead of detonating.** Michael: *"our system looks good and a detonation is a
little much, I'd rather the top half just sink to the ground and it all be on fire."* This REPLACES
GDD Ruling 7's "smashed-but-recognizable silhouette" mesh swap - a ruled beat superseded, so it owes
`docs/decisions-ledger.md` a row. The swap path is left intact and simply unused; it was already a
no-op on both mills, neither of which has a DestroyedMesh.

The tower is fractured once and pruned into **two** collections - `GC_WIndmill_Top` (23 pieces, falls)
and `GC_WIndmill_Stump` (27, stands) - plus `GC_Windmill_Sail` (27) and `GC_RoofTIles` (29) for the
loose parts. `UGSCrumbleComponent::SpawnProxyFor` was generalised out of `AGSBuildingObjective` and
takes an explicit asset name so one mesh can yield several collections.

## Evaluate

**OBSERVED:** Michael watched the mill burn down and give way - charred stump standing, cap falling
onto it, sails and roof breaking up at the foot, six fires on the wreck. *"The collapse looked
wonderful."*

**FOUR FAILED THEORIES ABOUT ANCHORING, AND I STILL DO NOT KNOW THE ANSWER.** The plan was to anchor
the base of ONE collection. Every attempt applied its anchors successfully - the log said "anchored
27 of 49" - and let the base go anyway:

| ordering | result |
|---|---|
| anchor, then crumble | base freed and exploded, top stayed as one cluster |
| anchor, no crumble | nothing separated at all: 27 pinned, 22 above the line, zero movement |
| crumble, then anchor | top fell correctly, 25 of 27 base pieces went with it |
| anchor before going dynamic | byte-identical to the previous |

**Michael picked the two-asset approach first and I talked him out of it on a cost estimate I got
wrong** - I had not worked out that the stump needs its own asset, and quoted "15 minutes". Which
pieces exist is a fact about the asset, and facts do not have ordering bugs. That is what worked.

**Bugs found that were nothing to do with anchoring**, each hiding the next:
- The falling top was DEMOLISHING the standing stump: collections default to
  `bEnableDamageFromCollision` with thresholds [500000, 50000, 5000] and a 100,000 kg cap clears
  those easily.
- `bAllowRemovalOnSleep` and `bAllowRemovalOnBreak` default to TRUE, so Chaos deleted settled pieces -
  Michael: "the top of the mill keeps disappearing?". #192 hit this on the statue: a raid whose ruins
  evaporate has no memory. **Every collection spawned today had this, houses included.**
- `AddImpulse(..., bVelChange=true)` makes the magnitude a VELOCITY: 40000 meant 400 m/s and put the
  sails 341 metres off the map.
- The sails are **311 tonnes** and have no RotatingMovementComponent, so the code stopping them
  spinning stopped nothing. Fractured instead of simulated whole, they behave.
- **A windmill is THREE actors and I handled two.** `SM_RoofTIles2` is a separate actor; when the
  tower sank the roof stayed in the sky. Michael's screenshot found in one glance what four rounds of
  measurement had pointed away from - I was chasing unmoved indices INSIDE the collections, which are
  invisible root nodes.

**MY INSTRUMENTS WERE WRONG REPEATEDLY, and cost more of Michael's takes than the bugs did.** The
outcome line read root-cluster velocity, which is zero once a collection shatters. `GetActorBounds`
stopped including the collection the moment it was detached, so a "spread dropped from 45,817 to
4,316" was measuring a hidden mesh. The hanging-piece warning fired at 0.5s and reported 16 of 23
stuck when the truth was two. And the straggler sweep's own guard disabled it in exactly the case it
existed for.

## Refine

Changed after evaluation: sweep capped at 3 passes and skipping index 0, after it was seen firing
every six seconds forever against a root node that can never move.

Left undone: the hill windmill is untested; `SM_RoofTIles2` does not render at runtime (#326); the
ledger row for superseding Ruling 7; and co-op, where the loose-part actors do not replicate.

> 2026-08-26T23:21Z Refreshing Evaluate after the roof and sweep-cap work

> 2026-08-26T23:21Z G/E/R written against the shipped tree.
