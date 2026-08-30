---
id: 359
title: Wire GS_CrumbleDemo_House to burn and crumble; add UGSCrumbleComponent auto-crumble-on-burn
agent: claude-fire
status: done
claimed: 2026-08-29T23:04Z
build: none
waiting_on:
evaluated: 2026-08-30T00:01:20Z
observed: 2026-08-30T00:01:28Z | Michael watched the full ignite-to-crumble chain fire automatically (no manual trigger), plus smoke position and asset iterated live across several rounds, final verdict Perfect
scenario: Live PIE, GS_CrumbleDemo_House in L_CombatArena, ignited via UGSFlammableComponent::Ignite(), auto-crumbled via the new bAutoCrumbleOnBurnedDown binding
files: 
  - Source/GoblinSiege/Destruction/GSCrumbleComponent.h
  - Source/GoblinSiege/Destruction/GSCrumbleComponent.cpp
  - Source/GoblinSiege/Destruction/GSBurnFXComponent.cpp
  - Content/Maps/Test/L_CombatArena.umap
---

## Goal

Wire GS_CrumbleDemo_House to burn and crumble; add UGSCrumbleComponent auto-crumble-on-burn

## Generate

`GS_CrumbleDemo_House` in `L_CombatArena` was a bare `GeometryCollectionActor` - a Chaos-destruction
demo prop with none of the project's fire/destroy gameplay wired to it, unlike `AGSBuildingObjective`
(built for kitbashed clusters, not a fit for this single pre-fractured actor).

- Added three instance components directly to the placed actor (`unreal.SubobjectDataSubsystem` -
  the generic `add_component_by_class`/`NewObject`+`RegisterComponent` paths don't work on a placed
  non-Blueprint actor instance, cost real time to find): `UGSFlammableComponent`,
  `UGSBurnFXComponent`, `UGSCrumbleComponent`. Tuned the crumble collapse shape with
  `AGSBuildingObjective`'s own live-PIE-tuned defaults (`ClusterCrumblePasses=3`,
  `CollapseShoveCount=7`, `CollapseShoveMagnitude=5000000`, `CollapseInwardRatio=0.6`), plus
  `CharAmountOnRelease=1.0` since burnt-down rubble should drop charred, not pristine.
- **New C++, `UGSCrumbleComponent::bAutoCrumbleOnBurnedDown`** (`GSCrumbleComponent.h/.cpp`): opt-in
  (default off - a statue must never auto-crumble from fire), binds a sibling
  `UGSFlammableComponent::OnBurnedDown` to `Crumble(ZeroVector, ZeroVector)` in `BeginPlay`. This is
  the actual "burns down -> comes apart" trigger; without it nothing ever called `Crumble()`.
  Enabled on the demo house's instance.
- `GSBurnFXComponent.cpp`: `SpawnSmolder()` now spawns at the CENTRE of the actor's world bounds
  (was: root component origin, i.e. wherever the kit piece's pivot happens to sit - buried inside a
  house-sized actor, invisible from outside) and is now also called from `HandleIgnited()`, not only
  `HandleBurnedDown()` - Michael wanted smoke for both the "on fire" and "smouldering" phases, and
  `SpawnSmolder()` is idempotent (no double-spawn). Went through several iterations live with
  Michael watching: top-of-bounds spawn (first pass) read as a cloud floating above the house once
  the plume itself was big enough to clear the roofline on its own - moved to bounds-centre per his
  call, "sourced from inside" now that size isn't a problem.
- `SmolderSystem` default: `N_MeteorSpawn`-style placeholder gap (`/Game/VFX/NS_GS_Smolder` never
  actually existed) closed. Went through a real back-and-forth on WHICH asset: tried
  `P_SmolderSmoke` (DreamscapeFarmlands) first, but it's legacy Cascade (`UParticleSystem`) and this
  project is Niagara-only - Michael asked to convert it rather than add a second FX tech stack, which
  needed the built-in "Cascade To Niagara Converter" plugin (off by default, enabled via Edit >
  Plugins > Built-In > FX). Final default: `P_SmolderSmoke_Converted`, Michael's own Niagara
  conversion of the purpose-built asset.

## Evaluate

**Verified live in PIE across several rounds, watched by Michael each time, not just compiled:**
- `Crumble()` in isolation: called directly, `has_crumbled()` returned `True`, confirmed the
  collapse-shape tuning actually holds together as a mechanism before wiring the auto-trigger.
- Full chain: ignited the house, `is_burning` went `True`, and it broke apart on its own ~12s later
  via the new `bAutoCrumbleOnBurnedDown` binding with no further script calls - Michael watched it
  happen. His own words: "the breaks are working."
- Smoke position and asset both iterated against Michael's live observations, not assumed correct
  after one pass - "not smoldering" -> found the empty `SmolderSystem` gap; "spawning inside... can't
  see it" -> traced to root-origin spawn, fixed to bounds-based placement; "fairly huge... place it
  inside the house" -> moved top-of-bounds to bounds-centre. Final round: "Perfect."

**Caught and corrected before it shipped:** a duplicate, unregistered `GSFlammableComponent` from an
earlier failed component-add attempt (the generic Python paths silently created an orphaned
`NewObject` that was never actually part of the actor) was found live in the PIE component list and
removed via `destroy_component` before the auto-crumble work continued - would have double-fired
ignition/burn-tick logic if left in.

**Not extended beyond this one demo actor.** `bAutoCrumbleOnBurnedDown` is new, reusable, opt-in
component behaviour - available to any actor with both components - but nothing else in the project
has been switched on to use it yet. `AGSBuildingObjective` still drives its own crumble path
directly (`HandleCompleted`/`CrumblePieces`), unchanged and not migrated onto the new flag.

## Refine

Closing as `done` - Michael watched every stage (crumble mechanism, full burn-to-crumble chain,
smoke position twice, smoke asset twice) and the last round was "Perfect." Next actual fire work
is field-fire testing (`AGSFieldFireObjective`/pooled `AGSFireVolume`), which is a different system
and a new ticket, not a continuation of this one.
