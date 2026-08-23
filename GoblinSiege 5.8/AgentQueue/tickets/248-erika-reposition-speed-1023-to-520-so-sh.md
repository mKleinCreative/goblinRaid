---
id: 248
title: Erika reposition speed 1023 to 520 so she reaches the run clip instead of sliding 2.5x
agent: claude-acf
status: done
claimed: 2026-08-21T22:54Z
build: none
waiting_on: SUPERSEDED by #250 - 520 fixed one archer and not the other; the measurement said walk, not run. Speed is now 200.
evaluated: 2026-08-22T01:28:36Z
observed: 2026-08-22T03:55:53Z | 520 fixed one archer and left the other at SQUARE, which is what sent us to 200 - the number itself was superseded but the measurement was the useful output
scenario: 8s capture, two archers, one in band and one repeatedly leaving it
files: 
  - Content/Blueprints/Adversaries/BP_ErikaArcher.uasset
---

## Goal

Erika sprinted at 1023.48 uu/s against `A_HU_Std_RunF`, authored at 406.9 - a 2.51x footslide, and
short bursts that never let a stride complete.

## Generate

`BP_ErikaArcher` CDO `MaxWalkSpeed` 1023.48 -> **520.0**.

520 rather than Michael's first instinct of ~400: `Walk -> Run` is entered at `HU_Speed > 500`, so a
400 cap would have locked her out of the Run state entirely and left her sliding 2.4x on the WALK
clip (authored 167.5) instead - worse than the problem. 520 is the cheapest value that still reaches
the run clip. Slide 2.51x -> 1.28x.

Authoritative because `DA_Race_Human`'s Archer row has `MoveSpeed = 0`, which
`AGSEnemyCharacter` treats as "the archetype has no opinion and the Blueprint's value stands"
(`GSEnemyCharacter.cpp:85`). Verified by reading the row, not assumed.

## Evaluate

Written and saved: `save_asset` returned True, the `.uasset` mtime moved, and no dirty package
remains. **Not verified at runtime** - `EditorAssetSubsystem` has no `unload_asset` in this build, so
a true re-read from disk was not possible, and nobody has watched her move.

Untouched deliberately: `BP_CastleGuard01` (1210.40) and `BP_CastleGuard02` (1238.62) slide 3.0x
against the same clip. Michael's ruling was about the archer; the guards are a separate call.

Still open underneath this: `Walk -> Run` at 500 against a 407-authored clip means every human in the
Run state slides >=1.23x no matter what any individual speed is set to.

## Refine

Nothing changed on review. The one risk - that the archetype row, not the Blueprint, owned this
number - was checked before writing rather than after.
