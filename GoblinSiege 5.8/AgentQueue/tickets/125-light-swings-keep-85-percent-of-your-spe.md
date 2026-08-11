---
id: 125
title: Light swings keep 85 percent of your speed so an archer can actually be chased down
agent: claude-animsmooth
status: done
claimed: 2026-08-10T23:48Z
build: none
waiting_on:
evaluated: 2026-08-11T00:28:43Z
files: 
  - GoblinSiege 5.8/Content/Blueprints/Abilities/Human/GA_HU_SwordLight.uasset
  - GoblinSiege 5.8/Content/Blueprints/Abilities/GA_GS_SwordLight.uasset
---

## Goal

Light swings keep 85 percent of your speed so an archer can actually be chased down

## Generate

Michael: *"it's almost impossible to hit an archer with a melee attack, because you don't walk and
attack at the same time. Is it possible to play only the top half of the melee attack while keeping
a 85% velocity moving with the bottom half?"*

The upper/lower split is a real job (see Refine). This ticket is the cheap half, done FIRST
deliberately so the expensive half can be judged as necessary or not.

**The premise needed correcting before acting on it.** You are not rooted while swinging today -
`MoveSpeedScale` was 0.55, so you already moved at 55%. It READS as rooted because the montage plays
full-body and overwrites the legs. So there are two separable causes, and raising the speed isolates
one of them.

`GA_HU_SwordLight` and `GA_GS_SwordLight`, all 3 stages each:

| | swing | recovery |
|---|---|---|
| stages 0,1 | 0.55 -> **0.85** | 0.75 -> **0.85** |
| stage 2 (Flurry) | 0.45 -> **0.85** | 0.65 -> **0.85** |

For the player at 470 uu/s: mid-swing 258 -> 400, recovery 352 -> 400.

**Recovery was raised too, not just the swing Michael named.** After #121 slowed the montages,
recovery is the LONGEST part of a swing (0.33-0.76s per stage against a 0.24-0.90s damage window),
so leaving it at 0.75 would have left most of a chase still slowed.

**`SwordHeavy` deliberately untouched at 0.35/0.55.** `FGSSwingStage`'s own comment says "a heavy
should feel like it plants you"; flattening every attack to the same mobility would erase a design
distinction to fix a light-attack problem.

## Evaluate

**Verified:** every stage re-read from the CDO after saving - all six at 0.85/0.85, both Heavy
abilities confirmed still 0.35/0.55. No rebuild; Blueprint defaults.

**NOT verified, and it is the whole point of doing this first:** whether it makes archers catchable.
That is Michael's call from play. Three outcomes and what each means:
- **Catchable now** - the upper/lower split drops to polish, not a fix.
- **Still not catchable, but the swing feels mobile** - the archers out-kite 400 uu/s and the answer
  is their standoff distance or kite speed, not animation. Relevant prior art: #111 gave archers a
  melee branch, #113 noted "archers never shoot while kiting".
- **Feels rooted despite moving at 400** - then it IS the full-body montage, and the split is the fix.

**Balance consequence not measured:** 0.85 during both swing and recovery makes the light attack far
less committal for the AI as well as the player - defenders now chase at 85% while swinging. Nobody
has watched what that does to crowding or to the stand-off slots tuned in #107/#108.

## Refine

**Nothing changed on re-reading.**

**Deliberately left undone - the upper/lower split itself**, scoped but not started. The findings
that matter for whoever picks it up:
- `ABP_Human` ALREADY has the plumbing: `Slot 'UpperBody'` feeding a `LayeredBoneBlend` at weight
  1.0, which nothing currently plays into. Attack montages use `DefaultSlot` (full body) instead.
- The blend's `LayerSetup` - the branch filter deciding WHICH bones the upper layer covers - is a
  node property and is NOT readable through the Python API available here. If it is empty, that slot
  is decorative and the filter needs authoring (root bone `spine`) by hand in the editor.
- `ThirdPerson_AnimBP_Gob` - the PLAYER's rig - has only `Slot 'DefaultSlot'`. No upper-body slot,
  no layered blend. That graph must be built from scratch, and it is the one that affects Michael.
- Montages are shared with the AI, so moving them to `UpperBody` gives defenders walk-and-swing too.
