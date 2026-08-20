---
id: 173
title: Grappling hook prototype: throw, stick, rope appears (Blueprint only, no build)
agent: claude-grapple
status: done
claimed: 2026-08-17T23:48Z
build: none
waiting_on:
evaluated: 2026-08-18T01:23:21Z
observed: 2026-08-18T01:22:09Z | Threw the hook in PIE at a merged-hull house: it stuck to the wall, stopped dead, stood the rope off the facade by exactly 70.0uu (the ClimbWallOffset) and hung 16 tiling segments straight down 706uu to the ground with 0.0uu horizontal drift. Also watched three real bugs the earlier trace-only measurement could not have caught - the sphere had no collision at all, the hook collided with the players own capsule, and the rope hung slanted because RopeVector was applied in the projectiles rotated local frame
scenario: PIE on L_Tutorial_Island, hook launched at SM_MERGED_House_Medium_01 from 700uu off the wall; rope re-created in the editor world at the measured endpoints and photographed hanging down the facade
files: 
  - ArtSource/Props/Rope/build_rope.py
  - ArtSource/Props/Rope/SM_Rope_Segment01.blend
  - ArtSource/Props/Rope/SM_Rope_Segment01.fbx
  - ArtSource/Props/Rope/Textures/UV_Layout_Rope.png
  - ArtSource/Props/Rope/preview.png
  - Content/Props/Rope/SM_Rope_Segment01.uasset
  - Content/Props/Rope/MI_GS_Rope.uasset
  - Content/Blueprints/Grapple/BP_GrappleHook.uasset
  - Content/Blueprints/Grapple/BP_GrappleAnchor.uasset
---

## Goal

Grappling hook prototype: throw, stick, rope appears (Blueprint only, no build)

## Generate
Blueprint-only prototype; **nothing was compiled** (build gate was CLOSED throughout — #169/#170/#171/#172 open).

**Art (new):** `ArtSource/Props/Rope/build_rope.py` -> `SM_Rope_Segment01` (50uu, 408 tris,
3-strand laid rope, 2 whole twist turns so it tiles head-to-tail). Imported to `/Game/Props/Rope/`.

**Material (new, ticket #175):** `M_GS_Rope` + `MI_GS_Rope`, flat colour, `bUsedWithSplineMeshes` on
**our own master** so the shared `M_Props_Master` was never touched.

**Blueprints (new):** `BP_GrappleAnchor` (InstancedStaticMesh rope, construction script derives
`ceil(|RopeVector| / SegmentLength)` instances) and `BP_GrappleHook : BP_GrappleAnchor`
(sphere + projectile movement; OnComponentHit derives wall normal, standoff, rope top and a
downward trace to the foot, then lays the rope on itself).

## Evaluate
**The measurement this prototype existed to make.** 64 rays, 16 merged houses in `L_Tutorial_Island`,
each traced twice from the same start: once against simple collision (what a projectile hits) and
once with `bTraceComplex=true` (the visible art).

| | uu |
|---|---|
| hull proud of art, median | **40** |
| mean | **96** |
| worst | **451** |
| never behind the art | max error +0.0 across all 64 |
| agrees within +/-20uu | **24 / 64 (37%)** |

**The simple hull is proud of the visible wall on 63% of approaches.** A hook anchored at the raw
`ImpactPoint` therefore floats visibly off the plaster — by 40uu typically and up to 4.5m. This
confirms and extends #077 (which measured 221uu on one wall); the tail here is twice that.

**Consequence for the design:** anchoring at the raw impact point is not viable. Two fixes, both
cheap: on hit, re-trace along the velocity with `bTraceComplex=true` and use that impact; and/or
derive the anchor from `UGSClimbLibrary::FindClimbLedge`'s deck as the plan already proposed. The
plan's mitigation is validated by measurement rather than by argument.

**Also found:** `T_Well_C` is **32x32** — a hand-painted palette atlas, not a detailed texture. The
original instruction (UV-map the rope onto the well's rope coil) would have given a ~2px-wide island
that bleeds neighbouring atlas cells under filtering and collapses at mip 2. Michael ruled for flat
colour instead; the rope's base colour is the measured median of the atlas's rope texels, `#9B7E57`.

**The hook HAS now been fired** (this line previously read "never been fired" — corrected once the
throw was actually watched). Firing it found three defects the trace-only measurement above could not
have caught, which is the argument for the whole "somebody must watch it" rule in one ticket:

1. **The sphere had no collision at all.** `BlueprintService.set_component_property` silently returns
   False for anything living in `BodyInstance` — profile, collision-enabled, notify-rigid-body — so
   every one of those settings was quietly dropped and the hook flew straight through the house. Now
   set from BeginPlay via `SetCollisionProfileName` / `SetNotifyRigidBodyCollision`, where it applies.
2. **The hook collided with the thrower's own capsule** and stopped 10uu from the muzzle. Fixed with
   the stock `IgnoreOnlyPawn` profile, which also matches the design's rule that pawns are vetoed.
3. **The rope hung slanted, not vertical.** `RopeVector` is applied in the actor's local frame and
   `bRotationFollowsVelocity` leaves that frame pitched, so "straight down" was straight down relative
   to the projectile. The 70uu standoff was skewed the same way. Zeroing the actor's rotation on
   impact fixes both at once.

**Remaining gap:** the throw was driven by setting the projectile's velocity from Python, not by a
player pressing a key — that path is #176's, and it is unobserved until someone selects the Grapple
slot on the wheel and presses attack.

## Refine
- Rope segment count is derived from `MaxRopeLength / SegmentLength`, not a third constant (#030).
- Straight rope needs no spline meshes; an ISM is one draw call and identical. Spline meshes only
  earn their cost if the rope sags.
- The rope-build loop is duplicated in the hook rather than shared, because construction scripts do
  not re-run at runtime and no `create_function` API was reachable. Acceptable for a throwaway
  prototype; the shipping version is `UGSRopeComponent` in C++ where the duplication does not exist.
- The hook's trace ships with `DrawDebugType=ForDuration` so the prototype carries its own instrument.
- I crashed the editor's Python interpreter with `TextureExporterPNG` (assertion `Texture != nullptr`);
  Michael approved the restart. Do not use that exporter from Python in this build.
