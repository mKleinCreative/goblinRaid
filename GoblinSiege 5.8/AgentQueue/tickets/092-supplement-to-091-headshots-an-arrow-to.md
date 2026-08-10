---
id: 092
title: Supplement to #091 - headshots: an arrow to the head does extra damage
agent: claude-plate
status: done
claimed: 2026-08-09T03:25Z
build: required
waiting_on:
evaluated: 2026-08-09T03:32:14Z
files: 
  - Source/GoblinSiege/Weapons/GSArrowProjectile.cpp
  - Source/GoblinSiege/Weapons/GSArrowProjectile.h
---

## Goal

Supplement to #091 - headshots: an arrow to the head does extra damage

## Generate

An arrow to the head does `HeadshotMultiplier` (2.5x) damage. Scaled in `AGSArrowProjectile` rather
than in the damage pipeline because this is the only place that still holds the `FHitResult` - by the
time the effect executes, the bone that was struck is gone.

Pairs with #091: a bow already ignores plate, and a headshot turns "a bow shot placed at the gaps"
from a way through a knight's armour into a way to end him.

## Evaluate

**Written and compiled; the headshot itself has NOT been observed firing.** Driving the player's bow
needs a human on the mouse and I did not fake it, so `HeadshotMultiplier` is unproven in play. The
`[GS.Arrow] ... bone 'X' HEADSHOT -> N damage` line under `GS.Combat.Debug 1` is there to make the
first real shot self-verifying.

**Two silent-failure traps were found by inspection and closed - both would have shipped a feature
that never fired:**

1. **`Hit.BoneName` is `None` for these hits.** The arrow's collision sphere is `BlockAllDynamic`, so
   against a character it stops on the CAPSULE, not the mesh - and a capsule hit carries no bone
   name at all. Naively trusting `Hit.BoneName` would mean headshots never triggered, ever, with no
   error. Rather than change the character mesh's collision (the melee sweep, the ragdoll and the
   camera all depend on it), `ResolveHitBone` falls back to
   `USkeletalMeshComponent::FindClosestBone(Hit.ImpactPoint)` - the bone is resolved geometrically
   and the collision setup is untouched.
2. **Two skeletons, two spellings.** `GOB_Scout_v2` and `SK_Human_Skeleton` have different
   retargeting histories, and Mixamo-derived rigs prefix bones (`mixamorig:Head`). Matching is
   therefore case-insensitive SUBSTRING against `HeadBoneFragments` (`head`, `neck`), not an exact
   name that would work on one skeleton and silently not the other.

A `None` bone is explicitly not a headshot: a body with no bones to aim at should not award a bonus
for luck.

**Not verified:** the multiplier value (2.5 is a guess - it wants a human firing at a militiaman and
a knight), whether `FindClosestBone` picks a sensible bone for a torso hit at these capsule sizes,
and whether "neck" as a fragment is too generous.

## Refine

**Chose geometric bone resolution over changing collision.** Making the character mesh block
projectiles would give true per-bone hits, but it touches a setting three other systems read, and on
this machine that is a six-minute build to discover a regression. `FindClosestBone` is honest enough
for a 2.5x bonus and costs nothing anywhere else.

**Deliberately left undone:** headshots for MELEE (the sword sweep has no bone data at all - it is a
sphere overlap, and giving it one would be a real piece of work), and any headshot feedback beyond
the debug line - no distinct hit sound, no crit numbers.
