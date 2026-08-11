---
id: 127
title: Hill arena: a calibrated slope range for testing melee up and down gradients
agent: claude-arena
status: done
claimed: 2026-08-11T00:34Z
build: none
waiting_on:
evaluated: 2026-08-11T00:51:19Z
files: 
  - GoblinSiege 5.8/Content/Maps/Test/L_CombatArena_Hills.umap
---

## Goal

Hill arena: a calibrated slope range for testing melee up and down gradients

## Generate

Michael: *"if we had an arena map that had elevation too it. That's going to be a major test.
attacking up and down hills."* He chose graduated test ramps over organic terrain, and mirrored
combatants.

`L_CombatArena_Hills`, duplicated from `L_CombatArena` so it inherits the floor, sun, sky, skylight
and navmesh bounds rather than being authored empty.

**Every ramp rises exactly 400uu, so the ONLY variable between them is the angle.** That is the whole
design: when melee breaks, the answer is "it breaks at 45 degrees", not "it breaks on hills".

| ramp | angle | run | plateau |
|---|---|---|---|
| `Ramp_15_West` | 15 deg | 1493 | `Plateau_West` |
| `Ramp_30_East` | 30 deg | 693 | `Plateau_East` |
| `Ramp_45_North` | 45 deg | 400 | `Plateau_North` |
| `Ramp_60_South` | 60 deg | 231 | `Plateau_South` |

Plus `Step_Ledge_100`, a 100uu lip for swinging over, and a 1600-wide flat fight pit at the centre.

Angles chosen against constants already in the project, not for looks: goblins walk to **65 deg**
(their claws, and the climb boundary, #084), so the 60 deg ramp should sit just inside goblin-walkable
and outside human-walkable. If the humans cannot take it, that is the map working.

Combatants mirrored per Michael: `Militia_1`, `Militia_2` and `Archer_1` hold `Plateau_East` at
Z=530; `Militia_3`, `Militia_4` and `Archer_2` stay in the pit at Z=130. One run therefore tests
uphill swings, downhill swings, and AI pathing in both directions.

`Hills_PlayerStart` sits dead centre of the pit facing +X up the 30 deg ramp. Placed there
deliberately: the pit radius is 800 and the camera trails ~450, so the camera stays on flat ground -
the #126 mistake, not repeated.

## Evaluate

**Verified by tracing the built geometry, not by trusting the arithmetic that placed it.** Line
traces down onto each ramp at 15%, 50% and 90% of its run returned heights matching prediction with
delta 0 (delta -7 at the top of the 60 deg, where the sample lands on the plateau lip). All four
plateau tops trace at Z=330 as intended and the pit floor is still at Z=-70.

**Angles re-derived from the traced heights rather than from the generator:** 30.0, 15.0, 45.0 and
59.4 degrees. Independent confirmation that the slabs are actually sloped, since a flat slab would
have shown no rise.

**A measurement I got wrong and am not reporting as fact:** the script also printed a "slope" column
read from the hit normal, which returned 180.0/0.0/0.0 - garbage from a mis-indexed hit tuple. The
height progression is the real evidence; the normal readings should be ignored.

**NOT verified:**
- **Navmesh.** The inherited `Arena_NavBounds` covers Z -500..1100 so the plateaus at 330 are inside
  it, but nobody has confirmed the navmesh actually generated over the ramps. **If the AI will not
  path up a slope, check this first** - it is far more likely than a behaviour-tree fault.
- **Nothing has been played.** No swing has been thrown uphill or downhill on this map.
- Ramp side edges are unguarded; a fighter shoved sideways off a ramp will fall to the floor below.
  Left that way - a fall is legitimate information about melee near a ledge.

## Refine

**Changed after the editor crashed.** The first attempt did `duplicate_asset` and `load_level` in one
script and took the editor down with a fatal `World Memory Leaks` assertion - switching worlds from
script while the old world is still referenced. Nothing was half-written; the duplicate never
reached disk. Rebuilt as three phases that never switch worlds from script: duplicate only, restart
the editor with the map on the command line, then spawn into the already-open level.

**Two guards added to the build script after that:** it aborts unless "Hills" is in the open level
name, so a wrong-map open cannot damage the flat arena; and it skips any actor label that already
exists, so a re-run cannot double the geometry.

**Deliberately left undone:** the two structural issues this map exists to expose are NOT fixed here,
because the map's job is to show them. Facing is yaw-only - nothing pitches a character toward a
target above or below, so a downhill swing may pass over a head. And the damage sweep is a sphere
trace of fixed radius, so uphill reach is whatever vertical slop that sphere gives. Expect both.
