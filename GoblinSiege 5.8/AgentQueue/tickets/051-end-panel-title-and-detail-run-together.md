---
id: 051
title: End panel: title and detail run together on one line instead of stacking
agent: claude-hud
status: done
claimed: 2026-08-06T23:09Z
build: none
waiting_on:
evaluated: 2026-08-06T23:12:09Z
files: 
  - Content/UI/WBP_GSPlayerHUD.uasset
---

## Goal

End panel: title and detail run together on one line instead of stacking

## Generate

Michael's `end screen bugs.png`, taken from a live PIE loss. The panel WORKS - "OUT OF LIVES / The
warband is spent. 0 of 0 objective types burned." is on screen and the clock is frozen at 29:49
(= the 1788s halt from #050). It reads badly though: the two lines are cramped together at the same
type size and sit right of centre.

**Corrected my own first reading.** I called it "not stacked, running together on one line". Looking
properly, they ARE on separate lines - roughly 18px apart, both centred on the same x. The bug is
that they are the same size with no spacing, so they read as one block of text rather than a title
and a subtitle.

Changes, all to `WBP_GSPlayerHUD`, no rebuild:
- `EndTitleText` -> **Bold 44** with a 2px outline; `EndDetailText` -> **Regular 20**, 1px outline,
  warm off-white. Outlines because this panel sits over bright terrain, where plain white text on
  the village at midday is close to invisible.
- Both slots explicitly `H_ALIGN_FILL` + `Justification=Center`, and 6px/2px vertical padding.
- Panel 700x160 -> **900x200**, moved from y=-60 to y=-40, so 44pt has room.

**The sphere in the screenshot is not mine.** Michael identified it as a leftover artifact from the
climbing tests. Chased it for three tool calls first - ruled out the sword sweep debug (cyan/green/
magenta, 0.35s life), `GS.Aim.Debug` (0), and the aim decal - before he said. Recorded so the next
person seeing a red wireframe sphere on L_Tutorial_Island does not repeat that.

## Evaluate

**I could not verify this one by looking, and that matters given the last four tickets.**
`capture_preview` renders the HUD without the end panel, because `NativeConstruct` collapses
`EndPanel` and the preview constructs the widget. That is *good* evidence the C++ hide works - and it
means the only way to see this layout is to lose a raid.

**One change is probably a no-op and I am flagging it rather than claiming it.** `H_ALIGN_FILL` is
enum value **0** - the default. Setting it explicitly likely changed nothing, which means my original
diagnosis ("the alignment is wrong") was probably wrong too. The font sizes are the change that will
actually be visible. I did not record the slots' prior alignment before overwriting it, so I cannot
say for certain either way - which is the same mistake I avoided when reparenting the wheel and made
here.

**Verified:** `get_font` reads back Bold/44/outline 2 on the title off a fresh load. That is the
change landing, not the change being right.

**Not addressed:** the panel still sits over a running game - input live, pawn walking. #050 said end-
of-raid policy is a design decision and that is still true; this ticket only makes the message legible.

## Refine

- **Re-read the screenshot instead of trusting my summary of it.** I had written "not stacked"; they
  were stacked. Acting on that would have produced a fix for a bug that did not exist while leaving
  the real problem - no type hierarchy - untouched.
- **Changed font size rather than only alignment**, once I realised FILL was already the default.
  Alignment was my theory; type size is the thing a person actually sees.
- **Added outlines.** The whole reason the wheel and this panel are hard to read is white text over a
  sunlit village. It is two properties and it removes the need to guess at a backing panel.
- **Stopped chasing the sphere when told it was a climbing artifact.** Three calls in I had ruled out
  three sources and was about to keep going; the cheapest source of truth was the person who had seen
  it before.
- **Said plainly that the alignment change may have done nothing.** The alternative was listing four
  changes and letting the reader assume all four mattered.

**Deliberately left undone:** a backing panel behind the text (outlines may be enough - worth seeing
first); end-of-raid policy; and verifying this layout, which needs a real loss.
