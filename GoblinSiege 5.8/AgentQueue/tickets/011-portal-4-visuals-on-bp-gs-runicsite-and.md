---
id: 011
title: Portal 4 visuals on BP_GS_RunicSite, and the four missing HUD widgets
agent: claude-raid
status: review
claimed: 2026-08-05T23:25Z
build: none
waiting_on:
files: 
  - Content/Blueprints/BP_GS_RunicSite.uasset
  - Content/UI/WBP_GSPlayerHUD.uasset
---

## Goal

Portal 4 visuals on BP_GS_RunicSite, and the four missing HUD widgets

## Generate

- **`Content/Blueprints/BP_GS_RunicSite.uasset`** (new) — parented to `AGSRunicSite`, with
  `SM_Portal4` on `PortalMesh`, `M_Portal4` on slot 0, `N_Portal4_V2` on `PortalFX`. Created via
  `BlueprintFactory` + `AssetToolsHelpers`, assigned through the compiled class's CDO, compiled and
  saved.
- **`Content/UI/WBP_GSPlayerHUD.uasset`** — added four `TextBlock`s to `RootCanvas`:
  `ObjectiveListText`, `ClockText`, `LivesText`, `AlarmText`. Names match
  `UGSPlayerHUDWidget`'s `BindWidgetOptional` properties exactly. Laid out via
  `WidgetService.set_property` with `Slot.LayoutData.Anchors` / `.Offsets`, plus `Justification` and
  placeholder `Text`.

Commit `e1525bb`.

## Evaluate

**Verified, with evidence**
- Blueprint assignment read back off the compiled CDO rather than assumed: `VERIFY mesh=SM_Portal4`,
  `VERIFY niagara=N_Portal4_V2`.
- HUD binding by negative evidence: every prior PIE start logged four `LogGSHUD: widget ... is not
  bound` warnings; after the change a full PIE session logged **zero**.
- HUD content by positive evidence: PIE capture showing the clock at `29:24`, `QUIET` beneath it,
  `LIVES 5`, and `BURN (0 / 3)` listing The Wheat Field / The Farm Field / The Windmill / The Market.
  Names only, no arrows — GDD §2.1, decision 11.

**Never run**
- `BP_GS_RunicSite` has never been instantiated in a level. It compiles and its CDO holds the right
  assets; nothing has spawned one. It is currently **unused** — the level's runic site is the bare
  C++ actor sitting on Michael's own placed `SM_Portal4`.
- The four `OnRaidEnded` / `OnObjectiveListChanged` Blueprint hooks are still unimplemented in UMG,
  so the raid-end panel does not exist. Only the C++ text output has been seen.

**Where I was wrong**
- I reported the portal as "invisible in the level until swapped to this Blueprint" and repeated it
  across two messages. Wrong: the level already contained a visible `SM_Portal4` actor, placed by
  Michael, 87,584 uu from where I had put my runic site. I was describing my own actor's empty mesh
  slot while he was looking at his portal. One query for portal actors would have caught it; I
  reasoned from my code instead of surveying the level, and only ran that query after being told
  twice.
- Consequence: `BP_GS_RunicSite` is a solution to a problem that did not exist here. It is not
  wasted — a generated map has no hand-placed portal — but it was not what was needed.

**Owed to AGENT_STATE.md** — recorded: `WidgetService.set_property` silently returns `False` for
`Anchors`, `Offsets`, `Position`, `Slot.Anchors`; the accepted paths are `Slot.LayoutData.Anchors`
and `.Offsets`; and `WidgetBlueprint` does not expose `widget_tree` to Python at all.

## Refine

- Left `PortalMesh` unassigned on the placed site rather than swapping it to the Blueprint. Two
  portal meshes at one spot would z-fight, and the art placement is Michael's, not mine.
- Did not chase the layout further after three attempts at slot properties. Positions are readable
  in the capture and a designer can drag them in seconds; more scripted guessing was not worth it.
- Deliberately undone: the raid-end panel, and any use of `BP_GS_RunicSite`. It should be treated as
  unproven until a map actually spawns one.
