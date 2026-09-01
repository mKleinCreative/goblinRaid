---
id: 393
title: Bulk Chaos fracture generation: custom editor module calling FractureEngine/PlanarCut C++ directly, real GC_ assets for all 42 house meshes
agent: claude-grapple
status: done
claimed: 2026-08-31T21:36Z
build: succeeded
waiting_on:
evaluated: 2026-09-01T00:20Z
observed: 2026-08-31T23:25:33Z | Burned houses near spawn (GSBuildingObjective_278/293, SM_MERGED_House_Small_18/Small_07 meshes); they now collapse into real fractured debris with correct materials and working collision - 'houses collapse right now and it looks really good'
scenario: Live PIE on L_Tutorial_Island, player character, torched two of the three houses closest to spawn/statue, real playtest by Michael
files: 
  - MyProject.uproject
  - Source/GoblinSiegeEditor/GoblinSiegeEditor.Build.cs
  - Source/GoblinSiegeEditor/GoblinSiegeEditorModule.h
  - Source/GoblinSiegeEditor/GoblinSiegeEditorModule.cpp
  - Source/GoblinSiegeEditor/GSFractureToolsLibrary.h
  - Source/GoblinSiegeEditor/GSFractureToolsLibrary.cpp
  - Source/GoblinSiege/Destruction/GSBuildingObjective.h
  - Source/GoblinSiege/Destruction/GSBuildingObjective.cpp
---

## Goal

Bulk Chaos fracture generation: custom editor module calling FractureEngine/PlanarCut C++ directly,
real GC_ assets for all 42 house meshes.

Superseded a rejected middle path first: #392's original fallback for the 41 of 42 `SM_MERGED_House_*`
meshes with no fracture asset went through two live-tested failures - v1 (hide+dust+Destroy) read as a
building vanishing with a floating smoke puff ("This is not an acceptable outcome"), v2
(physics-topple, same pattern as `UGSInteractableComponent::ApplyCollapse`) either launched a whole
merged house into the air (too much impulse to visibly rotate a house-scale rigid body) or silently
failed on individual kitbashed trim pieces shipping `ComplexAsSimple` collision (physics cannot
simulate on that collision type at all - confirmed live via log spam and "I can just walk through it").
Michael: "do what I originally planned and do it in bulk passes... this needs to be a process that
runs eventually after a level and houses get generated."

## Generate

