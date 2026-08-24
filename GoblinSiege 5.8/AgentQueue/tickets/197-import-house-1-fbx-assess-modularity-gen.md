---
id: 197
title: Import House_1.fbx, assess modularity, generate variant houses in L_LevelGen_Scratch
agent: claude-housevariant
status: done
claimed: 2026-08-22T19:27Z
build: none
waiting_on:
evaluated: 2026-08-22T21:29:16Z
observed: 2026-08-22T21:30:08Z | Captured and opened a viewport screenshot framing House_1 and House_2 side by side: same roof, chimney, balcony and exterior-stair silhouette on both, and the right-hand house (House_2) visibly has an extra ground-floor window and a door-shaped opening where House_1 has plain wall
scenario: Editor viewport, L_LevelGen_Scratch, camera positioned to frame both outliner folders (House_1 and House_2) after the spawn script completed
files: 
  - Content/Maps/House_1.fbx
  - Content/Maps/L_LevelGen_Scratch.umap
---

## Goal

Import House_1.fbx, assess modularity, generate variant houses in L_LevelGen_Scratch

## Generate

Turned out `House_1.fbx` was a red herring - Michael clarified it was exported FROM the
level, not something to import. The real "House_1" is the 172-actor kitbash already sitting
in an outliner folder of that name in the open `L_LevelGen_Scratch` level, built entirely
from the DreamscapeFarmlands modular house kit (`/Game/DreamscapeSeries/DreamscapeFarmlands/
Meshes/Modular/House/...`, the same kit `level-gen/kit.json` catalogues).

VibeUE's `execute_python_code` tool (the one CLAUDE.md documents as "the workhorse") is not
actually registered on this editor's MCP server - confirmed by querying
`http://127.0.0.1:8000/mcp` directly (the same endpoint `gs_ue.py` uses) and getting back
only `list_toolsets` / `describe_toolset` / `call_tool`. So this ran entirely through the
`editor_toolset.*` toolset surface, no raw Python API.

Steps:
1. Surveyed the House_1 folder via `ProgrammaticToolset.execute_tool_script` (batched
   `SceneTools.get_actors_in_folder` + `ActorTools.get_components`/`get_actor_transform` +
   `ObjectTools.get_properties` across all 172 static-mesh actors, skipping the one
   DirectionalLight). Got a full manifest: mesh path + world transform per piece, 19 unique
   meshes (walls, roof tiles, foundation, floor, pillars, corners, balconies, stairs, doors,
   windows).
2. Cross-referenced `level-gen/kit.json` to find same-footprint alternates for the wall
   pieces actually used (all wall variants in a given letter-family share bounding box, so
   they're drop-in swappable at the same slot).
3. Wrote a second `execute_tool_script` that re-walked the same House_1 actors live (no data
   round-tripped through my context - the sandbox only allows `json/re/datetime/time/math/
   copy`, not file I/O, so a giant precomputed manifest couldn't be embedded) and, per actor,
   either kept the original mesh or substituted one of:
   - `SM_House_Wall_5x4_A_01` (plain) -> `SM_House_Wall_5x4_Window_C_01`
   - `SM_House_Wall_5x4_Window_A_02` -> `SM_House_Wall_5x4_Window_C_02`
   - `SM_House_Wall_5x4_Window_B_01` -> `SM_House_Wall_5x4_Door_B_01` (every other occurrence)
   then spawned the (possibly substituted) mesh via `SceneTools.add_to_scene_from_asset` at
   the same transform +2600cm on X, and filed it into a new `House_2` outliner folder.
4. Result: 172 actors created, 18 wall pieces swapped, 0 errors. Saved the level
   (`AssetTools.save_assets` on `/Game/Maps/L_LevelGen_Scratch`).

## Evaluate

**Verified, with real evidence:** captured a viewport screenshot
(`EditorAppToolset.CaptureViewport`) framing both houses side by side and opened it. House_2
sits cleanly beside House_1 with no interpenetration, same roofline/chimney/balcony/exterior
stair silhouette, and visibly different ground-floor openings where the wall swap landed
(an extra window and a door-shaped opening on the right-hand house that the left one doesn't
have). That is genuine piece-level variation, not a blind copy - I looked at the image and
that is what it shows.

**Not verified:** I did not open the level in a running PIE session or check collision/
navmesh on the swapped wall pieces - since the swap is a same-footprint mesh substitution on
non-structural decoration and the kit pieces already have their own collision, I judged this
low-risk, but it is asserted, not observed. I also did not verify interior readability (i.e.
whether the new window/door openings expose or block anything meaningful room-side) - this
was a cosmetic variant, not a gameplay-reviewed layout.

**What I touched outside the stated goal:** none of House_1's own actors were modified;
House_2 is purely additive. The ticket's original file claim listed `House_1.fbx`, which
turned out to be irrelevant to the real work (never imported, never referenced) - the actual
touched asset is `L_LevelGen_Scratch.umap`, already on the claim.

**Decision line for AGENT_STATE.md:** House_1 (the reference kitbash in L_LevelGen_Scratch)
can be varied cheaply by re-walking its actor list live through
`ProgrammaticToolset.execute_tool_script` and swapping same-footprint kit meshes per slot -
no FBX import, no C++, no `execute_python_code` needed. `execute_python_code` itself is
absent from this editor's live MCP server despite CLAUDE.md still describing it as available
- worth a real fix or a CLAUDE.md correction, not something I patched myself since I don't
know if the removal was deliberate.

## Refine

Left the wall-swap set intentionally small (18 of 172 pieces) rather than reaching for a
mirror/rotate/full re-skin pass on the first variant - the goal was to prove the mechanism
(read live actors -> substitute same-slot meshes -> respawn at an offset -> verify visually)
without risking a large blind edit on shared level content. Nothing else changed after the
Evaluate; the single swapped-wall variant is what's committed to the level. Further variants
(different swap sets, mirrored footprint, alternate roof style) are straightforward repeats
of the same script pattern and are left for a follow-up request rather than done speculatively
here.
