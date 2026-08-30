---
id: 363
title: Wheat burn char: fix disconnected CharMask, add height-based chaff/stalk mask
agent: claude-fire
status: done
claimed: 2026-08-30T03:17Z
build: none
waiting_on:
evaluated: 2026-08-30T05:18:04Z
observed: 2026-08-30T05:18:05Z | Wheat visibly darkens toward black on burnt cells with correct before/after screenshots; Michael called it good enough for now, other work took priority
scenario: Live PIE, L_Tutorial_Island, multiple burn tests with screenshot comparisons
files: 
  - Content/VFX/Burn/M_GS_Crop_Master.uasset
  - Content/VFX/Burn/MF_GS_BurnChar.uasset
---

## Goal

Wheat burn char: fix disconnected CharMask, add height-based chaff/stalk mask

## Generate

Michael: "we also need to darken the burn wheat again, not sure if that's catching a burnt state,"
then "maybe make the chaff invisible and the stalks darkened if not black."

**Root cause, found by tracing the material graph node-by-node (M_GS_Crop_Master -> MF_GS_BurnChar):**
the char alpha is computed as `Saturate(Max(GS_BurnAmount_floor=0.0, BurnAmountIn) * CharMask *
GS_CharStrength)`. `BurnAmountIn` is correctly fed by `GS_BurnMask`'s R channel (the world burn mask
- that pipeline is sound, RegisterCropMaterials binds it, confirmed by the recurring "World burn mask
bound 3 material slot(s)" log line). But `CharMask` - the multiplicative term - was **never connected
at the call site**, defaulting to 0. Multiply-by-a-disconnected-zero collapses the whole alpha to zero
regardless of burn amount. Wheat was never going to visibly char, at any burn level, full stop - not
"too subtle to notice."

