---
id: 361
title: Mill: exterior-fire-immune rule retired, char redirected to real geometry
agent: claude-fire
status: done
claimed: 2026-08-30T00:29Z
build: none
waiting_on:
evaluated: 2026-08-30T01:27:49Z
observed: 2026-08-30T01:27:52Z | Both placed GSMillObjective instances lit, ran their 9s fuse, sank correctly (SinkTower log confirms both), and Michael watched the char/fire directly: cinematic fire during the burn, then collapse
scenario: Live PIE, L_Tutorial_Island, both placed mills (GS_Windmill, GS_Windmill_Hill) ignited and completed in one session, human-observed
files: 
  - Source/GoblinSiege/Destruction/GSMillObjective.h
  - Source/GoblinSiege/Destruction/GSMillObjective.cpp
  - Source/GoblinSiege/Destruction/GSBurnFXComponent.h
  - Source/GoblinSiege/Destruction/GSBurnFXComponent.cpp
---

## Goal

Mill: exterior-fire-immune rule retired, char redirected to real geometry

## Generate

Two independent changes, same two classes, done together because both were blocking a proper
live test of the mill:

**1. Exterior-fire-immune rule retired (Michael: "it's outdated").** `AGSMillObjective::IgniteAtLocation`
used to be a deliberate no-op (design doc's "stone base, no purchase" refusal, exterior torches
bounced off). Rewritten to just call `IgniteInterior()` - a torch anywhere on the mill now starts
the same fuse the window-interior route does. Removed the now-dead `OnExteriorIgnitionRefused`
delegate from `GSMillObjective.h` (nothing was ever bound to it). Updated the class-level header
comment and the window-search comment block to describe the retirement rather than the old rule.

**2. Mill char redirect.** `UGSBurnFXComponent::CacheMaterialInstances()` chars whatever mesh
components live on its OWNER - but both placed mills have empty `MillMesh`/`SailMesh` placeholders;
the actual visible tower is a separate `StaticMeshActor` resolved at runtime by
`AGSMillObjective::SinkTower()`'s own name-substring search. Without a redirect, char was being
driven correctly (`SetBurnAmount` ticking 0->1) at zero populated material slots - a silent no-op,
not a crash, so nothing in the log flagged it.

- `GSBurnFXComponent.h`/`.cpp`: added `SetCharTargetActor(AActor*)` and a `TWeakObjectPtr<AActor>
  CharTargetActor` member. `CacheMaterialInstances()` now targets `CharTargetActor` when set,
  falling back to `GetOwner()` for every other user of this component. `SetCharTargetActor` re-caches
  and re-pushes the current char level immediately, since a caller resolving its geometry mid-fuse
  (the mill can't know it at construction) needs the redirect to take effect without waiting for the
  next tick.
- `GSMillObjective.h`/`.cpp`: extracted `SinkTower()`'s inline geometry-search loop into a shared
  `AGSMillObjective::ResolveMillGeometry() const` helper (same case-insensitive `MillGeometryNameFilter`
  match from #360). `BeginPlay()` now calls it and, if found, calls
  `BurnFXComponent->SetCharTargetActor(Geometry)`. `SinkTower()` calls the same helper instead of
  duplicating the search.

Rebuilt (`Build-GoblinSiege.ps1 -IgnoreQueue`, solo session, only ticket in queue was this one) -
succeeded in 40s, no new warnings beyond the two pre-existing `AbilityTags` deprecation notices.

## Evaluate

**Verified live, by the strongest evidence tier this project has: Michael watched both mills burn.**
After relaunching the editor on the new binary and loading `L_Tutorial_Island`, both placed
`GSMillObjective`s were ignited and ran their full fuse-to-sink sequence in one PIE session. Log
confirms both completed cleanly:
```
[GS.Burn] GSMillObjective_0 interior lit - fuse running (9.0s to detonation).
[GS.Burn] GSMillObjective_1 interior lit - fuse running (9.0s to detonation).
'GSMillObjective_0' burned: Objective.Burn.Mill now 1/1. TYPE SATISFIED.
'GSMillObjective_0' sank: 'SM_WIndmill_Base2' retired, GC_WIndmill_Stump standing, GC_WIndmill_Top released, 6 fire(s) on the wreck.
'GSMillObjective_1' burned: Objective.Burn.Mill now 2/1.
'GSMillObjective_1' sank: 'SM_WIndmill_Base_Blueprint' retired, GC_WIndmill_Stump standing, GC_WIndmill_Top released, 6 fire(s) on the wreck.
```
Michael's own words, watching it happen: "both mills were cinematic in a beautiful way when they
got set on fire, then collapsed. Loved it." That is a human-watched, both-instances, direct visual
confirmation of exactly what this ticket set out to fix - the char/fire was invisible before this
change (fuse ran, no visible reaction) and is now the centerpiece of the moment.

**Not separately isolated: whether the exterior-ignite path specifically (as opposed to the
window-interior route) was what lit these two mills this run.** Both mills fired via
`IgniteInterior`/`[GS.Burn]` in the log, which either route now funnels into identically -
`IgniteAtLocation` is a thin wrapper around the same call. The code change is small and directly
inspectable (one function body, now three lines), and the ticket's evidence is strong on the char
fix; a dedicated exterior-torch-only test would be the next-session follow-up if anyone wants to
isolate that path specifically rather than trust the wrapper.

**Flagged, not fixed - found during this same PIE session, unrelated to this ticket's files:**
a full-map burn produced a critical system memory-pressure warning (0.73GB available, Cache panel
screenshot). Root cause of THIS session's specific hang was three leaked PowerShell background
processes (~24GB combined) from my own earlier tool calls, killed and confirmed resolved (25.7GB
free after). But Michael separately observed some buildings "secretly count as more than 1 building
are spawning multiple of the same fairly hefty fire" - a real, unaddressed concern about
per-building fire/smolder instance count on `AGSBuildingObjective` clusters, worth its own ticket.
Also pre-existing and out of scope here (already flagged in #360): both mills still log "has a
UGSBurnFXComponent but no UGSFlammableComponent - it will never char" at BeginPlay - a stale,
misleading warning left over from before the mill had any char path at all; harmless (the mill chars
via direct `SetBurnAmount` calls, not the flammable-driven route this warning describes) but worth a
follow-up ticket to either correct the wording or gate it off for actors that drive char manually.

## Refine

Closing as `done`. Both changes in this ticket are code-complete, built, and confirmed by direct
human observation on both placed mill instances in one PIE run - the strongest evidence tier
available. Two follow-ups written down above rather than silently absorbed: the multi-fire-per-cluster
building concern, and the stale/misleading "will never char" warning on the mill. Ticket #361 build
field left as the log-confirmed rebuild; no further build needed for this specific fix.
