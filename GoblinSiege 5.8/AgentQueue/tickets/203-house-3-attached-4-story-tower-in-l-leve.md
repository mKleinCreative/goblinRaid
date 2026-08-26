---
id: 203
title: House_3 + attached 4-story tower in L_LevelGen_Scratch_2
agent: claude-housetower
status: done
claimed: 2026-08-26T02:20Z
build: none
waiting_on:
evaluated: 2026-08-26T02:40:38Z
observed: 2026-08-26T02:40:51Z | Five viewport CaptureViewport screenshots opened and inspected directly: a close-up showing clean story-to-story wall seams at the corners with no gaps or z-fighting; a full elevation showing the tower standing flush against House_3 west wall on the same stone foundation line, four stories of windowed timber-frame walls; a roof-height shot showing the gable cap sitting proportionate on the first placement; a final shot after adding balconies showing a wraparound cantilevered gallery railing near the top, matching the signature feature of the user-supplied Frost-Wizards-Tower reference image
scenario: Editor viewport in L_LevelGen_Scratch_2, camera repositioned via SetCameraTransform/CaptureViewport to close, full-elevation, roof-height, and post-balcony angles around the new House_3_Tower outliner folder after each build phase
files: 
  - Content/Maps/L_LevelGen_Scratch_2.umap
---

## Goal

House_3 + attached 4-story tower in L_LevelGen_Scratch_2

## Generate

User asked to attach a 4-story tower to House_3 (`L_LevelGen_Scratch_2`), using the same
DreamscapeFarmlands house kit already cataloged in `level-gen/kit.json`. No round/turret kit
exists (confirmed by grepping `kit.json` for tower/spire meshes - only `SM_RoofSpike` and
interior-stair pieces turned up), so this is built from the same square wall/floor/foundation
family House_1-4 already use, stacked 4 stories instead of 1.

Surveyed House_3 live (37 actors, `get_actors_in_folder`/`get_actor_transform`/mesh lookups via
`ProgrammaticToolset.execute_tool_script`) rather than trusting `level-gen`'s own generator
script, since #201's ticket already documents this house as hand-edited (extra bay, duplicated
pieces reassigned between houses) and not a clean reference. Cross-referencing the west wall
line (`wall_win_13/16/17`, x=-1449.96) against a foundation piece's rotated local bounding box
confirmed the kit's yaw convention empirically (yaw+90 maps local (x,y) -> world (-y,x); pivot
sits far from the visible mesh - e.g. a foundation piece's own mesh sits ~280cm from its pivot at
yaw90) before placing anything of my own, rather than assuming an untested rotation handedness.

Built a fresh, symmetric 500x500cm tower (matching the kit's own module size) flush against
House_3's west wall line, centered on the middle bay: foundation ring (4x `SM_House_Foundation_5x4`)
+ per-story floor (`SM_House_Floor_5x4_01`) + 4-wall ring x4 stories (`SM_House_Wall_5x4_Door_B_01`
at ground-floor south for the entrance, `SM_House_Wall_5x4_Window_B_01` on all other faces/floors)
+ a `SM_House_Roof_02` gable cap (the same tiling+walltop technique #198 already validated for a
1-module-wide footprint - `Roof_01`'s corner pieces are too wide for a single module, confirmed
in that ticket and re-confirmed here by kit.json's own corner-piece sizes ~691-700cm). All 24
structural pieces placed by first-principles math (each piece's local min/max bounding box from
`kit.json`, rotated by the confirmed yaw formula, anchored so its own edge lands flush on the
tower's grid boundary) rather than copied from House_3's own noisy coordinates.

Mid-build the user shared a reference image (`Frost-Wizards-Tower.webp` - a round stone tower
with a conical spire and a cantilevered timber watch-gallery near the top). The available kit
has no cylindrical/stone-turret pieces, so an exact match isn't possible, but added the
reference's signature move - a wraparound gallery near the top - using `SM_Balcony_01` (the same
piece House_3's own wraparound balcony uses) on 3 of the 4 top-story faces (skipped the face
facing House_3, matching House_3's own precedent of leaving one side balcony-free where it
wasn't useful).

Total: 28 structural actors + 3 balcony actors = 31 actors, all filed into a new `House_3_Tower`
outliner folder, saved to `/Game/Maps/L_LevelGen_Scratch_2`.

## Evaluate

**Verified, with real evidence:** five viewport screenshots taken across the build (not just a
final one) - a close-up confirmed clean story-to-story wall seams with no gaps/z-fighting at the
corner joints; a full-elevation shot confirmed the tower stands flush against House_3's west
wall on the same foundation line, four stories tall with windows on every face; a roof-height
shot confirmed the gable cap sits proportionate on the first placement (no oversized-corner or
double-gable failure like #198 hit, because the first-principles math anchored eave/ridge
positions directly rather than eyeballing them - though this is one data point, not proof the
technique generalizes); a final shot after adding balconies confirmed the wraparound gallery
reads clearly against the reference image's intent (cantilevered railing near the tower top).

**Not verified:** collision on the roof tiles or balcony pieces, walkability of the interior
floors, or anything in PIE - this is an unoccupied static-mesh assembly, matching every prior
house-gen ticket's own disclosed scope. **There is no interior vertical circulation** - the 4
floor slabs exist but nothing connects them (no stairs, no floor cutout for a stairwell). The
kit has interior stair pieces (`SM_Stairs_Interior_*`) but placing them correctly (plus cutting
a floor opening) is a distinct sub-problem this ticket did not attempt; a player or AI cannot
currently reach stories 2-4 by any in-game means. This is a real gap, not a rounding error - flag
it before anyone assumes "4-story tower" means "4 reachable stories."

**What I touched outside the stated goal:** nothing - House_3's own 37 actors are untouched;
everything created lives in the new `House_3_Tower` folder.

**Decision line for AGENT_STATE.md:** the kit's yaw90 rotation maps local (x,y) -> world (-y,x),
and every wall/foundation/floor piece's pivot sits well outside its own visible mesh (confirmed
via kit.json's own local min/max bounds) - placement math must account for this offset per piece
type, not assume the actor transform is the mesh location. This kit has no round-tower/turret
family; a request for a stone/cylindrical tower silhouette cannot be met from
DreamscapeFarmlands alone.

## Refine

Left interior stairs/floor-cutouts undone rather than attempting them speculatively - the
existing kit's interior stair pieces and their correct placement relative to a cut floor opening
are not something any prior ticket in this project has solved or validated, and guessing at it
risked a worse outcome (a broken-looking stairwell) than clearly disclosing the gap. Also left a
spire finial (`SM_RoofSpike` exists in the kit) off the roof cap - the gable-plus-gallery
combination already reads clearly as a distinct tower against the reference image, and adding an
unvalidated ornament piece was lower value than stopping here and letting the user weigh in.
