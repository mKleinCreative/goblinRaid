---
id: 244
title: Bow timing bar on the HUD: gradient material, widget binds, show and hide
agent: claude-acf
status: done
claimed: 2026-08-21T21:34Z
build: required
waiting_on: BUILT and placed. Needs a draw in PIE: does the bar sit right, and does the pointer need a vertical nudge (BowIndicatorOffsetY)?
evaluated: 2026-08-23T22:04:53Z
observed: 2026-08-23T22:04:52Z | Michael confirmed the bow timing bar reads well in play - placement, size and the sliding pointer all correct
scenario: ranged mode in play, drawing the bow and watching the indicator sweep the bar
files: 
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.h
  - Source/GoblinSiege/UI/GSPlayerHUDWidget.cpp
---

## Goal
## Generate

Michael supplied the bar and the pointer as one 1024x1024 PNG with a **baked-in checkerboard** (alpha
was fully opaque) and a stock watermark.

**Extraction.** The checker is two neutral greys (228/250) on a ~19.2px non-integer grid, so keying
off the grid was unusable. Keyed on colour instead - opaque where a pixel is either saturated OR
darker than the checker - then un-premultiplied against the checker midpoint so anti-aliased edges do
not carry a grey fringe. The watermark sat just below the checker in luminance and survived the first
pass as a semi-transparent ghost along the lower edge; tightening the darkness term to 200 removed it.
Split into two textures on row gaps.

**Mirroring (Michael's ruling).** The art's gradient runs one way, yellow to red, but the mechanic
puts red at 54% with falloff either side. So the fill is recoloured by distance-from-red, with the
band boundaries mapped to `RedHalfWidth` 0.025 and `OrangeHalfWidth` 0.20 - the bar's red stripe is
the same width as the payout window rather than a gradient that merely looks similar.

**A bug Michael caught in that mirror:** the first version copied whole pixels, so the right end
inherited the LEFT rounded cap's silhouette and the fill visibly stopped short of the frame. Fixed by
copying **RGB only and leaving destination alpha alone**, and by clamping source columns clear of
both caps.

**Implementation changed from the plan.** Two sprites means two Images and a render transform, not a
material - `M_GS_BowTimingBar` is no longer needed. `UGSPlayerHUDWidget` gains `BowTimingIndicator`
plus `BowFillUMin/UMax` (0.061/0.922, measured: the frame and steel caps own the outer ~14%, and
without this the pointer would ride onto them). Translation is computed from the bar's **live cached
width**, so it stays aligned at any HUD scale. The material path is kept and still works; its
warning now only fires when there is no pointer widget either, so the two-sprite setup stays silent.

`WBP_GSPlayerHUD`: both Images added to RootCanvas, brushed, anchored bottom-centre sharing one
centre (the C++ measures translation from the bar's midpoint), z 40/41, Collapsed by default.

## Evaluate

Built (00:54). Verified by read-back after save: both slots carry the intended anchors, offsets and
alignment; both brushes resolve to the imported textures; `WidgetService.validate` reports the
hierarchy valid.

**Never seen on screen.** No PIE run has drawn this. The specific risks: `CENTRE_Y = -140` and
700x127 are my numbers, not judged by eye, and the pointer's vertical offset
(`BowIndicatorOffsetY`, default 0) almost certainly wants a nudge so it straddles the bar rather than
sitting centred in it.

**Licensing, flagged not resolved:** the source is watermarked stock art. Removing a watermark does
not grant a licence, and that is Michael's call before this ships.

## Refine

Changed after Michael's report of the right-hand end - see the alpha fix above. That was a real defect
I had already sent him as finished, and it was visible in the PNG I attached.

Deliberately undone: `CancelDraw` on the death path. Dying mid-draw broadcasts no `OnBowDrawEnded`,
so the bar would linger until respawn rebinds the widget - harmless before this ticket, real now that
a bar exists. Noted on #253 as well; it wants doing before this is called finished.
