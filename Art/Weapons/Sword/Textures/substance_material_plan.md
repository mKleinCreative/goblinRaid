# SM_Sword_Arming01 — Substance Designer material plan

Goal: one texture set (basecolor, normal, roughness, metallic, height, ambient
occlusion) covering the whole sword — steel for the blade + fittings regions,
dark leather for the grip — using the mesh bakes already generated in
`Textures/Bakes/`.

## Inputs already on disk (Textures/Bakes/)

- `SM_Sword_Arming01_ambient-occlusion-from-mesh.png`
- `SM_Sword_Arming01_curvature-from-mesh-v2.png` (use v2 — v1 blew out to
  black on this mesh's thin blade edges, don't use it)
- `SM_Sword_Arming01_normal-from-mesh.png`
- `SM_Sword_Arming01_position-from-mesh.png`
- `SM_Sword_Arming01_thickness-from-mesh.png`
- `SM_Sword_Arming01_color-from-mesh.png` — **material-ID mask**: each of the
  3 material slots (M_Sword_Blade, M_Sword_Fittings, M_Sword_Grip) bakes as a
  flat, distinct solid color. This is what you'll use to mask steel vs
  leather regions.

Known limitation: the UV islands are non-overlapping and fill the tile's
bounding box, but individual pieces (esp. the 12 guard segments) are still
small relative to the canvas — fine for these mesh maps, but hand-painted
detail will be low-res there without a tighter re-pack later.

## 1. New graph

File > New > from template > **PBR Metallic Roughness**
(`02_pbr_metallic_roughness.sbs` under Designer's own templates — this is
what `New Graph` gives you by default). Confirmed output identifiers on this
template: `basecolor`, `normal`, `roughness`, `metallic`, `height`,
`ambientocclusion`.

Import the 6 bakes above as bitmap resources (right-click Resources panel >
Import > pick all 6 PNGs), then drag each in as a **Bitmap** node.

## 2. Steel (blade + fittings)

Library search "Metal" under PBR Materials. I confirmed `metal_002` is a
full standalone material (outputs basecolor/normal/roughness/metallic/
height/ambient_occlusion) but it's a **rusted/battle-damaged plate with
bullet holes** — not what we want. Before committing, preview `metal_002`,
`metal_003`, `metal_006`, and `metal_plate_001`/`metal_plate_004` side by
side and pick the cleanest/most polished-looking one.

Whichever you pick, instance it and zero out damage-style params so it reads
as clean knight-grade steel — on `metal_002` specifically these are exposed:
`scratches`, `deep_rust`, `rust`, `rust_drips`, `bullets_impacts_amount`,
`bullets_impacts_size` (drop these to 0 or near-0). Use the simple tweak
params to match the color already validated in the Blender preview render:
- Blade: light steel gray, RGB ≈ (0.80, 0.81, 0.83), roughness ≈ 0.12–0.22
- Fittings (guard/pommel): slightly duller gunmetal, RGB ≈ (0.68, 0.69, 0.71)

`luminosity` / `contrast` / `hue_shift` / `saturation` on the instance get
you there without touching the graph internals.

Feed our baked `curvature-from-mesh-v2` and `ambient-occlusion-from-mesh`
maps into the material's grunge/wear inputs where it has them exposed — that
drives realistic edge-highlight/cavity-dirt using the actual mesh geometry
instead of the material's own generic procedural wear.

## 3. Leather (grip)

There's no standalone leather base material bundled — `leather_weathering`
(category "Mesh Adaptive") is a **weathering filter**, not a base: it takes
basecolor/normal/roughness/metallic/height/ambient_occlusion as *input* and
adds dust/dirt/edge-wear/cracks/age on top, exposing `Dust`, `Dirtiness`,
`Edges_Wearing`, `Used`, `Age`, `Cracks_Scale`, `Sharp_Edges_Scratches_Scale`,
`Used_Leather_Desaturation`, `Used_Leather_Brightness`.

Given the grip is meant to read as simple, near-flat dark leather (matches
the Blender preview: RGB ≈ (0.02, 0.02, 0.02), roughness ≈ 0.55,
non-metallic), the low-risk path: build a flat base yourself (Uniform Color
node for basecolor/roughness/metallic constants, a basic Leather pattern
generator from the Library search "leather" for subtle normal-map grain),
then run it through `leather_weathering` feeding our baked `thickness`,
`curvature-from-mesh-v2`, and `position` maps into its mesh-driven inputs so
wear follows the actual grip geometry.

## 4. Combine via the material-ID mask

`color-from-mesh.png` has 3 flat solid colors, one per material slot. Use a
**Color to Mask** / RGB comparison (e.g. a Gradient Map or a couple of
Multi-Switch / distance-to-color nodes) to derive a grayscale mask isolating
the grip region, then **Blend** (or Multi-Material Blend) the steel result
and the leather result using that mask, per channel (basecolor, normal,
roughness, metallic, height, ao).

## 5. Output

Bridge the final blended channels to the graph's 6 `graphoutput` nodes
(`basecolor`, `normal`, `roughness`, `metallic`, `height`, `ambientocclusion`
— identifiers must match exactly, that's what the FBX/Unreal side expects).
Set the graph as root, cook (Ctrl+B or it auto-cooks), then File > Export
Outputs to PNG (2048 to match the bakes) into `Textures/` alongside the
bakes.

Those exported PNGs are what get assigned to the sword's material in
Unreal once a character/socket setup is ready.
