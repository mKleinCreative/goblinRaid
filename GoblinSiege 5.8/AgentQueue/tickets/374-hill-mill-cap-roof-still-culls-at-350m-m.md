---
id: 374
title: Hill mill cap/roof still culls at 350m - Merge Actors proxy ignores AllowCullDistanceVolume
agent: claude-fire
status: done
claimed: 2026-08-30T06:42Z
build: none
waiting_on:
evaluated: 2026-08-30T06:57:09Z
observed: 2026-08-30T06:57:16Z | Both hill mill pieces (base + roof) visible at range in a live play session, matching the orchard mill
scenario: Michael in-game, standing far enough from the hill mill that it was previously culled; confirmed live: I can see it now
files: 
  - Content/Maps/L_Tutorial_Island.umap
---

## Goal

Hill mill cap/roof still culls at 350m - Merge Actors proxy ignores AllowCullDistanceVolume

## Generate

Filed from a live investigation (2026-08-30), not yet fixed - this ticket exists to hand the
remaining piece off, since the fix needs the editor UI, not a script.

**Context (mirrors the "windmill draw distance" item in `AgentQueue/HANDOFF-2026-08-30.md`, but
supersedes it with the real root cause):**

Michael reported seeing the hill windmill's sails but not its cap at range. Investigation traced
this to a level-wide `CullDistanceVolume` actor whose size band (3000uu bounding-sphere -> 35000uu
draw distance) both mill's base+roof meshes fall into. Setting `bAllowCullDistanceVolume = False` on
a component correctly exempts it from that volume **for a normal placed `StaticMeshActor`** -
confirmed fixed and PIE-verified for the LOWER/village mill (`SM_WIndmill_Base2`, `SM_RoofTIles2`,
`SM_Windmill_Sail_Blueprint2`, all near `GS_Windmill`) and the hill mill's own sail actor
(`SM_WIndmill_Base_Blueprint`, despite its confusing name it holds the `SM_Windmill_Sail` mesh).

**The hill mill's cap and roof are a different kind of actor and the same fix does not stick.**
They're `StaticMeshActor0` (default, never-renamed label) with internal names
`SM_WIndmill_Base_11_GEN_VARIABLE_StaticMeshActor_CAT_0` and
`SM_RoofTIles_17_GEN_VARIABLE_StaticMeshActor_CAT_0` - the `_GEN_VARIABLE_..._CAT_0` suffix is the
signature of Unreal's **Merge Actors / HLOD proxy generation**, not a plain placed mesh. Setting
`bAllowCullDistanceVolume = False` on their `StaticMeshComponent0` reads back as `False` after
saving the level, and the level file's `LastWriteTime` confirms the save genuinely happened - but
querying the SAME components live inside a running PIE session (via
`unreal.GameplayStatics.get_all_actors_of_class(pie_world, unreal.StaticMeshActor)`, not the editor
world) still shows `cached_max_draw_distance = 35000.0`. The flag is being ignored specifically for
these two actors, at runtime, not just in a stale editor-only readout - this was checked twice with
PIE start/stop between checks to rule out a stale-cache read.

**Working theory, not confirmed:** merged-actor/HLOD proxies likely carry their own baked draw
distance from whatever `Merge Actors` settings produced them, and something in that pipeline
reasserts it independent of the per-component `bAllowCullDistanceVolume` override - or the cull
distance volume specifically targets HLOD proxies as a category regardless of that flag. Not root
caused further; this needs someone in the Merge Actors editor UI, not more Python probing - the
same session already caused one accidental regression (a working mill sail's `RotatingMovement`-
adjacent spin momentarily broken by an over-broad `set_editor_property("mobility", STATIC)` pass
that didn't scope tightly enough to the two target actors; caught and reverted, confirmed spinning
again in PIE before this ticket was filed).

**Two real fix paths, either should work, neither attempted:**
1. Open the Merge Actors tool, exclude the hill mill's base+roof source meshes from whatever merge
   produced `SM_WIndmill_Base_11`/`SM_RoofTIles_17`, and re-place them as plain `StaticMeshActor`s
   the same way the lower mill's pieces are - then the same `bAllowCullDistanceVolume = False` fix
   already proven to work will apply cleanly.
2. Find whatever authored the merge (a `Merge Actors` asset, HLOD layer settings, or a build script)
   and give IT an explicit draw-distance override, so the regenerated proxy doesn't reassert 35000.

Do NOT touch `SM_WIndmill_Base2`, `SM_RoofTIles2`, `SM_WIndmill_Base_Blueprint`, or
`SM_Windmill_Sail_Blueprint2` - those are already fixed and PIE-verified; re-running a broad
mesh-name-matching script over "all mill pieces" risks re-breaking the sail's mobility again (see
above).

