---
id: 306
title: The statue joins the objective list, and completing one says so
agent: claude-ui
status: done
claimed: 2026-08-25T07:02Z
build: required
waiting_on: needs an editor-closed build (GSPlayerHUDWidget.cpp changed), then Michael re-topples the statue and watches the row flip to Complete and the line appear.
evaluated: 2026-08-26T01:14:01Z
observed: 2026-08-26T01:02:14Z | Toppled the statue and its row on the objective board flipped from required to complete
scenario: PIE in L_Tutorial_Island, raid running, statue pulled down by the player
files: 
  - Source/GoblinSiege/Raid/GSRaidDirector.h
  - Source/GoblinSiege/Raid/GSRaidDirector.cpp
  - Source/GoblinSiege/Destruction/GSTopplableComponent.h
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.h
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.cpp
  - Content/UI/WBP_GSPlayerHUD.uasset
---

## Goal

Michael, having played #305: *"I wasn't able to burn the windmills of the 3 on the map. and the statue
wasn't listed up there with the objectives on the HUD."* Then: *"Make both windmill 1 and 2 on the
inside burnable as objectives, but only make one count as a primary, the other is secondary for
points."*

## Generate

**The statue was invisible BY CONSTRUCTION.** `GetObjectiveRows` walks `TrackedCarriers`, which is
`AGSBurnObjectiveBase` only - and a monument is not one. #305 made the statue count toward the win and
left it off the list, which is the worst pairing: **a requirement the player cannot see is one they
cannot plan around.** Monuments now emit rows too, Required until they fall and Complete after.
`UGSTopplableComponent::ObjectiveDisplayName` names them, falling back to the tag leaf so a monument
that counts is never a nameless row.

**A third bug, found by looking at the HUD rather than by being told.** The collapsed row counted GROUP
SIZE, so the 40% house rule read **"Houses 0 / 67"** when 27 would do. The player budgets the whole
raid against that number. `GetRequiredCountForType` now supplies the denominator, and the tally is
clamped so fire spread over-completing reads `27 / 27` rather than `34 / 27`.

**Objective prompts.** `UGSRaidDirector::OnObjectiveAnnounced` carries one finished line; the HUD shows
it for 4 seconds. **One delegate with a ready sentence rather than a family of typed events** - the
HUD's job is to show a sentence, and a HUD that assembled the wording would be a second place the
rules live. It says a tally while one is running (`Houses 12/27`), the type name when a type is
satisfied, and **"The way out is open"** when the raid's objectives complete - which is Michael's own
phrasing for the moment: *"the prompt to leave doesn't come back unless you've completed those
missions"*.

The timer **resets** rather than stacks: fire spread completes houses seconds apart, and each
announcement should hold for its full time rather than vanish on the first one's schedule.

**The second windmill.** `GS_Windmill_Hill` at (7494, 75496) - duplicated from `GS_Windmill` so every
tuned field came with it. **The mill type is REQUIRED again** (it was Optional for a few hours under
ruling 63) with NO fraction, which is exactly "one primary, one secondary": `RequiredCount` stays 1,
the first windmill to burn satisfies the type, and the demotion pass drops the other to Optional -
still burnable, still scored, off the critical path. **Ruling Q-32 survives here precisely because
this is the case it was written for.**

Which two are "inside" was read from distance to the village rather than guessed: Base2 at 20k uu and
Base_Blueprint at 30k, against Base3 and Base4 at 56k - both of those sit outside the boundary Michael
drew.

## Evaluate

**Build succeeded. Runtime, PIE on `L_Tutorial_Island`** - and this was verified by LOOKING at the HUD,
which took three attempts to manage:

```
BURN (0/5)
[] The Wheat Field   [] The Farm Field
[] The Windmill      [] The Hill Windmill
[] The Market        [] Houses  0 / 27
[] Statue
```

`Raid starting: 72 carriers across 5 types` - the new windmill, and the mill gating again.

**HighResShot RENDERS NO UMG**, which is why every screenshot this session showed a clean scene and no
HUD - including the ones used to sign off other work. `screenshot showui` captures the whole editor
window WITH the HUD. Worth knowing: an agent verifying a UI change with HighResShot will conclude the
UI is missing when it is fine, or fine when it is missing.

**NOT ESTABLISHED:** nobody has seen an announcement fire. The delegate is bound and the widget exists,
but `ObjectivePromptText` sits at the canvas default position because **`WidgetTree` is protected and
canvas slots cannot be set from Python** - so it needs a drag in the designer before it will read
properly.

## Refine

**Deliberately not done:** optional objectives still list identically to required ones - "The Windmill"
and "The Hill Windmill" look the same after one burns, though only one still matters. The list has the
state (`Optional`) and does not show it.

**Also not done:** the out-of-lives path, which Michael named as the other way the leave prompt returns.

## Observed failure, and the handover (2026-08-26, claude-ui)

Michael, having played this: *"I broke the statue, it didn't mark as complete when I completed
the objective."* Ticket reassigned from claude-warren to claude-ui on his instruction; it had
been STALE 16.7h and could not close against an observed defect.

**The backend was never wrong.** The log settles it:

    [00:27:28] Objective.Topple.Statue satisfied - 1 of 1 monument(s) cast down.

Registration ran, `HandleMonumentToppled` fired, the bucket completed. The binary was current
too - it built at 15:46, after the last source edit at 14:43 - so this was not stale code.

**The HUD list simply never rebuilt.** `RebuildObjectiveList()` was reachable only from
`HandleObjectiveListStateChanged` and `HandleObjectiveProgress`, both bound PER CARRIER in
`HandleObjectiveRosterChanged`. A monument is not an `AGSBurnObjectiveBase` and has neither
delegate, so nothing refreshed the list when the statue fell - it stayed Required until an
unrelated house burned and rebuilt it as a side effect.

The comment eight lines below the carrier binding had already identified this exact hazard for
the ANNOUNCEMENT - *"a monument is not a carrier... binding per-objective would have missed
both"* - and the list refresh was left carrier-only anyway. The lesson is that noticing a
hazard for one consumer of a signal does not fix it for the others.

**Fix:** `HandleObjectiveAnnounced` now calls `RebuildObjectiveList()` **above** its
`!ObjectivePromptText` early return. Above, because the list refresh must not be conditional on
the prompt widget being bound. It is the only director-bound path that sees an objective state
change, and every announcement is by definition one.


### ObjectivePromptText is placed (no longer Michael's job)

The earlier note said canvas slots were unreachable from Python because `WidgetTree` is
protected. That was the wrong conclusion from one failed route. `WidgetTree` is not needed: the
widget's own `Slot` property is readable and hands back the CanvasPanelSlot by object path, and
the slot's `LayoutData` is fully settable. (`CanvasPanelSlot.Content` IS protected - reading the
canvas children directly is the route that does not work. Go widget -> slot, never slot ->
widget.)

Placed top-centre, anchors (0.5,0) alignment (0.5,0), offsets L0 T150 W900 H60, z-order 10 -
clear of AlarmText, which ends at y112, and clear of the centre reticle. Read back from the
reloaded asset. `top` is the one number to change if it sits wrong on screen.

