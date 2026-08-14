---
id: 138
title: Import the ArtSource weapons: six meshes into /Game/Weapons, with material instances where bakes exist
agent: claude-weapons
status: done
claimed: 2026-08-12T05:13Z
build: none
waiting_on:
evaluated: 2026-08-12T05:19:41Z
observed: 2026-08-12T05:19:41Z | rendered the hunting horn and saw a tan horn body with steel bands, chain and dark mouthpiece rather than the flat white that a zero map weight produces; a viewport capture showed all five other weapons present and shaded in the level
scenario: editor render of SM_HuntingHorn_Signal01, plus the six meshes spawned in a row in L_CombatArena and captured, then removed
files: 
  - Content/Weapons
---

## Goal

Import the ArtSource weapons: six meshes into /Game/Weapons, with material instances where bakes exist

Michael, 2026-08-11: *"grab stuff from the ArtSource/Weapons/ folder"*, after choosing
meshes-plus-materials and the `/Game/Weapons/<Name>` + `SM_` naming.

## Generate

### Six meshes, measured on arrival

| asset | dest | tris | size (full) | slots |
|---|---|---|---|---|
| `SM_Sword_Arming01` | `/Game/Weapons/Sword` | 282 | 88cm | 3 |
| `SM_Crossbow_Militia01` | `/Game/Weapons/Crossbow` | 1716 | 64 x 75cm | 3 |
| `SM_Crossbow_Bolt01` | `/Game/Weapons/Crossbow` | 216 | 40cm | 3 |
| `SM_Lance_War01` | `/Game/Weapons/Lance` | 420 | 2.7m | 3 |
| `SM_HuntingHorn_Signal01` | `/Game/Weapons/HuntingHorn` | 7772 | 49cm | 4 |
| `SM_Sword_Medieval01` | `/Game/Weapons/SwordMedieval` | 4317 | ~1m | 1 |

Dimensions were checked rather than assumed - a 2.7m lance and a 40cm bolt are right; a weapon
imported at the wrong unit scale would show here and nowhere else until someone put it in a hand.

Imported with `combine_meshes`, lightmap UVs and auto collision. The five Blender-authored FBXs
carried their own named material slots (`M_Sword_Blade`, `M_Crossbow_String`, ...), so nothing came
in on the default grey.

### The `*MapWeight` trap, avoided in one place and paid in another

Every imported material is a `MaterialInstanceConstant` parented to
**`FBXLegacyPhongSurfaceMaterial`** - the exact parent `CLAUDE.md` warns about, where assigning a
texture does nothing on its own because each map is blended through a `*MapWeight` scalar that
**defaults to 0** (that cost most of #099; the texture, UVs and mesh were all fine the whole time).

- **The five Blender weapons do not hit it**: they carry flat `DiffuseColor` values and no maps at
  all, so there is no weight to be zero. The blade reads `0.80/0.81/0.83` - the exact steel value the
  Substance plan records as validated in the Blender preview.
- **The medieval sword does not hit it either**: it arrived with `DiffuseColorMap` set *and*
  `DiffuseColorMapWeight` already at 1.0.
- **The horn does hit it**, because it is the one asset where I assigned textures. `DiffuseColorMap`
  and `NormalMap` set on all four slots, and `DiffuseColorMapWeight`/`NormalMapWeight` set to 1.0
  alongside each. Read back both the texture *and* its weight, because the texture alone proves
  nothing.

### Textures: only the horn's, and deliberately not the sword's

`T_HuntingHorn_BaseColor/Normal/Roughness/AO` imported with correct settings - normal map flagged
`TC_NORMALMAP`, roughness and AO `TC_MASKS` with sRGB off, since they are linear data and reading
them as colour is wrong.

**The sword's six bakes were deliberately NOT imported.** `substance_material_plan.md` states they
are Substance Designer *inputs* - curvature, position, thickness, AO-from-mesh, and a material-ID
mask - and that the finished PNGs "are what get assigned to the sword's material in Unreal" *once
authored*. They do not exist yet. Wiring a position or curvature bake in as basecolor would have
produced something spectacularly wrong that still passed every read-back check.

## Evaluate

**The horn was looked at.** A rendered capture shows the tan horn body, steel bands, chain and dark
mouthpiece - textured, correctly lit, and not the flat white that a set-texture-with-zero-weight
produces. That is the one asset whose materials I changed, so it is the one that most needed an eye.

**The other five were verified by measurement plus a viewport capture confirming they are in-world
and rendering, but NOT by a clean individual product shot.** Their materials are exactly as imported
- I did not modify them - so the risk is import-side (scale, UVs, slots), which the table above
covers. Stated plainly rather than implied: I have not looked closely at the crossbow's or the
lance's shading.

**Verified:** tri counts, bounds, slot counts and material parents read off each asset; horn texture
parameters *and* their weights read back after writing; 28 `.uasset` files present on disk.

**Not verified:** nothing has been held by a character, attached to a socket, or seen in gameplay.
No weapon data asset references any of this, so **nothing in the game uses these yet** - they are
content on the shelf.

**Owed to `AGENT_STATE.md`:** a DECISION line that project-authored weapon art now lands in
`/Game/Weapons/<Name>` with `SM_`/`M_`/`T_` prefixes, distinct from the third-party landing zone at
`/Game/_Import/Weapons/` (`GS_*`) that the existing weapon data assets point at.

## Refine

**Changed in response to my own review:** I was going to wire the sword's bakes alongside the horn's,
on the strength of the folder being called `Textures/Bakes` in both cases. Reading
`substance_material_plan.md` stopped that - the horn's are finished PBR outputs and the sword's are
authoring inputs, and the two are indistinguishable from the directory listing alone.

**Deliberately left undone:**

- **No hand socket, no attachment, no `UGSWeaponDataAsset`.** That was the third option offered and
  not the one chosen; it touches combat data and deserves its own verification pass.
- **`ArtSource/Weapons/unreal_import_and_socket.py` was read but not run, and not deleted.** It
  hardcodes `C:\Users\rando\...` (another machine) and relies on `EditorAssetLibrary`, which this
  build's own gotcha list records as a *silent no-op* - its `does_asset_exist` skip check would never
  fire. I took its naming convention and ignored its code. Left in place because it is not mine.
- **The three untextured weapons stay flat-coloured.** That is what the source provides; inventing
  materials for them would be authoring art, not importing it.
