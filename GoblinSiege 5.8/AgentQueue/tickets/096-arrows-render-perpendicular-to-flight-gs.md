---
id: 096
title: Arrows render perpendicular to flight: GS_Arrow's long axis is +Z, pivot at the tail
agent: claude-arrowfix
status: done
claimed: 2026-08-09T06:21Z
build: required
waiting_on:
evaluated: 2026-08-09T06:24:49Z
files: 
  - Source/GoblinSiege/Weapons/GSArrowProjectile.cpp
  - Source/GoblinSiege/Weapons/GSArrowProjectile.h
---

## Goal

Arrows render perpendicular to flight: GS_Arrow's long axis is +Z, pivot at the tail

## Generate

Michael: arrows fly with their heads pointing straight down, launched from a point that looks like
where an arrow standing straight up would sit.

`ArrowMeshOffset` defaulted to IDENTITY, and its own header comment admitted why: *"GS_Arrow's
authored orientation has not been confirmed in flight - if the arrow flies sideways or tail-first,
this is the knob."* `AGENT_STATE.md` carried the matching NEXT entry. Nobody had ever looked.

**Measured the asset instead of guessing.** `GS_Arrow` bounds: extent (1.4, 1.6, **29.75**), origin
Z = +29.75. So the shaft is **59.5uu long, its long axis is +Z, and its pivot is at the tail** - all
three of the reported symptoms in one set of numbers:

- The actor's +X follows velocity (`bRotationFollowsVelocity`), so a mesh whose length runs along +Z
  renders at ninety degrees to its own flight path - standing up out of the collision sphere.
- With the pivot at the tail, the whole shaft sat FORWARD of the sphere, so the arrow appeared to be
  launched from its own middle.

Corrected the constructor default: `Pitch -90` swings the mesh's +Z onto the actor's +X; `X = -59.5`
pulls it back by its own length so the HEAD sits on the collision sphere - the point of the arrow is
now the point that hits.

## Evaluate

**VERIFIED GEOMETRICALLY, not by eye** - measured on live arrows in PIE:

| | before | after |
|---|---|---|
| shaft vs. direction of travel | 90 deg | **0.0 deg** |
| arrowhead vs. collision sphere | 59.5uu forward | **0.0uu** |
| mesh origin (tail) | on the sphere | 59.5uu behind |

Checking the transform rather than taking a screenshot matters here: "looks about right" is what
produced the identity default in the first place.

**On "maybe they're much bigger than they're supposed to be" - the arrow is NOT oversized; the
goblins are undersized.** 59.5uu against each body that uses it:

- vs Erika (human, head 256uu above feet): **23%** of her height - if anything slightly short for an
  arrow.
- vs a goblin (head ~70uu above feet): **85%** of its height - an arrow nearly as long as the archer.

That is the same capsule/mesh mismatch recorded in #094, seen from another angle. **Not "fixed" by
shrinking the arrow**, which would make it wrong for the humans who are correctly proportioned.

**Not verified:** which END of the mesh is the head. Bounds give the axis and the pivot but not
which way the point faces, so if it now flies tail-first the fix is a 180 flip of Pitch and nothing
else - the length and axis above are measured facts. Nobody has looked at an arrow in flight since
the change; the numbers say it is right, a human eye has not confirmed it.

**Owed to `AGENT_STATE.md`:** the `BP_GS_Arrow` NEXT entry is now resolved - the offset is correct in
C++, so a Blueprint subclass is no longer needed to make arrows look right, only to make them
different.

## Refine

**Put the correction in the C++ default rather than making a Blueprint subclass.** The NEXT entry
suggested a BP so the knob would be editable without a rebuild, but a BP would also need a BP of
`UGSGA_BowShot` to point at it (the player uses the C++ ability class directly), which is two assets
and two more places to leave unset - for a value that is a measured property of the FBX and should
not vary per arrow.

**Deliberately left undone:** the goblin proportions (a content decision, #094), and a fletching or
trail VFX, which is what would actually sell an arrow in flight at these speeds.
