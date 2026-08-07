---
id: 046
title: Wire the wheel widget: UGSWeaponWheelWidget binds the slot delegates, character owns its lifetime
agent: claude-wheel
status: done
claimed: 2026-08-06T21:19Z
build: required
waiting_on:
evaluated: 2026-08-06T21:22:08Z
files: 
  - Source/GoblinSiege/UI/GSWeaponWheelWidget.h
  - Source/GoblinSiege/UI/GSWeaponWheelWidget.cpp
  - Source/GoblinSiege/Characters/GSPlayerCharacter.h
  - Source/GoblinSiege/Characters/GSPlayerCharacter.cpp
  - Content/UI/WBP_WeaponWheel.uasset
  - Content/Blueprints/BP_GSPlayerCharacter.uasset
---

## Goal

Wire the wheel widget: UGSWeaponWheelWidget binds the slot delegates, character owns its lifetime

## Generate

#045 left `WBP_WeaponWheel` as a correctly-arranged picture that nothing drove. This makes it live.

**`UI/GSWeaponWheelWidget.h/.cpp` (new UCLASS).** A `UUserWidget` that binds
`OnWheelOpenChanged` and `OnWheelHighlightChanged` in `NativeConstruct`, unbinds in `NativeDestruct`,
and recolours three labels. It **owns nothing** - it reads `GetWheelHighlight()` and
`IsWheelCommitted()` and draws them. The sector mapping stays in `SlotForDirection` alone; two copies
of that is how a preview starts lying about what the input will do.

- `BindWidgetOptional` on `Label_Torch` / `Label_Bow` / `Label_Sword`, not `BindWidget`. Strict
  binding makes a renamed label a compile error on the Blueprint pointing at C++; optional degrades
  to "that label does not light up" and warns once. Both the missing-component and
  missing-label cases log.
- Three colours: `HighlightColour` (committed), `UncommittedColour` (highlighted but still inside the
  dead zone), `IdleColour`. The middle one exists so **"drag back to centre cancels" is visible** -
  without it the wheel looks identical at rest and at commit, and a player cancelling would see their
  slot lit and reasonably think releasing had selected it.
- `HitTestInvisible` when shown, never `Visible`: the wheel is driven by mouse delta through
  `Input_Look`, and a hit-testable panel over the viewport would swallow the input it exists to draw.
- `Repaint` loops the enum rather than assigning three labels explicitly, so a fourth slot cannot
  leave one stuck on the previous frame's colour.

**`AGSPlayerCharacter`.** `WeaponWheelWidgetClass` (`EditDefaultsOnly`) plus a transient
`WeaponWheelWidget`, created once in `BeginPlay` for the locally controlled pawn and added to the
viewport at Z-order 10, collapsed. Created once rather than per gesture - Q is pressed often, and a
construct/destruct cycle each press would rebind the delegates every time. Left unset, the wheel
behaves exactly as it does today: selection, mesh swap and torch prop are all component-side.

**Chose C++ over authoring the graph.** `BlueprintService` can build it node-by-node, but binding two
delegates and recolouring three labels that way is dozens of pin-level calls whose failure mode is a
silently broken graph. `UMG`, `Slate` and `SlateCore` are already public dependencies and
`UGSPlayerHUDWidget` already uses this exact `UTextBlock` + bind pattern.

## Evaluate

**NOT COMPILED - the editor is open.** A new `UCLASS`, so this needs an editor-closed full build;
Live Coding cannot register it.

**Two editor steps remain after the build, and the feature does nothing without them:**
1. **Reparent `WBP_WeaponWheel` to `GSWeaponWheelWidget`.** It was created under `UUserWidget` in
   #045, so today it has none of this behaviour. Until reparented, the labels never bind.
2. **Set `WeaponWheelWidgetClass = WBP_WeaponWheel`** on `BP_GSPlayerCharacter`.

Both are MCP calls I can make once the DLL exists. Neither is done, so **"the wheel widget is wired
up" is not yet true from a player's seat** - it is true in code only.

**Nothing here has run.** No compile, no PIE. Specific things I cannot claim:
- That `GetOwningPlayerPawn()` is non-null in `NativeConstruct` at the moment the character creates
  the widget in its own `BeginPlay`. The pawn exists - it is doing the creating - but the widget is
  constructed against the PlayerController, and if the ordering is unlucky the component lookup
  fails. It logs loudly if so, which is why the warning is there rather than a silent null.
- That `HitTestInvisible` genuinely lets the mouse delta through. It should; it has not been tried.
- That the colours read clearly. There is still **no backing panel and no font styling** - #045
  flagged this and it remains true, so white-ish labels sit over bright terrain.

**Reparenting risk.** Changing a Widget Blueprint's parent class can drop widget-tree state in ways
the editor does not always warn about. The tree here is four components created an hour ago, so the
blast radius is nil - but if reparenting does clear it, rebuilding is the four `add_component` calls
in #045 rather than lost authored work.

**Touched outside the goal:** none. All six files claimed before editing, including the two content
assets I will need after the build.

**Owed AGENT_STATE.md** - nothing yet. The interesting entry, if the widget works, is that
`BindWidgetOptional` + a warn-once is the right default for authored UMG that C++ drives; holding it
until it has run, per the pattern that caught #043.

## Refine

- **Added the uncommitted colour after re-reading `CloseWeaponWheel`.** My first pass had two
  colours, highlighted and idle. But the highlight is seeded from the CURRENT slot on open, so inside
  the dead zone the wheel would show the player's existing slot lit and look exactly like a commit -
  making "drag back to centre cancels" invisible. The third colour is the only thing that
  distinguishes "this is where you are" from "this is what you will get".
- **Chose `BindWidgetOptional` over `BindWidget`.** Strict binding is the usual advice and is wrong
  here: it turns a designer renaming a label into a Blueprint compile error whose message points at
  C++. Optional plus a warning fails in the direction the person can act on.
- **Created the widget once instead of per gesture.** Per-press creation is simpler and rebinds two
  dynamic delegates on every Q, on a key meant to be tapped constantly.
- **Unbind explicitly in `NativeDestruct`.** The pawn outliving the widget is the normal case on a
  level transition, and a stale dynamic delegate pointing at a destroyed widget is a crash, not a
  leak.
- **Did not author the Blueprint graph**, having established `BlueprintService` could. The decision
  was reliability: a fragile graph that silently half-works is worse than a build cycle.

**Deliberately left undone:** the reparent and the class assignment (both need the DLL first); the
backing panel, font and any icon art; and an actual PIE pass, which is the only thing that will show
whether the wheel reads at a glance.