1. **New editor-only module `GoblinSiegeEditor`** (`Source/GoblinSiegeEditor/`, registered in
   `MyProject.uproject`'s `Modules` as `Type: Editor`) - kept separate from the runtime `GoblinSiege`
   module so `FractureEngine`/`PlanarCut`/`DataflowCore` are never linked into a shipped build.
   Enabled the `Fracture` and `PlanarCut` plugins in `MyProject.uproject` (both `"Installed": false`/
   `"EnabledByDefault": false` in the engine, but their binaries are already compiled and shipped -
   confirmed by reading the installed engine's actual headers, not assumed) with
   `TargetAllowList: ["Editor"]` so they never enter a packaged build either.
2. **`UGSFractureToolsLibrary`** (`GSFractureToolsLibrary.h/.cpp`), two `BlueprintCallable` functions,
   both Python-callable from VibeUE's `execute_python_code`:
   - `GenerateFractureAsset(SourceMesh, DestFolder, NumVoronoiCells, bOverwriteExisting)` - builds a
     `UGeometryCollection` from a `UStaticMesh` and fractures it, using the SAME two calls the project's
     own hand-made Dataflow graph (`DF_GS_HouseFracture`, behind the one pre-existing
     `GC_MERGED_House_Small_03`) wraps: `FGeometryCollectionEngineConversion::AppendStaticMesh` (mesh
     -> collection, building directly on the destination asset) then
     `FFractureEngineFracturing::UniformFracture` (the Voronoi cut). Idempotent by default - skips a
     mesh that already has a `GC_` asset, which is what makes the bulk function below safe to re-run.
   - `BulkGenerateMissingBuildingFractures(SourceFolder, DestFolder, NamePrefix, NumVoronoiCells)` -
     scans the Asset Registry for meshes matching a name prefix and generates only the ones missing a
     fracture asset. **This is the reusable process Michael asked for**: re-running it after new house
     meshes are added only touches the new ones.
3. **Two real bugs found and fixed across two live-tested rounds** (not guessed, each one root-caused
   from an actual playtest):
   - Round 1 ("the buildings didn't break, they just changed their materials into all different kinds
     of materials"): the first version used the standalone
     `FGeometryCollectionEngineConversion::ConvertStaticMeshToGeometryCollection` and then manually
     assigned its `OutMaterialInstances` to `NewAsset->Materials` - that conversion path builds its own
     internal per-face `MaterialID` indexing scheme that a bare array assignment afterward does not
     reproduce, so every piece rendered a materials-array entry that was not its own. Fixed by building
     directly on the destination asset via `AppendStaticMesh` instead (`bAddInternalMaterials=true`),
     which keeps geometry and materials in sync by construction - the same thing the project's own
     Dataflow "Static Mesh to Collection" node does.
   - Round 2 ("it has no collision, so I can just walk through it"): neither the conversion nor the
     fracture call generates collision geometry - confirmed by reading `SpawnCollectionProxy`'s runtime
     expectations and finding nothing in the generation pipeline ever touched
     `GeometryCollectionConvexUtility.h`. Fixed by adding
     `FGeometryCollectionConvexUtility::CreateNonOverlappingConvexHullData` after fracturing, which
     builds a per-piece convex hull - what the runtime's `"Destructible"` collision profile needs
     something to actually collide with.
4. **`AGSBuildingObjective::SpawnGenericRubbleFallback` reverted to non-physics** (hide + dust/sound +
   `Destroy()`, no `SetSimulatePhysics`) - the physics-topple version (from the #392-adjacent session
   immediately before this ticket) is gone entirely, not tuned, per the two live failures in the Goal
   section. Removed the now-dead `GenericRubbleToppleImpulse` tuning property with it. This function
   should rarely be reached at all now that 42 of 42 house meshes have a real fracture asset - it
   remains only as an honest degrade for a future mesh type that has not been through the bulk tool
   yet.
5. **Deleted and regenerated all 41 tool-made `GC_MERGED_House_*` assets** after the materials/collision
   fixes landed (the original `GC_MERGED_House_Small_03`, made by hand before this session, was left
   untouched) - the first batch was generated with the buggy code and had to be thrown away, not
   patched in place.

## Evaluate

- **Watched live in PIE by Michael, twice, on real houses near spawn** (`GSBuildingObjective_278`/
  `_293`, using `SM_MERGED_House_Small_18`/`Small_07`, chosen specifically as "within torch throwing
  distance of spawn" for cheap iteration rather than re-running the full 41-mesh bulk pass every
  round): "burned it, houses collapse right now and it looks really good."
- Iteration strategy matters here and is worth recording: after the second live failure, Michael
  redirected to "iterate on the building that's within torch throwing distance of spawn, then run it
  on the rest once it works on the three buildings closest to the statue" - fixing the two real bugs
  above was done by testing 2-3 nearby houses per round instead of the full ~10-minute, 41-mesh bulk
  regenerate every time, which is why the round-trip on each fix was fast despite the total tool being
  large.
- **Not yet re-run on ALL 42 after the final fix** in the sense of a fresh full-map playtest burning
  every single house - only the 2-3 nearest to spawn were watched directly. The full bulk regenerate DID
  run against the fixed code and completed cleanly (41 generated, 1 already had one, no warnings in the
  log about 1-piece stub collections), and since every mesh goes through the identical
  `GenerateFractureAsset` code path, the 2-3 watched houses are representative - but "representative"
  is not "watched", and this is recorded as the honest gap rather than rounded up to "all 42 confirmed."
- **Compiled clean on the first attempt** for the module/plugin scaffolding and the initial
  `GenerateFractureAsset` implementation - the header research pass done before writing any code (real
  installed-engine header reads for `FGeometryCollectionEngineConversion`,
  `FFractureEngineFracturing::UniformFracture`, `FDataflowTransformSelection`) paid for itself here,
  unlike the grapple ticket's first LNK2019 round.
- Touched outside the original claim: none - the two bug-fix rounds were both edits to files already
  in this ticket's claim (`GSFractureToolsLibrary.cpp`, `GSBuildingObjective.h/.cpp`).
- **AGENT_STATE.md** gets a DECISION entry: the `Fracture`/`PlanarCut` plugin-enablement + module
  pattern (real headers ship even when a plugin is `EnabledByDefault: false`), the
  `AppendStaticMesh`-not-`ConvertStaticMeshToGeometryCollection` materials lesson, and the missing
  convex-hull-generation step - all three are exactly the kind of "worked once, will be reached for
  again" facts that belong there, not just in this ticket.

## Refine

- Left deliberately undone: a full-map burn-every-house verification pass. Given the fix is uniform
  across all 42 assets and the representative sample looked correct, this was judged acceptable risk
  rather than re-running the ~10-minute bulk regenerate a third time for a confirmation that would not
  change the code either way - flagged above, not hidden.
- Left deliberately undone: `NumVoronoiCells` (currently a flat 8 for every mesh regardless of size) is
  not tuned per-mesh. Some generated collections had very different piece counts (119 to 686 pieces)
  purely from mesh complexity at the same site count - if a specific house's fracture density looks
  wrong by eye later, this is the number to revisit, the same "tune by eye, do not guess twice" pattern
  as this class's other tuning fields.
- Nothing changed in the module/plugin scaffolding after the first compile - it was correct on the
  first attempt.
