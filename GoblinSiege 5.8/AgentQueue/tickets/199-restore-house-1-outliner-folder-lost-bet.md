---
id: 199
title: Restore House_1 outliner folder (lost between #197 and #198), keep all three houses in separate folders
agent: claude-housevariant
status: done
claimed: 2026-08-23T00:38Z
build: none
waiting_on:
evaluated: 2026-08-23T00:48:19Z
observed: 2026-08-23T00:48:25Z | get_folders returned House_1, House_2 and House_3 after the fix; get_actors_in_folder on each returned exactly 172, 172 and 37 actors respectively with no overlap between the sets
scenario: Editor MCP queries against the live L_LevelGen_Scratch level, before and after reassigning House_1 membership
files: 
  - Content/Maps/L_LevelGen_Scratch.umap
---

## Goal

Restore House_1 outliner folder (lost between #197 and #198), keep all three houses in separate folders

## Generate

The user asked to "separate the houses into separate folders." Checked first rather than
assuming: `SceneTools.get_folders` returned only `["House_2","House_3"]` - `House_1` had
dropped off the registered folder list entirely, and `get_actors_in_folder("House_1")` errored
"Folder does not exist." The actors themselves were intact (`StaticMeshActor_1` still resolved
to `SM_House_Floor_5x4_73`, its known #197 label) - only the folder REGISTRATION was gone, not
the content. No other agent's ticket has touched this file since #198 closed, so this wasn't a
collision with concurrent work; most likely a level reload/save cycle dropped the folder's
registration even though nothing re-affirmed or touched its actors in between (unconfirmed -
not chased further, since re-deriving and reassigning correctly was cheap and root-causing an
editor quirk was not).

Recovered the exact 172-actor membership from `house1_manifest.json` (saved to the session
scratchpad during #197 - a JSON array of `{label, mesh, xform}` captured directly from
`get_actors_in_folder("House_1")` before it broke). Used `SceneTools.find_actors` with a
world-space bounding box around House_1's known footprint (x:-7500..-5700, y:-2000..450) to
get candidates (187 - some legitimate unrelated scratch-level content, e.g. a "ScratchGround"
plane and stray pillar/window pieces, sits in the same region), then intersected against the
172 saved labels via `ActorTools.get_label` and called `SceneTools.set_actor_folder(actor,
"House_1")` only on exact matches. Matched exactly 172 of 172 expected - the 15 excluded
candidates were confirmed to be labels never in the #197 manifest (not silently dropped).

Verified all three folders after: `House_1`=172, `House_2`=172 (both unchanged from their
build tickets), `House_3`=37 (unchanged from #198's post-cleanup count: 29 structural + 8
Roof_02 pieces after the WallSide removal). Saved `L_LevelGen_Scratch`.

## Evaluate

**Verified:** `get_folders` now returns `["House_1","House_2","House_3"]`;
`get_actors_in_folder` on each returns exactly the expected count for that house, with zero
overlap between folders (checked by construction - each house's actors were selected by a
disjoint world-space region and, for House_1, cross-checked against saved labels, not just
"whatever's nearby"). This is read-back-and-compared-to-a-known-count evidence, not just "the
tool returned success."

**Not verified:** WHY the folder disappeared in the first place. I did not reproduce the loss
or confirm a root cause, so it could recur. If `House_1` (or any folder) goes missing again,
the fix is the same: re-derive membership (by bounding box + label match, or transform match
against a saved manifest) and re-call `set_actor_folder` - the actors are never actually lost,
only the folder's registration.

**What I touched outside the stated goal:** nothing beyond folder assignment - no actor was
moved, renamed, or had its mesh/transform changed.

**Decision line for AGENT_STATE.md:** outliner "Folders" in this editor build can apparently
lose their registration independent of the actors that reference them (`get_folders` omitted
`House_1` while its 172 actors were provably still present and correctly labeled). Anyone
relying on an outliner folder to persist across a session boundary should treat it as
re-derivable, not guaranteed - keep (or regenerate) a label/transform manifest for anything
folder-organized that matters.

## Refine

Did not chase the root cause of the folder loss - re-deriving membership from already-saved
data was cheap and fully verifiable, while root-causing an editor/save-cycle quirk would have
cost much more for no change in outcome. No further changes after the Evaluate.
