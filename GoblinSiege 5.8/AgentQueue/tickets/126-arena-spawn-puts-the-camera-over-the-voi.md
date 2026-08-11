---
id: 126
title: Arena spawn puts the camera over the void: move the PlayerStart in off the rim
agent: claude-arena
status: done
claimed: 2026-08-11T00:26Z
build: none
waiting_on:
evaluated: 2026-08-11T00:28:10Z
files: 
  - GoblinSiege 5.8/Content/Maps/Test/L_CombatArena.umap
---

## Goal

Arena spawn puts the camera over the void: move the PlayerStart in off the rim

## Generate

Michael, mid-playtest: *"You're spawning me off the cliff."* Both the initial spawn and respawns, in
`L_CombatArena`.

**The pawn was never the problem.** Live PIE query at the original spawn:

```
PAWN   (-2600, 0, 52)   IsFalling=False   velocity=0   ground 122uu below a 120 capsule
CAMERA (-3050, 55, 117) <- 50uu PAST the floor edge at X=-3000
```

The third-person arm trails ~450uu behind the pawn. `Arena_PlayerStart` sat 400uu from the rim, so
the camera spawned OUTSIDE the arena looking back across the drop - the ground falls away behind you
and one step backwards walks off. Every spawn shares the PlayerStart, which is why it happened on
both paths.

Moved `Arena_PlayerStart` X from -2600 to **-2000**, chosen from the measured 450uu arm rather than
by eye: camera lands at -2450, 550uu inside the rim. Approach to the patrol is 2600uu, so #110's
"start the player across the arena from the patrol" still holds.

## Evaluate

**Verified in a live PIE run, before and after** - not from arithmetic:

| | before | after |
|---|---|---|
| pawn X | -2600 (400 from rim) | -2000 (1000 from rim) |
| camera X | -3050, past the edge | -2450, 550 inside |
| ground under camera | none | yes |
| IsFalling | False | False |

**Two wrong hypotheses, recorded because I would have shipped either as a fix:**
1. *"#110 moved the spawn past the floor edge."* Wrong - the floor is a Cube scaled 60x, spanning
   -3000..+3000, i.e. 6000 across. #113's note that "the floor is 3000" is what misled me; that
   figure is not the span. The pawn was always on the floor.
2. *"The floor has no collision and he falls through."* Wrong - one simple box, step-up enabled,
   visible, and `IsFalling` was False throughout.

Both were plausible, both were checkable, and both died on a measurement. The thing that actually
found it was querying the CAMERA rather than the pawn - reading the pawn position kept answering
"fine", because the pawn was fine.

**NOT verified:** how it feels to spawn there. The numbers are sound and Michael has not yet played
it. Also unexamined: whether any OTHER map spawns a player within one camera-arm of a drop. This
ticket fixed the arena only, and the same class of bug would look identical anywhere else.

## Refine

**Nothing changed on re-reading.** The alternative - shortening the camera arm, or pushing the arm
forward when it would clip past a ledge - is a real feature (camera collision already exists for
walls) but it is a much larger change than moving one actor 600uu, and the spawn being a step from a
cliff edge is worth fixing on its own terms regardless.

**Deliberately left undone:** the arena has no guard rail or kill volume. Walking off the edge is
still possible anywhere along the rim, and there is no respawn-on-fall - that is a level-design
question for Michael, not something to add unasked.
