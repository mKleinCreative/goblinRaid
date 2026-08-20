---
id: 175
title: Supplement to 173: M_GS_Rope master material (flat colour, spline-mesh flag) so the shared Dreamscape parent is not touched
agent: claude-grapple
status: done
claimed: 2026-08-17T23:58Z
build: none
waiting_on:
evaluated: 2026-08-18T00:15:28Z
observed: 2026-08-18T00:15:26Z | Watched SM_Rope_Segment01 render with MI_GS_Rope applied in the static mesh editor and in the level - rope-coloured, three-strand twist visible, not engine-default grey (which is what a missing bUsedWithSplineMeshes looks like)
scenario: Static mesh editor thumbnail and level viewport capture, editor open on L_Tutorial_Island
files: 
  - Content/Props/Rope/M_GS_Rope.uasset
---

## Goal

Supplement to 173: M_GS_Rope master material (flat colour, spline-mesh flag) so the shared Dreamscape parent is not touched

## Generate
`Content/Props/Rope/M_GS_Rope` — flat-colour master (RopeColour / Roughness / Specular params),
`used_with_spline_meshes = true`, `used_with_static_lighting = true`. `MI_GS_Rope` instances it.

## Evaluate
Exists because `bUsedWithSplineMeshes` lives on the PARENT material, and `MI_Well`'s parent is
`M_Props_Master`, shared by the whole Dreamscape prop set. Setting the flag there to serve one rope
would have touched every prop in the village. Our own 3-node master costs nothing and touches nothing.

## Refine
Kept as a master + instance pair rather than a bare material so the colour can be retuned against the
village well without recompiling a shader.
