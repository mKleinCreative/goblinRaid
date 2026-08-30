---
id: 365
title: Windmill draw distance: base/roof/sail cull inconsistently, roof invisible from a distance
agent: claude-fire
status: done
claimed: 2026-08-30T04:12Z
build: none
waiting_on:
evaluated: 2026-08-30T04:41:44Z
observed: 2026-08-30T04:41:45Z | Roof visible up close and confirmed on both mills after disabling dithered_lod_transition override on the three roof material instances
scenario: Live PIE, L_Tutorial_Island, both placed mills, human-observed on both roofs
files: 
  - Content/DreamscapeSeries/DreamscapeFarmlands/Materials/Structures/MI_TowerRoofTiles.uasset
  - Content/DreamscapeSeries/DreamscapeFarmlands/Materials/Structures/MI_TowerWood.uasset
  - Content/DreamscapeSeries/DreamscapeFarmlands/Materials/Structures/MI_TowerSpear.uasset
---

## Goal

Windmill roof (SM_RoofTIles) invisible at all times, up close and far - NOT a draw-distance issue
(original title was a wrong guess, corrected during investigation).

## Generate

**False start, corrected:** originally guessed this was a draw-distance/cull-distance mismatch
(base/roof cull at 350uu, one sail instance had no cull at all) and edited per-instance
`LDMaxDrawDistance` on the level's placed components. That edit turned out to be an inert no-op
(the authored value was already 0 both before and after) and was reverted via `git checkout` once
Michael reported the roof was invisible even standing right next to it - a symptom no draw-distance
setting could produce. Confirmed on the fully-reverted, clean level: still invisible, so this was
never caused by anything touched this session (separately verified by a subagent sweep of every
burn/crumble/fracture C++ file - zero references to this mesh, this material property, or foliage/
dither systems anywhere in `Source/GoblinSiege/Destruction/`).

**Actual root cause, found by elimination:** swapping the roof's three material slots
(`MI_TowerRoofTiles`, `MI_TowerWood`, `MI_TowerSpear` - all children of the shared
`M_Props_Master`) to a plain engine `DefaultMaterial` made the roof render immediately. Geometry,
position/bounds, visibility flags, scale, blend mode, WPO, ground-coverage switches, texture
assignments and shader compile were all individually checked and ruled out first - every one came
back clean, which is what made "swap the whole material and see" the actual decisive test.

Isolating one slot at a time (real `MI_TowerRoofTiles` + two `DefaultMaterial` slots) still broke
the WHOLE mesh, confirming the fault is a MATERIAL PROPERTY, not one section's content - a broken
section wouldn't take down sections rendering a completely unrelated material. The property:
`base_property_overrides.dithered_lod_transition`, inherited as `true` from `M_Props_Master`. Its
own doc string: "Whether the material should support a dithered LOD transition **when used with
the foliage system**." `SM_RoofTIles` is placed as a plain, non-instanced `StaticMeshActor` (or a
`ChildActorComponent`-spawned one, on the hill mill) - never as foliage/HISM - so the shader never
receives the per-instance dither data it expects and reads as permanently, fully dithered out.

Fix: added an INSTANCE-level `base_property_overrides` on all three material instances
(`override_dithered_lod_transition = true`, `dithered_lod_transition = false`), leaving the shared
`M_Props_Master` untouched (it's used broadly; this override is scoped to exactly the three
instances actually placed on non-foliage windmill geometry).

**Tooling gotcha worth keeping:** `MaterialInstanceBasePropertyOverrides.to_dict()` returns `{}`
even when the override IS correctly set - verify via `get_editor_property` on the individual named
fields (`override_dithered_lod_transition`, `dithered_lod_transition`), never via `to_dict()` for
this struct. Also: setting the struct's fields in-place then reassigning the same struct object did
NOT persist; constructing a fresh `unreal.MaterialInstanceBasePropertyOverrides()`, setting fields
on THAT, then assigning the fresh struct to the instance is what worked.

## Evaluate

**Verified live by direct observation, both mills, after the fix.** Michael: "it works, I can see
them" (ground mill), then confirmed "yes, both roofs work now" after checking the hill mill
separately - important because the fix was applied once, to the shared material assets, and needed
to be confirmed on BOTH placements (a plain `StaticMeshActor` and a `ChildActorComponent`-spawned
one) to know it wasn't specific to one placement method.

Saved to disk and confirmed via file timestamp on all three `.uasset` files (not trusted from the
Python return value alone - `save_loaded_asset` has been unreliable all session, see #363's ticket
for the same gotcha).

**Not fixed, flagged separately:** the original (wrong) draw-distance observation still has a real,
minor, LOWER-PRIORITY component underneath it - `SM_RoofTIles`/`SM_WIndmill_Base` cull at 350uu
while one sail instance has no cull distance at all, so from far enough away the windmill's
silhouette is inconsistent (sail visible, everything else gone). Now that the roof itself actually
renders up close, this is purely a "how far should a windmill read as a landmark" tuning question,
not a bug - not touched further this ticket since Michael's priority moved to the mill's ignition
difficulty.

## Refine

Closing `done`. The original ticket title and goal were a wrong initial diagnosis (draw distance) -
left the corrected framing in Goal/Generate above rather than silently rewriting history, since the
false start and how it was ruled out is itself part of the record (the git-revert-to-verify step is
what proved this session's own work wasn't the cause, which mattered given Michael's direct
suspicion "it's something you did").
