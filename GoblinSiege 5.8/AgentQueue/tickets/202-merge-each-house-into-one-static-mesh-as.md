---
id: 202
title: Merge each house into one static mesh asset for manual FBX export
agent: claude-housevariant
status: done
claimed: 2026-08-23T02:57Z
build: none
waiting_on:
evaluated: 2026-08-23T03:00:01Z
observed: 2026-08-23T03:01:44Z | Opened an asset thumbnail capture of SM_MERGED_House_3 and it shows the whole cottage as one object: gable roof, timber-frame gable end, wraparound balcony railing on the visible side, and the ground-floor door opening, all in one continuous mesh silhouette with no missing or floating pieces
scenario: CaptureAssetImage on the merged /Game/Meshes/Houses/SM_MERGED_House_3 asset, opened and looked at directly
files: 
  - Content/Maps/L_LevelGen_Scratch.umap
  - Content/Meshes/Houses/SM_MERGED_House_1.uasset
  - Content/Meshes/Houses/SM_MERGED_House_2.uasset
  - Content/Meshes/Houses/SM_MERGED_House_3.uasset
  - Content/Meshes/Houses/SM_MERGED_House_4.uasset
---

## Goal

Merge each house into one static mesh asset for manual FBX export

## Generate

User wants the four houses exported as individual FBX files and pushed to the repo. Checked
first rather than assuming: no toolset registered on this editor's MCP server exposes FBX
export for static level geometry (`StaticMeshTools` has `import_file` but no export
counterpart; the only FBX-related toolset, `SequencerImportExportTools`, is for animation/
Sequencer data, not static meshes), and `execute_python_code` - which could do this via
`unreal.Exporter`/`AssetExportTask` - is the same tool already confirmed absent in #197. Gave
the user this constraint directly rather than attempting a workaround that would silently fail
or fabricate success, and offered a concrete middle path: I merge each house into one static
mesh asset (`SceneTools.merge_actors` IS available and needs no Python), so the user's manual
step is 4 right-click-Export actions in the Content Browser instead of hand-selecting
dozens-to-hundreds of actors per house. They chose that option.

Created `/Game/Meshes/Houses/`, then for each house folder: fetched its current actor list
(`get_actors_in_folder`) and called `merge_actors` with `destroy_source_actors=False` - the
individual pieces stay in the level, editable, exactly as before; the merge only ADDS one new
combined actor+asset alongside them, so nothing about the four houses' existing state is
disturbed. `merge_actors` silently double-prefixed the output name (my `.../SM_MERGED_House_1`
input became asset `SM_SM_MERGED_House_1`) - caught by checking `exists()` on the path I'd
asked for, which came back false; found the real path by reading the merged actor's own
`StaticMesh` component property, then used `AssetTools.move` to rename all four down to the
clean `SM_MERGED_House_N` name I'd intended. Verified the rename by re-running `exists()` on
the corrected paths (all four true) and pulling real triangle/vertex counts off each asset
(34266/50081 tris/verts for House_1 down to 13890/20881 for House_3) - substantial, not
trivial/empty meshes. Saved all dirty assets (the level plus the four new `.uasset` files) and
confirmed all four `.uasset` files exist on disk under `Content/Meshes/Houses/`.

Placed each merge's resulting actor into its own `House_N_Merged` outliner folder so it doesn't
get counted as house content by anything that reads the plain `House_N` folders (like #201's
clustering script, if it's ever re-run).

## Evaluate

**Verified:** all four `.uasset` files exist on disk (`ls Content/Meshes/Houses/` - real, so
they're either non-empty, non-zero byte files: 351KB-838KB each). Triangle/vertex counts read
back off the assets are non-trivial and vary sensibly with each house's actual piece count
(House_2, the largest at 181 actors, has the highest triangle count; House_3, the smallest
non-merged-anomaly house at 37, has the lowest). The rename was verified by `exists()` on the
new path, not assumed from the `move` call's boolean return alone.

**Not verified - and this is the real gap, not a rounding error:** I have NOT produced any
`.fbx` file. The stated goal ("export as fbx") is only half done by design - the merge is
scaffolding for a manual step the user agreed to do themselves, because no available tool can
finish it. Whoever reads this ticket alone, without the conversation, could easily think
"export" was completed; it was not, on purpose, and the user was told so directly before this
work started.

**What I touched outside the stated goal:** created a new `/Game/Meshes/Houses/` content
folder and four new merged-actor outliner folders. No existing actor was modified or removed
(`destroy_source_actors=False` was a deliberate, load-bearing choice, not a default I didn't
think about).

## Refine

No changes after the Evaluate. Left the double-prefix rename as a one-time fix rather than
investigating why `merge_actors` adds its own `SM_` prefix - worth knowing for whoever calls it
next (pass a name WITHOUT an `SM_` prefix and let the tool add it, rather than fighting it).
