---
id: 392
title: Buildings with no fracture asset now break: generic rubble fallback for the 41 of 42 SM_MERGED_House_* meshes missing a GC_
agent: claude-grapple
status: done
claimed: 2026-08-31T20:28Z
build: succeeded
waiting_on:
evaluated: 2026-08-31T20:35Z
observed: 2026-08-31T20:29:14Z | Burned a house lacking a fracture asset; it produced a smoke/dust burst and completely disappeared instead of standing untouched after HandleCompleted
scenario: Live PIE on L_Tutorial_Island, player character, real playtest by Michael
files: 
  - Source/GoblinSiege/Destruction/GSBuildingObjective.h
  - Source/GoblinSiege/Destruction/GSBuildingObjective.cpp
---

## Goal

Buildings with no fracture asset now break: generic rubble fallback for the 41 of 42 SM_MERGED_House_* meshes missing a GC_

Raised by Michael right after #390 closed: "not every building has their collapse mesh cached." Quantified before writing any code (`ground-truth-before-thresholds` in project memory) rather than guessing at scope: of 42 distinct `/Game/.../SM_MERGED_House_*` meshes, only 1 (`SM_MERGED_House_Small_03`) has a matching `GC_` fracture asset under `Content/Destruction`. `AGSBuildingObjective::CrumblePieces`/`SpawnCollectionProxy` already treat a missing `GC_<name>` as the expected, silent case (per that method's own header comment) - so the other 41 house shapes simply stopped where they stood on `HandleCompleted`, with no error and no visible failure.

## Generate

Presented Michael the real constraint (a Geometry Collection bakes in the specific geometry it was
fractured from, so the one existing `GC_MERGED_House_Small_03` cannot stand in for a different mesh)
and two paths: bulk-author real fractures for all 42 meshes (better result, large content lift,
Chaos Fracture Mode's Python surface unverified), or a generic non-fracture fallback for pieces with
no match (ships same-day, every building visibly breaks, un-fractured ones look plainer). He picked
the fallback.

- `AGSBuildingObjective::CrumblePieces` now calls the new `SpawnGenericRubbleFallback(Piece)` whenever
  `SpawnCollectionProxy` returns null, instead of silently skipping the piece.
- `SpawnGenericRubbleFallback`: hides the piece's mesh + disables its collision (identical to the
  real-fracture path, so an invisible-but-solid house does not read as a failed collapse), spawns a
  one-shot Niagara burst and a sound at the piece's transform, then `Destroy()`s it. Deliberately not
  a fracture of any kind - see the header comment on why that would render the wrong house's rubble.
- Reused existing content rather than authoring new assets: `GenericRubbleFXAsset` C++-defaults to
  `/Game/EnvironmentVFX/VFX/EnvironmentDust/Systems/N_PebbleDust` (the project's existing generic
  debris-burst system), `GenericRubbleSoundAsset` to
  `/Game/NaPH_RPG_Fantasy_Sounds_Bunle/.../SC_Rock_Large_Debris_2-1_Cue` (existing rock-impact cue).
  Both `EditAnywhere` and soft-referenced, same C++-default-but-overridable pattern as
  `AGSTorchProjectile::FireVolumeClass`/`FlameSystem`.
- Updated `CrumblePieces`'s one summary log line ("released N of M... N have no fracture asset yet and
  simply stop where they stand") - that claim is no longer true; it now reports how many used the
  fallback instead.

## Evaluate

- **Watched live in PIE by Michael**: burned a house, reported "there's some smoke and it completely
  disappeared" - the fallback path (dust + destroy), not the real-fracture path, since this map's
  houses are near-exclusively the 41 unfractured `SM_MERGED_House_*` shapes.
- **Not directly confirmed**: the sound cue playing (Michael's report only mentioned smoke). Not
  chased further - a missing/wrong sound is a much smaller failure than a building not breaking at
  all, and the visual result (disappeared) is the part that was actually broken.
- **Not verified**: server/client replication of the fallback (`SpawnGenericRubbleFallback` runs
  wherever `CrumblePieces` runs, which is already gated `HasAuthority()` in `HandleCompleted` - but
  the piece's `Destroy()`/hide is not itself explicitly replicated the way the real-fracture path's
  `AGeometryCollectionActor` proxy is via `SetReplicates(true)`). Single-player PIE session only.
- Scope stayed inside the two claimed files - no other objective type (mill, statue, market) touched.
- **AGENT_STATE.md** gets a DECISION entry: the 42-vs-1 fracture-asset gap, the fallback chosen over
  bulk-authoring, and the replication caveat as a NEXT item.

## Refine

- Left deliberately undone: real fracture assets for the 41 missing `SM_MERGED_House_*` meshes. The
  fallback ships the release; better-looking collapses for those houses is follow-up content work, not
  a code fix.
- Left deliberately undone: verifying the fallback's `Destroy()` replicates correctly to a remote
  client - flagged above and in AGENT_STATE rather than guessed at, since this session never tested
  outside single-player PIE.
- Nothing else changed after the first pass - Michael's one-line confirmation ("smoke... completely
  disappeared") matched the intended behavior on the first watch.
