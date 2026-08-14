---
id: 064
title: HUD stamina frozen: BindWidgetOptional properties are not BlueprintReadOnly, so WBP_GSPlayerHUD fails to compile
agent: claude-hudfix
status: done
claimed: 2026-08-07T06:14Z
build: required
waiting_on:
evaluated: 2026-08-07T06:16:06Z
files: 
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.h
---

## Goal

HUD stamina frozen: BindWidgetOptional properties are not BlueprintReadOnly, so WBP_GSPlayerHUD fails to compile

## Generate

Michael: "the hud isn't correctly representing the stamina". It was #054 that broke it.

**Root cause, in the compiler's own words** (`Saved/Logs/MyProject.log`, and reproduced by compiling
the widget again just now):

```
[Compiler] GSPlayerHUDWidget.StaminaBar is not blueprint visible
           (BlueprintReadOnly or BlueprintReadWrite). ...  Get StaminaBar
[Compiler] GSPlayerHUDWidget.StaminaText is not blueprint visible ...  Get StaminaText
```

`unreal.BlueprintEditorLibrary.compile_blueprint` on `/Game/UI/WBP_GSPlayerHUD` returns
**`BlueprintStatus.BS_ERROR`**.

#054 added `StaminaBar` and `StaminaText` as `UPROPERTY(meta = (BindWidgetOptional))` with **no
Blueprint visibility**. But `WBP_GSPlayerHUD`'s EventGraph still READS both names every Tick - it
polls the character's `SprintStamina`/`MaxStamina` and writes the widgets itself, because the C++
stamina component is not fed yet (the Blueprint migration from the approved plan is still
outstanding). An invisible property does not degrade quietly there; it is a compile ERROR, which
fails the ENTIRE widget Blueprint. With the widget uncompiled the Tick poll never runs, so the bar
sits at whatever C++ wrote once on bind - 100% - forever.

**Fix:** `UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "GoblinSiege|HUD")` on
both, plus a comment at the declaration explaining why it is load-bearing on these two specifically.

**Why only these two broke.** Every other binding in the class (`HealthBar`, `ClockText`,
`EndPanel`, `EndScoreText`...) is equally invisible, and none errors - because no Blueprint graph
reads them. I confirmed the widget's EventGraph reads exactly `StaminaBar`, `StaminaText` and
`DebugText`; `DebugText` is a pure Blueprint widget with no C++ counterpart, so it is unaffected.

## Evaluate

**The DIAGNOSIS is proven; the FIX is not compiled.**

Proven, with evidence:
- `compile_blueprint(WBP_GSPlayerHUD)` -> `BS_ERROR`, reproduced live.
- Two compiler error lines naming `StaminaBar` and `StaminaText` by name.
- `get_connections` shows the Blueprint's poll chain fully intact and wired:
  `Event Tick -> Cast To BP_GSPlayerCharacter -> Set Percent(StaminaBar)` fed by
  `SprintStamina / MaxStamina`, then `SetText(StaminaText)`, then `SetText(DebugText)`.
  So the display logic was never removed - it simply cannot run.
- In PIE, BP `SprintStamina` = 100 and the C++ component = 100 at rest, so both sources agree at
  full and the freeze is invisible until something drains.

**NOT COMPILED, and I did not force it.** The build gate is closed by three tickets - #062
(claude-move, which a climb-lip workflow is actively running against), #063 (claude-levelgen) and
this one - and the editor is open at PID 47924. Building would have required closing the editor out
from under a workflow in flight. `-IgnoreQueue` exists and I did not use it; that flag is Michael's
call and I have no standing to spend it on someone else's running work.

**So the HUD is still broken right now.** The header change is correct but inert until someone builds.

**A latent issue this fix does NOT remove.** Once compiled there will be TWO writers to the same two
widgets: the Blueprint's Tick poll (the real `SprintStamina`) and C++ `HandleStaminaChanged` (the
component, which nothing feeds and which therefore reads a constant 100). The Blueprint wins every
frame because C++ only writes on bind - `NativeTick`'s retry is guarded by
`if (!BoundCharacter.IsValid())`, so it fires once per character, not per frame. It is benign today
and becomes correct when the planned migration makes the component the single source. It would stop
being benign if anything ever starts feeding the component while the Blueprint still polls.

**Not verified:** that the bar visually tracks stamina after the fix. That needs a build, then PIE
with something actually draining the pool. I could not screenshot PIE to check - an unfocused editor
renders black, which is already recorded in AGENT_STATE.

**Owed AGENT_STATE.md** - FAILED, once confirmed: *a `BindWidget`/`BindWidgetOptional` UPROPERTY that
any Blueprint graph READS must be `BlueprintReadOnly`, or the widget Blueprint fails to compile
entirely and every graph in it silently stops running.* The symptom is a frozen HUD element, which
looks like a data bug rather than a compile failure.

**Touched outside the goal:** none. Only the claimed header was edited.

## Refine

- **Compiled the widget instead of reasoning about who wins.** I had a plausible theory - two writers
  fighting over one progress bar - and it was wrong. Both writers exist, but the Blueprint wins every
  frame and the C++ writes once. Chasing that theory would have produced a fix for a race that was
  not happening. One `compile_blueprint` call returned `BS_ERROR` and ended the guesswork.
- **Added `BlueprintReadOnly` rather than removing the C++ binding.** Deleting `StaminaBar`/
  `StaminaText` from C++ would also fix the compile and would undo work the approved plan wants -
  the component is meant to own this display once the migration lands. One keyword restores the
  Blueprint's access while keeping that path open.
- **Did not "fix" the double write.** Making C++ stop writing, or making the Blueprint stop polling,
  are both real changes to which system owns stamina, and that is the migration's decision to make.
  Silently picking one under cover of a HUD bugfix is how a plan gets half-executed by accident.
- **Did not build.** Three tickets hold the gate and a workflow is using the editor. The fix being
  ready and the fix being live are different things, and saying so is more useful than a build that
  breaks someone else's run.

**Deliberately left undone:** the build; PIE verification that the bar tracks a draining pool; and
auditing whether any OTHER Blueprint in the project reads a C++ `BindWidget` property that lacks
Blueprint visibility - I checked this widget thoroughly but not the others, and the same trap would
present as a different frozen element.
