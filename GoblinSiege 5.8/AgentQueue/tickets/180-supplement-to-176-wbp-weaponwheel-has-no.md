---
id: 180
title: Supplement to 176: WBP_WeaponWheel has no Label_Grapple, and the 90-degree sectors left Bow and Sword labels in the old 120-degree positions
agent: claude-grapple
status: done
claimed: 2026-08-18T01:29Z
build: none
waiting_on:
evaluated: 2026-08-18T01:31:59Z
observed: 2026-08-18T01:37:42Z | Michael pressed Q and could see and select the Grapple sector, which he could not before this ticket - the label is drawn and the sector is reachable
scenario: Michael playing in PIE, opening the weapon wheel with Q and selecting the bottom sector
files: 
  - Content/UI/WBP_WeaponWheel.uasset
---

## Goal

Supplement to 176: WBP_WeaponWheel has no Label_Grapple, and the 90-degree sectors left Bow and Sword labels in the old 120-degree positions

## Generate

`WBP_WeaponWheel` gained a `Label_Grapple` TextBlock, styled off `Label_Torch` (Roboto Bold 24,
white, centred) so it cannot drift from its siblings, and **all four labels were repositioned**:

| label | was | now | sector |
|---|---|---|---|
| TORCH | (0, -200) | (0, -200) | top, unchanged |
| BOW | (+150, +120) | **(+200, 0)** | right |
| GRAPPLE | - | **(0, +200)** | bottom, new |
| SWORD | (-150, +120) | **(-200, 0)** | left |

All anchored to screen centre (0.5, 0.5) with alignment (0.5, 0.5), radius 200 - the radius
`Label_Torch` already used.

## Evaluate

**Michael pressed Q and reported the grapple was not there, which is how this was found.** The C++
in #176 was correct and live - `GSWeaponSlot.GRAPPLE` is in the built DLL and `SlotForDirection`
returns it - but the wheel is drawn entirely by these four TextBlocks, so a sector with no label is
a sector that does not visibly exist.

**The bigger half of the defect was not the missing label.** Changing the sectors from three
120-degree to four 90-degree ones silently invalidated the positions of the labels that already
existed: Bow's sector moved from bottom-right to right and Sword's from bottom-left to left, but
both labels stayed where they were. So even once Grapple appeared, two of the other three would have
pointed at the wrong quadrant - a player dragging left for the sword would have selected it while
the word SWORD sat at the bottom-left. **#176 changed the geometry in C++ and did not follow it into
the asset that draws it**; that is the actual mistake, and the missing label was only its most
visible symptom.

Verified by reading the slot offsets back out of the saved asset, not from the setter return values.
The widget preview render is white-on-white (the labels are white text) and shows nothing useful -
recorded so nobody mistakes that blank PNG for a failure.

## Refine

- Positioned by the same rule the sectors use (top/right/bottom/left, radius 200) rather than by eye,
  so the next slot is an entry in one table and one in the other, not a guess.
- `BindWidgetOptional` on `Label_Grapple` matches its siblings: a WBP that has not been re-authored
  still compiles and simply shows no label, which is how this degraded rather than crashed.
- Not done: the wheel has no artwork behind the labels, so the four sectors are still implied by text
  positions alone. That was true before this ticket and is out of its scope.
