---
id: 148
title: "No crosshair: the player aims orders, the bow and the torch with nothing on screen to aim with"
agent: claude-crosshair
status: done
claimed: 2026-08-13T06:10Z
build: none
waiting_on:
evaluated: 2026-08-13T06:41:42Z
observed: UNOBSERVED 2026-08-13T06:41:42Z - The reticle is confirmed at dead screen centre by a widget preview capture, but nobody has seen it drawn over gameplay - size, tint and whether a 4096 filigree texture reads at 72px are all unjudged.
scenario: none - never run
files: 
  - Content/UI/WBP_GSPlayerHUD.uasset
  - Content/UI/M_UI_ReticleRune.uasset
---

## Goal

Michael, 2026-08-13: *"We need a basic crosshair for this game so people can tell where they're
aiming. it's hard to attempt to aim a command at something."*

Then, on being shown a plain four-tick plus: *"look up the crosshair example I have in the
screenshots"* - `What aiming the bow and torch should look like..png`, which is a medieval battle
scene with an ornate **glowing rune-circle** reticle, not an FPS crosshair.

The gap is real and this codebase already admitted it twice in its own comments:
`GSHordeCommandComponent.h:124` and `:157` both say *"there is no reticle on screen, so the player is
aiming by feel"* - written while explaining why the order trace needed a forgiving sweep radius.

## Generate

**Nothing new was drawn.** `T_RuneOfFate`
(`/Game/MagicRuneVFX/Essentials/Textures/Runes/T_RuneOfFate`) already ships with the project and
matches the reference closely: gold ornate ring, radiating flourishes, and an **open centre** so the
reticle does not hide the thing being aimed at. Chosen by exporting five rune candidates to PNG and
looking at them, not by picking the best-sounding asset name.

**`/Game/UI/M_UI_ReticleRune`** - a new material, and the reason one is needed at all:

`T_RuneOfFate` is a *VFX* texture. Niagara draws those **additively**, where black adds nothing and
reads as transparent. UMG does not work that way - an `Image` brush keys transparency off the texture's
**alpha channel**, which an additive VFX texture generally does not carry. Dropped in raw it would
have drawn a **black square with a gold ring inside it**.

So the material converts additive-intent into UI-correct translucency:

- `Material Domain = User Interface`, `Blend Mode = Translucent`
- `ReticleTexture` (TextureSampleParameter2D) RGB x `Tint` (VectorParameter, warm gold 1.0/0.92/0.65)
  -> Final Colour
- **Luminance of the same RGB** - dot product with (0.3, 0.59, 0.11) - x `ReticleOpacity`
  (ScalarParameter) -> Opacity. Bright ring becomes opaque, black background disappears.

Every knob is a named parameter, so tint and strength are tunable from a material instance without
touching the graph.

**`WBP_GSPlayerHUD`** - new `Reticle` Image on `RootCanvas`:

| | |
|---|---|
| Brush | `M_UI_ReticleRune`, drawn as Image, 72x72 |
| Anchors | `(0.5,0.5)-(0.5,0.5)`, alignment `(0.5,0.5)` - screen centre at any resolution |
| Visibility | `HitTestInvisible` - it must never eat a click |
| Z-order | `-1`, so it sits behind the meters and can never cover the end-of-raid panel |

Anchoring to centre rather than positioning in pixels is the same lesson as #146: the wheel labels
were at the canvas default anchor and ended up in a screen corner.

## Evaluate

**Verified by read-back and by picture:**

- Slot reads `anchors(0.5,0.5)-(0.5,0.5) offsets(l=0 t=0 r=72 b=72) align(0.5,0.5) z=-1`.
- Brush reads `resource_path: /Game/UI/M_UI_ReticleRune.M_UI_ReticleRune, draw_as: Image,
  image_size: 72x72`.
- `WidgetService.capture_preview` at 1280x720 shows the rune **at dead screen centre**, with the
  existing HUD text undisturbed around it.

**NOT verified, and it is the part that matters: whether it READS.** The preview renders on a white
background, so nothing can be judged about contrast, colour or legibility from it. Two specific
doubts:

1. **`T_RuneOfFate` is 4096x4096 with very fine filigree, drawn at 72px.** That is a ~57x downscale.
   The fine flourishes will almost certainly disappear into the mip and may leave a fuzzy ring rather
   than a crisp one. If it looks like a smudge, the fix is a bigger draw size (96-128) or a simpler
   rune - `T_MagicRune` and `T_HolyRune` were the other exported candidates.
2. **The luminance-as-opacity trick is unproven on this specific texture.** It is the right approach
   for an additive source, but if `T_RuneOfFate` *does* carry a usable alpha channel, luminance may be
   double-darkening the edges. Nobody has seen it drawn over gameplay.

**Deliberately not done:** the texture is left in `TEXTUREGROUP_WORLD` at 4096. Re-grouping a shared
marketplace VFX asset to UI would change it for every Niagara system that uses it, which is a bigger
decision than a crosshair.

**Owed to AGENT_STATE:** the project has a reticle as of #148, built from an existing rune texture
through a UI-domain material; and the general note that VFX textures need luminance-as-opacity to be
usable in UMG at all.

## Refine

**Changed on self-review: the whole design.** The first plan was four white tick marks around a centre
gap - a competent FPS crosshair and completely wrong for this game. Michael's screenshot reference
redirected it to something diegetic and medieval. Worth recording because the first plan would have
shipped and looked fine in isolation; it was only wrong against the game's own art direction.

**Also changed:** the initial attempt tried to build a `SlateBrush` and hand it to `set_brush`, which
takes a `WidgetBrushInfo` instead - and `image_size` on a raw `SlateBrush` now wants
`DeprecateSlateVector2D`, not `Vector2D`. Reading the struct back with `get_brush` first, then
mutating and re-setting it, avoids guessing at either.

**Deliberately left undone:**

- **Any judgement on size, tint or legibility.** 72px and warm gold are opening guesses. All three are
  named parameters or one-field edits.
- **Contextual behaviour.** The reticle is always on. It may want to fade out when no weapon is
  readied, or grow/tighten while aiming the bow - but "basic crosshair" was the ask, and behaviour
  should follow seeing the static one first.
- **Whether it should highlight a valid order target.** The order trace already knows when it has a
  subject (`GetLatchedSubject`), so the reticle could turn gold on a guard and grey on bare ground -
  which would answer the exact complaint that started this. Deliberately not built yet: it needs the
  static version judged first.
