---
id: 310
title: The generated HUD art gets real alpha and comes into the project
agent: claude-ui
status: done
claimed: 2026-08-25T23:43Z
build: none
waiting_on: Michael: 306 must close before the textures can be wired into WBP_GSPlayerHUD. Nothing is on screen yet.
evaluated: 2026-08-26T01:42:55Z
observed: 2026-08-26T01:42:54Z | The wooden HUD art drew in game at the right scale - bars, clock, objective board and prompts - with no transparent gaps in the metal
scenario: PIE raid on L_Tutorial_Island, played through with the HUD up
files: 
  - Content/UI/Textures
---

## Goal

The generated HUD art gets real alpha and comes into the project

## Generate

Michael generated the whole UI kit from the prompts in the spec - 18 PNGs, 16 named
assets, in Downloads. Every one came back **RGB with no alpha channel and a
transparency checkerboard painted into the picture**. The generator drew the
Photoshop checker instead of producing transparency. Seven had already been imported
in that state.

Built `dechecker.py` to key the checker out and give each one real alpha, then sliced
the multi-icon sheets into 34 separate assets, generated the 2 bar fills that were not
in the batch, and imported all 36 into `/Game/UI/Textures`.

## Evaluate

Four keying approaches, each failing differently, each measured rather than assumed:

1. **Colour range + border flood** - ate the pewter end caps. The checker's light band
   (145) is the same grey as the painted pewter, so no colour range separates them.
2. **Rigid checker grid model** - fit never rose above 0.55. The checkerboard is
   hand-painted, so it drifts; there is no grid to match.
3. **Outline-dammed flood** - correct silhouettes, but the flood tunnels through pewter
   wherever the dark outline has a gap, and the whole art blob goes transparent.
4. **Local bimodality (kept)** - every window of checker holds both band values and
   almost nothing else; painted metal is a continuous gradient. This separates them
   where colour cannot.

Three more measured defects on top of that:
- The analysis window straddling art and checker left a halo. Fixed by racing a
  checker front and a paint front through the ambiguous band - the dark outline seals
  the metal, so only the halo is reachable from the checker side.
- The baked drop shadow dims both bands by the same factor, so pooled shadow in
  concave corners read as paint. Fixed by testing bimodality at five brightness scales.
- Connected-component slicing merged touching tiles. Replaced with projection profiles.

Verified by compositing all 34 over magenta and reading them at 1:1, and by checking
the alpha map directly rather than trusting the import return.

## Refine

- Import settings copied off `T_GS_BowTimingBar` rather than guessed: TEXTUREGROUP_UI,
  TC_EDITOR_ICON, sRGB on, no mipmaps, never stream.
- Staged PNGs went to the scratchpad, not under `Content/` - loose PNGs there show up
  as unsupported files.
- Recess measurement for the bar fills was attempted three times and abandoned rather
  than iterated a fourth. It only matters for widget layout, which #306 holds.

## Not done, and why

- **Nothing is wired to `WBP_GSPlayerHUD`** - #306 holds that asset. The textures exist
  and are correct; no one has seen them on screen.
- Michael's 7 checkerboard imports (`Health_Bar`, `Stamina_Bar`, `Objective_Board`,
  `Objective_State_Markers`, `LifeMarkers`, `Aiming_Reticule`, `Arrow_Count`, plus their
  PNGs) are superseded by the `T_GS_*` set but left in place - not mine to delete.

## Wiring pass 1 (2026-08-26) - done under #306, which holds WBP_GSPlayerHUD

Wired and LOOKED AT via `WidgetService.capture_preview`, not just read back:

- `Reticle` -> T_GS_Reticle, `InteractRing` -> T_GS_ChannelRing.
- `HealthBar` / `StaminaBar`: frame art as a canvas Image at z=-1 behind, the ProgressBar inset
  into the frame's channel. The channel is opaque wood, so the frame CANNOT be drawn over the
  fill - the bar has to sit inside it. Channel measured off the art: health (179,67)-(786,167)
  of 962x259, stamina (51,53)-(902,132) of 954x183.
- Backing plates for the objective board and the raid clock.

Three things the preview caught that read-back never would have:

1. **Every plate was stretched off-aspect.** Sizes now follow each texture's ratio.
2. **The plates rendered permanently** while the text they back is Collapsed until something
   happens - the announcement banner would have sat on screen for the entire raid. Both prompt
   plates are Collapsed for now, which needs the C++ fix below to be right.
3. The list and the HP/STA text were drawing on the frame rails, not inside them.

### Known incomplete

- `AnnouncementPlate` and `InteractPromptPlate` are Collapsed and nothing shows them. They need
  `BindWidgetOptional UImage` members toggled alongside their text - a code change and a rebuild.
  Until then those two announcements draw with no plate, which is where they started.
- ARROWS and LIVES are still bare text; the quiver and skull art needs per-item widgets.
- The 8 objective/state icons need `RebuildObjectiveList` rebuilt into rows (separate ticket).
- 10 wheel textures belong to WBP_WeaponWheel / WBP_HordeOrderWheel (separate ticket).
- `EndPanel` art not wired.

