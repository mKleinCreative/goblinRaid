---
id: 281
title: Finite arrows stage 4 - the arrow count on the HUD
agent: claude-warren
status: done
claimed: 2026-08-24T09:05Z
build: required
waiting_on: "Michael: 20 seconds of looking. Press play and confirm the arrow count reads 30, drops as you shoot and jumps when you walk over a bundle. I could not reach the live widget object to read it programmatically - see Evaluate. Also: its position is a guess and wants moving by eye."
evaluated: 2026-08-24T16:25:15Z
observed: 2026-08-24T17:19:18Z | Michael played it and the arrow count reads and updates on screen. He also found what the count made obvious: with zero arrows the bow still draws and aims, and only the loose is refused, so the bow reads as broken rather than empty.
scenario: Michael playing in PIE.
files: 
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.h
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.cpp
  - Content/UI/WBP_GSPlayerHUD.uasset
---

## Goal

Stage 4, and the half that makes finite arrows playable rather than a surprise: until the count is on
screen, running dry is something that happens *to* the player rather than something they can plan
around.

## Generate

**`UGSPlayerHUDWidget::ArrowCountText`** - a `UTextBlock` with
`UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))`. `BlueprintReadOnly` is mandatory rather
than house style: #064 was a **whole-widget** compile failure from omitting it on a binding a designer
graph touched, and it surfaces as a red widget in the editor rather than as a C++ error, so it can
survive a clean build.

**`HandleInventoryChanged()` / `RefreshArrowCount()`**, wired in `BindToCharacter` following the
stamina and bow-timing blocks precisely, because that shape encodes two lessons the widget already
learned:

- **Unbind from the OLD character first.** The raid respawns you up to five times; without it, five
  dead pawns' inventories all write to one text block. Added to the same unbind block as stamina.
- **Paint immediately after binding**, and *outside* the `if (Inventory)` block, so a character with
  no inventory gets the text collapsed rather than inheriting the previous pawn's number.

**`OnInventoryChanged` rather than `OnItemAdded`/`OnItemRemoved`.** It is parameterless, fires on
every mutation including `OnRep_Inventory` on clients, and the handler re-reads the total anyway - so
the delta the other two carry is not wanted. No tick, no polling.

**Which item counts as "arrows" is read from the equipped bow**, the same place
`UGSGA_BowShot::GetAmmoItemClass` reads it. Asking the same source means the HUD cannot display a
number the bow does not actually spend.

**Collapsed when there is no ammo concept** - an AI archer, or a bow with no `ArrowItemClass`. Same
structural rule as `ShowBowTimingBar(false)`: nothing to ask means nothing to show, rather than a
second condition to keep in sync. Deliberately collapsed rather than showing `0`, because an
unlimited bow reading "0" is a worse lie than no readout.

**`Content/UI/WBP_GSPlayerHUD`** gains a `TextBlock` named exactly `ArrowCountText` on `RootCanvas`,
alongside `LivesText` / `ClockText` / `AlarmText`.

## Evaluate

**Build succeeded in 51s**, editor closed, only the two pre-existing `C4996 AbilityTags` warnings.

**Verified on the asset:**

- `WBP_GSPlayerHUD` compiles - `BS_UP_TO_DATE`. **This is the #064 check** and it is the one that
  fails as a red widget rather than a build error.
- `ArrowCountText` exists as a `TextBlock` on `RootCanvas` with **`is_variable = True`**, which is
  what `BindWidgetOptional` needs in order to resolve at all.
- The generated class exposes the property, so the C++ side sees it.

**NOT VERIFIED, AND THIS IS THE HONEST GAP: nobody has seen the number on screen.** I could not read
the live widget instance programmatically - `get_live_property` needs a `PIEWidgetHandle` I could not
obtain, `get_widget_snapshot` returns zero entries while PIE is running, and there is no
`WidgetBlueprintLibrary` in this build to enumerate live user widgets. A viewport capture returned the
**editor** viewport with debug annotations rather than the game's HUD.

So what is proven is that the binding *can* resolve and the widget *does* compile. Whether the number
appears, updates and survives a respawn is exactly the sort of thing this project's queue insists
somebody watch - and I stopped trying to fake that rather than stacking more failed approaches.

**What to look at, in about twenty seconds of play:**

1. Press play. The count should read **30 before you shoot anything** - not blank, and not whatever
   the asset was saved with.
2. Fire. It should tick **down one per arrow**.
3. Walk over one of the three bundles in the arena. It should jump **+15**.
4. Die and respawn a couple of times, then fire once. It should move **exactly once** - two writes
   per shot would mean the old pawn's binding leaked, which is the failure the unbind block exists
   to prevent.

## Refine

**Its position is a guess and should be moved by eye.** I placed it at `Left=60, Top=210` on the root
canvas, under the lives readout, at the same 24pt Bold as the other status text. That is a starting
point derived from where the neighbours sit, not a design judgement - this project's standing rule is
that framing and placement go to Michael with the numbers rather than being iterated on by an agent.

**One known limitation, recorded rather than fixed:** the count refreshes on inventory change, not on
weapon change. Equipping a *different weapon* whose `ArrowItemClass` differs would leave the number
stale until the next pickup or shot. Irrelevant today - the player has one bow and it is the only
thing that names an arrow item - and fixing it means binding to a weapon-changed delegate, which is
more surface than the case currently justifies.

**Deliberately not done:** no torch count, per ruling 47. The plan originally carried one and it went
when the scope narrowed to arrows.