**Fix, in `M_GS_Crop_Master` only (MF_GS_BurnChar itself untouched):**
- New node chain: `WorldPosition(Absolute) - ObjectPositionWS` -> Z component -> divided by
  `ObjectBounds(local).Z * GS_ChaffHeightFraction` (new scalar param, default 0.55) -> `Saturate` ->
  `HeightFraction01` (0 at the plant's own base, 1 at/above 55% of ITS OWN height - not a hardcoded
  cm value, so any future burnable plant mesh works with no per-asset tuning, per Michael's "long term
  solution for out of the box burning on plants" question).
- `StalkFactor = OneMinus(HeightFraction01)` -> connected to the previously-dangling `CharMask` input.
  Base/stalk region now correctly darkens toward `GS_CharColor` (already authored near-black:
  RGB 0.019/0.015/0.012 - no color change needed, only the missing connection was the bug). Chaff
  region (top) gets CharMask~0, so it skips BaseColor darkening - moot anyway, see next point.
- Opacity fade for "chaff invisible": `FadeAmount = Saturate(HeightFraction01 * BurnAmount[GS_BurnMask.R])`
  -> `KeepFactor = OneMinus(FadeAmount)` -> multiplied against the material's EXISTING
  `MP_OPACITY_MASK` source (previously a direct `Reroute_3` connection, now routed through this new
  multiply) and reconnected to `MP_OPACITY_MASK`. At burn 0, KeepFactor is 1 everywhere (opacity
  unaffected); as a cell's burn amount rises, the plant's own top fraction fades toward fully
  transparent while the base stays opaque and darkens.
- Compiled clean (`recompile_material` returned zero errors) and saved. Content-only change - no
  C++ touched, no engine rebuild needed, takes effect immediately in the already-running editor/PIE.

## Evaluate

**Not yet visually verified - compile success is not evidence, per this project's own standard.**
The graph wiring is sound by construction (traced every input by name, not guessed) and compiles
without error, but nobody has watched a wheat cell actually burn since this landed. Two specific
things need a live look: (1) does `GS_ChaffHeightFraction=0.55` read as a sensible split on the
actual wheat card geometry, or does it need retuning - this is exactly the kind of number that
should be judged by eye in PIE, not guessed twice; (2) does `ObjectBounds` return sane values for
HISM/foliage instances specifically (it is well-defined for ordinary static mesh components; foliage
instancing is the one case worth double-checking since the whole "out of the box for any future
plant" promise depends on it behaving the same way there).

**Scope check:** stayed inside `M_GS_Crop_Master` only, per the claimed file. Did not touch
`MF_GS_BurnChar` (the shared function - other burnables may use it, so its own graph and
`GS_CharColor` were left alone) or the mesh assets' own default materials (still `MI_VillageWheat`,
not the burn-aware `MI_GS_VillageWheat` - flagged, not fixed, in the earlier investigation: a future
field placed without the level-side material override would silently get no char at all).

## Update - watched, iterated live, closing as "good enough for now" (Michael's call)

**Verified live, multiple rounds, real before/after screenshots.** The disconnected-CharMask fix
works - burnt wheat visibly darkens now, confirmed by direct before/after comparison (bright gold
vs. dark olive-brown swath, screenshots `NewAfterBurn.png` and others). Two real bugs found and
fixed during that live iteration, beyond the original disconnected-input fix:

- **First height-mask attempt used `ObjectPositionWS` ("Object Position"), which is NOT per-instance
  for HISM/foliage** - it returns the owning actor's single shared pivot, not each individual
  plant's own base. This made "local height" reflect the plant's position across the WHOLE FIELD
  rather than height above its own root, so once the fade multiplier was boosted, nearly everything
  read as "top/chaff" and the field went to bare dirt. Fixed by switching to
  `TransformPosition(World -> TRANSFORMPOSSOURCE_INSTANCE)`, which IS per-instance-correct.
- **Cranking `GS_CharStrength` to 15 (chasing "not black enough") turned the burn mask's existing
  bilinear-smoothed gradient into a hard per-texel step**, reading as a visible checkerboard/grid
  (the mask is coarse - a few uu per texel on this map). Dialed back to 5.0, which keeps burnt areas
  solidly dark while letting the existing smoothing blend texel edges again.

**Final tuned values** (all on `MF_GS_BurnChar` unless noted): `GS_CharColor` = neutral cool
near-black (0.006, 0.006, 0.007) - deliberately NOT warm/brown, per Michael's explicit correction
("shouldn't be brown or olive... should feel black or burnt". `GS_CharStrength` = 5.0. On
`M_GS_Crop_Master`: `GS_ChaffHeightFraction` = 0.35 (top 65% of each plant's own height fades),
`GS_ChaffFadeBoost` = 7.0, `GS_CharMaskBoost` = 3.0.

**Process note worth keeping:** `unreal.EditorAssetLibrary.save_loaded_asset`/`save_asset` returned
`False` on the majority of calls this session even when the save genuinely succeeded (confirmed via
the engine log's `Saving Package`/`Moving output files` lines and the `.uasset` file's own
`LastWriteTime`) - and on a few calls, returned `False` and genuinely DID skip the save (no such log
lines at all), requiring an explicit `asset.modify(True)` before saving to reliably persist. Every
single edit this ticket made was verified against the file's on-disk timestamp, never trusted from
the Python return value alone. Written up as a gotcha in `GoblinSiege 5.8/CLAUDE.md`.

**Not fully polished, and that's fine - Michael's call, other work is higher priority right now:**
the exact darkness/fade balance is tuned by eye against a handful of screenshots, not exhaustively
verified across every lighting condition or viewing distance. `MI_VillageWheat` (the mesh assets'
own default material, still not burn-aware) remains a known gap - flagged, not fixed, same as
before. Closing `done` on Michael's explicit "call it good for now," not on exhaustive verification.

## Refine

Superseded by the update above - closing `done`. The original discipline ("needs a torched wheat
cell, watched, before done") was honored: this got watched, repeatedly, with real screenshots, and
two real bugs were caught and fixed BECAUSE it was watched rather than assumed. What's left
undone (further polish, `MI_VillageWheat` default material) is deliberately deprioritized, not
silently dropped.
