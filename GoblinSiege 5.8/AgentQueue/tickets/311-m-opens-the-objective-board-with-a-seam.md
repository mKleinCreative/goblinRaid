---
id: 311
title: M opens the objective board, with a seam for the map it becomes
agent: claude-ui
status: done
claimed: 2026-08-26T01:16Z
build: required
waiting_on:
evaluated: 2026-08-26T01:42:16Z
observed: 2026-08-26T01:33:15Z | Pressed M in game and the objective board opened and closed
scenario: PIE raid, player character, M pressed during play
files: 
  - Source/GoblinSiege/Characters/GSPlayerCharacter.h
  - Source/GoblinSiege/Characters/GSPlayerCharacter.cpp
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.h
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.cpp
  - Content/UI/WBP_GSPlayerHUD.uasset
  - Content/Input/IMC_Default.uasset
  - Content/Input/IA_Map.uasset
---

## Goal

M opens the objective board, with a seam for the map it becomes

## Generate

Michael, on the first art pass: *"the UI is way too huge, at least the objective marker, maybe we
can make it openable with the tab key?"* - then, on checking: *"right now, tab goes betweens
weapons"*. Tab is IA_SwapWeaponMode. M was free and is the genre-idiomatic key, and he asked that
it be built as the seam for a map later: *"we can expand that later to a version of a map, make a
stub of that, but don't implement anything."*

`IA_Map` -> M in IMC_Default. `SetMapOpen` / `ToggleMap` / `IsMapOpen` on the HUD widget, board
CLOSED by default. `MapAction` + `Input_ToggleMap` on the character, mirroring the WeaponWheel
binding exactly. The whole HUD scaled down: bars 420->280, board 397x520->260x340, clock 300->230,
reticle 128->96, fonts 24->14-20; prompt plates later 400x159->260x103 and 560x351->380x238.

## Evaluate

**The map is a SEAM and nothing else.** `SetMapOpen` is the single function a map view has to hook.
There is no map widget, no map asset, and no reader of one. It is named for what it becomes rather
than what it is, because the caller is the M key and the key is the thing that will not be renamed.

**Two claims in the earlier notes turned out to be wrong, and both were worth checking rather than
inheriting:**

1. *"Canvas slots are unreachable from Python because WidgetTree is protected."* Wrong conclusion
   from one failed route. `CanvasPanelSlot.Content` IS protected - so reading canvas CHILDREN
   fails - but a widget's own `Slot` property is readable and hands back the slot by object path,
   and its `LayoutData` is fully settable. Go widget -> slot, never slot -> widget.
2. The plates had to be Collapsed with the text they back. A plate is a SIBLING on the canvas, not
   a parent, so nothing hides it automatically - the announcement banner would have sat on screen
   for the entire raid. `RebuildObjectiveList` above the early return, and the plate toggles beside
   each visibility call.

**Everything visual was judged from `WidgetService.capture_preview`, not from read-back.** That is
what caught the off-aspect plates, the permanently-visible banners, and text drawing on the frame
rails instead of inside them. Read-back would have reported all of it as correct.

## Refine

- The bars needed measuring, not a brush swap: the frame's channel is opaque wood, so the fill sits
  INSIDE the frame rather than behind it. Channel read off the art - health (179,67)-(786,167) of
  962x259, stamina (51,53)-(902,132) of 954x183.
- `bMapOpenByDefault` is EditDefaultsOnly, so the closed-by-default choice is one tick to reverse.
- Four attempts at measuring that channel algorithmically failed before reading it off a coordinate
  grid by eye. The lesson is the cheaper one should have come first.

