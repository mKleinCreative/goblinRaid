---
id: 095
title: Supplement to #094 - every arrow was a headshot: resolve by distance to the head bone, not nearest bone
agent: claude-archer
status: done
claimed: 2026-08-09T03:58Z
build: required
waiting_on:
evaluated: 2026-08-09T04:05:10Z
files: 
  - Source/GoblinSiege/Weapons/GSArrowProjectile.cpp
  - Source/GoblinSiege/Weapons/GSArrowProjectile.h
---

## Goal

Supplement to #094 - every arrow was a headshot: resolve by distance to the head bone, not nearest bone

## Generate

Replaced `ResolveHitBone` (nearest bone to the impact point) with `IsHeadshot`, which asks the real
question: did this land ON the head? It uses the engine's reported bone when the mesh was hit
directly, and otherwise measures the impact point against the head bone's world position, inside
`HeadshotRadius` (22uu).

## Evaluate

**#092 shipped a bug and this is it.** That ticket's own Evaluate said the multiplier was "unproven
in play" - it was worse than unproven. Giving archers a bow in #094 fired the first arrows anyone had
actually watched, and **every single one was a headshot**: a flat `raw 50.0` (20 x 2.5) on every hit.

The cause was `FindClosestBone(ImpactPoint)`. The arrow stops on the CAPSULE, so the impact point
sits on a cylinder around the body - and on these rigs the nearest NAMED bone to a point at roughly
chest height is the neck/head chain, so "closest bone" answered "head" essentially always. Asking
"which bone is least far away" is simply not the same question as "did this hit the head", and only
the second one is a headshot.

**Fixed and measured.** After the change the same test produced a real mix - `raw 20.0` body hits and
`raw 50.0` head hits, with the engine itself reporting `bone 'Head'` on the latter:

```
[GS.Arrow] BP_ErikaArcher_C_1 hit BP_HordeGoblin_C_1 (bone 'Head') HEADSHOT -> 50.0 damage
[GS.Arrow] BP_ErikaArcher_C_0 hit BP_GSPlayerCharacter_C_2 (bone 'None') -> 20.0 damage
```

**Still too frequent, and the cause is NOT detection.** 12 of the last 20 arrows were headshots even
after removing the aim loft. These are genuine mesh hits on the Head bone, so the shot line really is
passing through skulls - an aiming/geometry matter, not a detection one, and #094 records the likely
reason: the goblin meshes are a fraction of their capsules (head 40-70uu above the feet inside a
240uu capsule), so the relationship between "aimed at the capsule centre" and "hit the head" is not
what it should be anywhere in this game. **Not tuned further on purpose** - lowering the multiplier
would paper over a geometry problem rather than fix it.

**Owed to `AGENT_STATE.md`:** a FAILED line - "closest bone" is not "the bone you hit", and a
detection heuristic that can only ever answer yes is worse than none.

## Refine

**Deleted `ResolveHitBone` rather than fixing it.** It returned a bone name, which invited exactly
this error - any caller would reasonably believe the name meant something. A function that answers
the single boolean question cannot be misread the same way.

**Deliberately left undone:** the headshot RATE (a geometry problem, see #094), melee headshots (the
sword is a sphere overlap with no bone data at all), and any headshot feedback beyond the debug line.