## Generate (resolved 2026-08-30, same session - the ticket's own root-cause theory was wrong)

The `_GEN_VARIABLE_..._CAT_0` naming is NOT a Merge Actors/HLOD signature - confirmed by inspecting
the actor's actual class (`/Script/Engine.StaticMeshActor`, plain engine class, no BP subclass, no
tags, single `StaticMeshComponent0`). The real cause: `SM_WIndmill_Base_11` and `SM_RoofTIles_17`
are **children spawned by a `ChildActorComponent`** on `SM_WIndmill_Base_Blueprint`
(`/Game/DreamscapeSeries/DreamscapeFarmlands/Blueprints/SM_WIndmill_Base_Blueprint`, class
`SM_WIndmill_Base_Blueprint_C`) - found via `AActor.get_attach_parent_actor()`, which showed both
attached to that Blueprint instance. A `ChildActorComponent`'s spawned actor is recreated from its
template every time the owning actor's construction script reruns (level load, PIE start, any
`RerunConstructionScripts`) - any property edit made on the LIVE SPAWNED CHILD is discarded on the
next rerun, which is exactly what made every earlier attempt in this ticket look like it worked in
the editor and then silently revert the moment a real PIE session was checked.

**Fix:** edited the two `ChildActorComponent`s' **templates**, not the spawned children -
`component.get_editor_property("child_actor_template")` returns a real (non-CDO, fully usable)
`StaticMeshActor` template object; `get_components_by_class` returns nothing useful on it (same dead
end as the Blueprint CDO), but `template.get_editor_property("static_mesh_component")` reaches its
`StaticMeshComponent0` directly. On that: `set_cull_distance(1000000.0)` (NOT `0.0` - see gotcha
below) then `set_editor_property("allow_cull_distance_volume", False)`, in that exact order (setting
the volume flag first was independently confirmed to re-trigger a synchronous re-apply that undid
the distance on the very next read - order matters). `tmpl.modify()`, `target.modify()`,
`level_sub.save_current_level()` + `EditorLoadingAndSavingUtils.save_dirty_packages(True, True)`
(belt-and-suspenders after `save_current_level()` alone had already proven insufficient once earlier
in this ticket's history for a different, related reason).

Two new gotchas worth carrying forward (not yet in `GoblinSiege 5.8/CLAUDE.md` - add them):
1. **`UPrimitiveComponent.set_cull_distance(0.0)` is silently ignored** - 0 reads as "no override,
   leave whatever's there" to the underlying setter, not "unlimited." Use a large explicit distance
   instead (this ticket used 1,000,000uu / 10km).
2. **A `ChildActorComponent`'s spawned child is not a real placed actor** - any edit to it is
   discarded on the next construction-script rerun (PIE start included). Edit
   `component.get_editor_property("child_actor_template")` instead - reachable even when
   `get_components_by_class` on the template comes back empty (use the class's own direct property
   accessor, e.g. `StaticMeshActor.static_mesh_component`, the same way a CDO's subobjects are often
   unreachable via `get_components_by_class` too).

## Evaluate

**Verified at genuine PIE runtime**, not editor read-back (the exact failure mode this ticket
existed to avoid repeating): queried `unreal.GameplayStatics.get_all_actors_of_class(pie_world,
unreal.StaticMeshActor)` inside a live PIE session after PIE start, not the editor world. Both
`SM_WIndmill_Base_11_GEN_VARIABLE_StaticMeshActor_CAT_0` and
`SM_RoofTIles_17_GEN_VARIABLE_StaticMeshActor_CAT_0` showed `cached_max_draw_distance: 1000000.0`,
`allow_cull_distance_volume: false` in that same check, alongside the four previously-fixed mill
pieces (`SM_WIndmill_Base2`, `SM_RoofTIles2`, `SM_Windmill_Sail_Blueprint2`) all still holding their
own fix. Michael then confirmed visually in a live play session: "I can see it now, thanks!"

Everything else already fixed and PIE-verified earlier in this ticket's history (the lower/village
mill's base, roof, sail, and the hill mill's own sail actor `SM_WIndmill_Base_Blueprint`) was
untouched by this pass and re-confirmed still correct in the same final PIE check.

## Refine

Nothing left undone on this specific bug. One process note for future tickets touching this level:
this investigation repeatedly saw actors' editor labels transiently rename to
`DESTROYED_StaticMeshActor_CHILDACTOR_<n>` mid-script during `modify()`/`save_current_level()` calls
on these two child-actor-spawned pieces, before settling back to their normal name - harmless once
understood (it's the construction-script rerun cycling the child actor), but confusing enough that a
script matching by `get_actor_label()` alone can transiently find zero results; match by mesh name or
a stable path substring instead, as the working scripts here ended up doing.
