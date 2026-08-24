---
id: 200
title: Move selected House_4 actors into their own outliner folder
agent: claude-housevariant
status: done
claimed: 2026-08-23T02:14Z
build: none
waiting_on:
evaluated: 2026-08-23T02:24:24Z
observed: 2026-08-23T02:24:29Z | get_folders returned House_1 through House_4 after the move; the 46 moved actors mesh-checked as a coherent Dreamscape house kitbash at a location clear of the other three houses, not a stray subset of an existing one
scenario: Editor MCP queries against the live L_LevelGen_Scratch level, before and after the folder reassignment
files: 
  - Content/Maps/L_LevelGen_Scratch.umap
---

## Goal

Move selected House_4 actors into their own outliner folder

## Generate

User asked to move "the selected assets" into their own house folder - read the live editor
selection via `EditorAppToolset.GetSelectedActors` rather than guessing what was meant. It
returned 48 actors: 46 `StaticMeshActor`s plus `SkyAtmosphere_0` and `ExponentialHeightFog_0`
(level-wide environment actors, not house pieces - almost certainly swept in by whatever
selection method the user used, e.g. a box-select). Checked mesh assignments and world
positions on the 46 before moving anything: they're Dreamscape house-kit pieces (foundation,
floor, door/window walls, corners, balcony, brick stairs, Roof_02 tiling+walltop) forming one
coherent structure at x:[-124,949] y:[-2207,-185] z:[-47,948] - clear of House_1/2/3's regions,
so this is a genuinely separate fourth house, not a stray subset of an existing one. Some
piece labels (`roof2b_tile_N`, `roof2b_walltop_N`) match the naming this session's own #198
script used, suggesting the user duplicated House_3 in-editor as a starting point and moved/
edited the copy themselves - consistent with it sitting at a new location with new actor IDs.

Called `SceneTools.set_actor_folder(actor, "House_4")` on exactly the 46 StaticMeshActors -
deliberately excluded the 2 environment actors rather than filing global atmosphere/fog into a
per-house folder. Saved `L_LevelGen_Scratch`.

## Evaluate

**Verified:** `SceneTools.get_folders` now returns `["House_1","House_2","House_3","House_4"]`;
re-read is not needed beyond that plus the pre-move mesh/position check, since `set_actor_folder`
is a direct, unambiguous one-actor-at-a-time call with no transform or mesh side effects to
verify further.

**Not verified:** whether excluding the two environment actors from the move is what the user
actually wanted - they were included in the raw selection, so if the user's mental model treats
them as "part of the selection to move," this is a judgment call I made unilaterally. Said so in
my reply back to the user rather than silently deciding it was obviously correct.

**What I touched outside the stated goal:** nothing - only `FolderPath` changed on the 46
actors; no transform, mesh, or the 2 environment actors were touched.

## Refine

No changes after the Evaluate - the environment-actor exclusion is exactly the kind of
judgment call that's cheap to reverse if wrong (a single `set_actor_folder` call each), so it
was surfaced to the user rather than treated as something to iterate on unilaterally.
