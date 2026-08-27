---
id: 314
title: The objective list becomes rows with type and state icons
agent: claude-ui
status: abandoned
claimed: 2026-08-26T02:24Z
build: required
waiting_on: Michael: the Hill Windmill row clips the board edge - shorten the name, widen the board, or drop a font point?
evaluated: 2026-08-26T02:48:38Z
observed: 2026-08-26T02:48:37Z | Opened the objective board in a raid and saw six rows draw with a state marker and a type picture each, Houses collapsed to 0 of 27
scenario: PIE on L_Tutorial_Island, raid running, board open, screenshot showui
files: 
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.h
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.cpp
  - Source/GoblinSiege/UI/GSObjectiveRowWidget.h
  - Source/GoblinSiege/UI/GSObjectiveRowWidget.cpp
  - Content/UI/WBP_GSPlayerHUD.uasset
  - Content/UI/WBP_GSObjectiveRow.uasset
---

## Goal

The objective list becomes rows with type and state icons

## Generate

Michael: *"let's work on objective row icons."* The 3 state icons and 5 type icons came in with
the UI kit (#310) and could not be used: the whole list was ONE TextBlock printing
`[x] Houses 12 / 27`, so there was nowhere for a picture to go.

- `FGSObjectiveDisplayRow` + `EGSObjectiveRowIcon` in `GSRaidTypes.h` - a row already collapsed,
  counted and pluralised.
- `UGSPlayerHUDWidget::BuildDisplayRows()` - the grouping pass, extracted from the string builder.
- `UGSObjectiveRowWidget` (+ `WBP_GSObjectiveRow`): StateIcon, TypeIcon, LabelText, DetailText.
- `ObjectiveList` VerticalBox on the HUD, `ObjectiveRowClass`, and `ObjectiveTypeIcons` as a
  `TMap<FGameplayTag, TSoftObjectPtr<UTexture2D>>`.

## Evaluate

**Seen running, not read back.** PIE on `L_Tutorial_Island`, board open, `screenshot showui`
(HighResShot renders no UMG). Six rows drew: The Wheat Field, The Windmill, The Hill Windmill,
The Market, `Houses 0 / 27`, Statue - each with a state marker and a type picture, under a
`BURN (0 / 5)` header. The house row collapsed to the REQUIRED 27, not the 67 that exist.

**The one design decision worth defending.** `OnObjectiveListChanged` already existed "for a
Blueprint that wants a real list", but it emits the RAW per-carrier rows - the collapse rule, the
required denominator and the over-completion clamp are applied later, while building the string.
A row list fed from that event would have had to re-derive all three. Each of those rules was
written to fix a specific misreading (`0 / 67` for a 40% rule; `34 / 27` looking like a broken
counter), and two consumers deriving them independently would eventually disagree - the player
would be told two different things about one objective. So the grouping pass was extracted and
BOTH renderings now read its output.

**Two bugs the runtime check caught that read-back could not have:**

1. `SetMapOpen` did not know about the new `ObjectiveList`, so the rows would have floated on
   screen with no board behind them whenever the board was closed.
2. Collapsing `ObjectiveListText` to make room for the rows silently dropped the `BURN (n/m)`
   header - the win condition, and the one number the raid is scored on. It now renders the header
   alone when the rows take over the body.

**Self-inflicted, recorded rather than buried:** the first build failed because a Python escaping
slip turned `TEXT("
")` into a literal newline inside the string - damaging, of all lines, the one
whose comment explains why it must be `
` and not `LINE_TERMINATOR`. Rebuilt the replacement from
character codes so no escaping layer remained.

## Refine

<!-- REPLACE: what you changed in response to your own evaluation, and what you
are deliberately leaving undone. "Nothing changed, and here is why the first pass
survives scrutiny" is a valid answer; silence is not. -->

> 2026-08-26T17:59Z Michael 2026-08-26: disregard it, it doesn't matter. Left by claude-ui with Refine unwritten and the HUD rows never watched in PIE; abandoned rather than done so the board does not claim a review that nobody did.
