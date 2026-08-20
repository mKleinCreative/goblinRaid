---
id: 183
title: Grapple hook renders red and weird: kill the 15s debug trace draw, and stop the placeholder cone standing upright on the wall
agent: claude-grapple
status: done
claimed: 2026-08-18T01:38Z
build: none
waiting_on:
evaluated: 2026-08-18T01:43:04Z
observed: 2026-08-18T01:43:05Z | Threw the hook at a test wall in PIE: no red debug lines, the hook mesh carries MI_GS_Rope, and the cone tip points into the wall at (1.00,-0.00,0.00) where it previously stood upright - while the rope still hangs with 0.0uu drift and a 70.0uu standoff, confirming the mesh rotation did not re-break it
scenario: PIE on the scratch Untitled level with a spawned floor and wall, hook launched at the wall face from 880uu
files: 
  - Content/Blueprints/Grapple/BP_GrappleHook.uasset
---

## Goal

Grapple hook renders red and weird: kill the 15s debug trace draw, and stop the placeholder cone standing upright on the wall

## Generate

Michael threw the hook and reported it "renders red and looks weird". Three causes, all mine:

1. **The red was my own instrument.** Both `LineTraceSingle` nodes were left at
   `DrawDebugType = ForDuration`, `TraceColor = (1,0,0,1)`, `DrawTime = 15` - so every throw painted
   two red trace lines and hit boxes for fifteen seconds. Set to `None` on both.
2. **The hook was a white cone** - `/Engine/BasicShapes/Cone` with no material assigned. Given
   `MI_GS_Rope`, so the hook reads as the same object as the rope hanging off it.
3. **The cone stood bolt upright on the wall.** This one was a side effect of the earlier fix: the
   rope only hangs true because the actor's rotation is zeroed on impact, and zeroing it also stood
   the mesh up. Separated the two concerns - the ACTOR stays unrotated for the rope's sake, and the
   HOOK MESH component is now rotated by `MakeRotFromZ(-WallNormal)` so its tip points into the wall.

## Evaluate

**Verified at runtime in PIE**, throwing at a spawned test wall rather than the hamlet (loading a
185MB level from Python is what crashed the editor earlier, so the test was built to not need it):

| | |
|---|---|
| cone tip direction | **(1.00, -0.00, 0.00)** - dot with the wall's +X inward axis = **1.0** |
| before the fix | would have been (0, 0, 1), straight up |
| hook material | `MI_GS_Rope` |
| both traces | `DrawDebugType = None` |
| rope still correct | 12 instances, **0.0uu** horizontal drift, **70.0uu** standoff |

The regression check matters as much as the fix here: rotating the mesh could easily have been done
by rotating the actor instead, which would have re-broken the rope. The 0.0uu drift is the evidence
it did not.

**What I did NOT verify: whether it now looks right.** "Looks weird" is a visual judgement and the
numbers above cannot close it. The hook is still a scaled engine cone - correctly oriented and
correctly coloured, but a cone. A real hook prop is Phase 7 art in the plan, and it is the most
likely remaining source of "weird".

## Refine

- Deleted the debug draw rather than gating it behind a cvar. It was a prototype instrument, it did
  its job (it is how the merged-hull offset got measured), and #012 had to strip exactly this kind of
  leftover out of `BP_GSPlayerCharacter` once already. If it is wanted again it is two pin values.
- Rotating the mesh component rather than the actor is the load-bearing choice: the rope's world
  geometry is derived in the actor's frame, so the actor must stay unrotated.
